#!/bin/bash
set -e
if [[ $(sysctl vm.max_map_count | cut -d'=' -f2 | tr -d ' ') -lt 1000000 ]]; then
    echo "vm.max_map_count too low! Must be at least 1M"
    exit 1
fi
trap "kill -- -$$" SIGINT

: ${BENCH_LOG_FILE:=$HOME/tmp/log}
logfile=$BENCH_LOG_FILE
echo "********** Begin script: $(realpath $0) **********" >> $logfile

MAXTIME="5m"
CORES="1 2 4 8"
BASE_ITER=1
RECORD_ITER=1
REPLAY_ITER=1

bench=(fib cbt cilkfor)
source config.sh

errcheck () {
    errcode=$1
    logname=$2

    if [[ $errcode -eq 0 ]]; then
        tmp=$(grep "time" $logname | tr "=" ":" | cut -d':' -f 2 | cut -d' ' -f 2 | tr -d ' ')
        printf "%0.2f" "$tmp"
    else
        printf "\n[Error]:\n"
        if [[ $errcode -eq 124 ]]; then
            printf "Timed out after %s.\n" $MAXTIME
            exit 1
        fi
        cat log
        exit 1
    fi
}

runcmd() {
    P=$1
    mode=$2
    name=$3
    args=$4
    oldargs=$4
    if [[ "$name" = "run.sh lock" ]]; then
        args="$4 $P"
    fi

    cmd="CILKRR_MODE=$mode ./$name lock $args out $P"
    echo "$cmd" >> $logfile
    CILK_NWORKERS=$P CILKRR_MODE=$mode ./$name $args &> log
    echo "------------------------------------------------------------" >> $logfile
}

runreplay() {
    name=$1
    args=$2
    declare -A vals
    
    for P in $CORES; do
        vals=()
        for i in $(seq $REPLAY_ITER); do
            runcmd "$P" "replay" "$name" "$args"
            val=$(errcheck $? "log")
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
    name=$1
    cmdname=$2
    args=$3
    
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
            runcmd "$P" "none" "$cmdname" "$args"
            val=$(errcheck $? "log")
            if [[ $? -ne 0 ]]; then
                printf "Error\n"
                exit 1
            fi
            vals[$i]=$val
        done
        avg=$( echo ${vals[@]} | tr " " "\n" | datamash mean 1 | tr -s " ")
        stdev=$( echo ${vals[@]} | tr " " "\n" | datamash sstdev 1 | tr -s " ")
        printf "\t%.2f(%.2f)" "$avg" "$stdev"


        rm -f .recordtimes
        vals=()
        for i in $(seq $RECORD_ITER); do
            runcmd "$P" "record" "$cmdname" "$args"
            val=$(errcheck $? "log")
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
        runreplay "$name" "$args"
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
