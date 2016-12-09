#!/bin/bash
set -e
trap "kill -- -$$" SIGINT

if [[ $(sysctl vm.max_map_count | cut -d'=' -f2 | tr -d ' ') -lt 1000000 ]]; then
    echo "vm.max_map_count too low! Must be at least 1M"
    exit 1
fi
type datamash &>/dev/null || { 
    echo >&2 "This script requries datamash. Aborting. "
    echo >&2 "On Ubuntu 14+, this can be installed with apt-get."; 
    exit 1; 
}


scriptdir=$(pwd)
: ${BENCH_LOG_FILE:=${scriptdir}/bench.log}
logfile=$BENCH_LOG_FILE
tmplog=${scriptdir}/.log
rm -f $logfile $tmplog

echo "********** Begin script: $(realpath $0) **********" >> $logfile

MAXTIME="20m"
CORES="1 2 4 8 12 16"
BASE_ITER=3
RECORD_ITER=3
REPLAY_ITER=3

bench=(matching refine)
source config.sh

function errcheck () {
    errcode=$1
    logname=$2

    if [[ $errcode -eq 0 ]]; then
        tmp=$(grep "time" $logname | tr "=" ":" | tail -1 | cut -d':' -f 2 | cut -d' ' -f 2 | tr -d ' ')
        printf "%0.2f" "$tmp"
    else
        printf "\n[Error]:\n"
        if [[ $errcode -eq 124 ]]; then
            printf "Timed out after %s.\n" $MAXTIME
            exit 1
        fi
        cat $logname
        exit 1
    fi
}

function runcmd() {
    local P=$1
    local mode=$2
    local name=$3
    local args=$4
    if [[ "$name" = "run.sh lock"* ]]; then
        args="$4 log $P"
    fi

    cmd="CILK_NWORKERS=$P PORR_MODE=$mode ./$name $args"
    echo "$cmd" >> $logfile
    CILK_NWORKERS=$P PORR_MODE=$mode ./$name $args 2>&1 | tee -a $logfile &> $tmplog
    echo "------------------------------------------------------------" >> $logfile
}

runreplay() {
    local name=$1
    local args=$2
    declare -A vals
    
    for P in $CORES; do
        vals=()
        for i in $(seq $REPLAY_ITER); do
            runcmd "$P" "replay" "$name" "$args"
            val=$(errcheck $? $tmplog)
            if [[ $? -ne 0 ]]; then
                printf "Error\n"
                exit 1
            fi
            vals[$i]=$val
        done
        avg=$( echo ${vals[@]} | tr " " "\n" | datamash mean 1 | tr -s " ")
        stdev=$( echo ${vals[@]} | tr " " "\n" | datamash sstdev 1 | tr -s " ")
        printf "\t%.2f(%.2f)" "$avg" "$stdev"
    done

}


runall () {
    local name=$1
    local cmdname=$2
    local basecmd="${cmdname}_base"
    local args=$3

    printf -- "--- $name $args ---\n"
    header_left="P\tbase\t\trecord"
    printf "${header_left}\t\t\treplayP\n"
    printf "%${#header_left}s\t\t\t\t\t\t\t\t\t" | tr " " "=" | tr "\t" "====="
    for P in $CORES; do printf "\t\t$P"; done;
    printf "\n"

    declare -A vals

    for P in $CORES; do
        printf "$P"
        vals=()
        for i in $(seq $BASE_ITER); do
            runcmd "$P" "none" "$basecmd" "$args"
            val=$(errcheck $? $tmplog)
            if [[ $? -ne 0 ]]; then
                printf "Error\n"
                exit 1
            fi
            vals[$i]=$val
        done
        avg=$( echo ${vals[@]} | tr " " "\n" | datamash mean 1 | tr -s " ")
        stdev=$( echo ${vals[@]} | tr " " "\n" | datamash sstdev 1 | tr -s " ")
        printf "\t%.2f(%.2f)" "$avg" "$stdev"
        base=$avg


        rm -f .recordtimes
        vals=()
        for i in $(seq $RECORD_ITER); do
            runcmd "$P" "record" "$cmdname" "$args"
            val=$(errcheck $? $tmplog)
            if [[ $? -ne 0 ]]; then
                printf "Error\n"
                exit 1
            fi
            vals[$i]=$val
            printf "%.2f\t%d\n" "$val" "$i" >> .recordtimes
            mv .cilkrecord .cilkrecord.$i
        done
        avg=$( echo ${vals[@]} | tr " " "\n" | datamash mean 1 | tr -s " ")
        stdev=$( echo ${vals[@]} | tr " " "\n" | datamash sstdev 1 | tr -s " ")
        printf "\t%.2f(%.2f)" "$avg" "$stdev"

        median=$(cat .recordtimes | sort | cut -f 2 | datamash median 1)
        mv .cilkrecord.$median .cilkrecord
        runreplay "$cmdname" "$args"
        mv .cilkrecord .cilkrecord-p$P
        printf "\n"
    done
    printf "\n-------------------------"
    set -e

}

if [ $# -gt 0 ]; then bench=($@); fi
for b in "${bench[@]}"; do
    cd ${dirs[$b]}
    (${makecmds[$b]} 2>&1) > compile.log
    if [[ "$?" -ne 0 ]]; then
        echo "Compile error!"
        cat compile.log
    fi
    runall "$b" "${cmdnames[$b]}" "${args[$b]}"
    cd - >/dev/null
done

echo "********** End script: $(realpath $0) **********" >> $logfile
