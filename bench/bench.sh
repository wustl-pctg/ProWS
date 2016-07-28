#!/bin/bash
set -e
ulimit -v 6291456
# Don't forget that we might need to set vm.max_map_limit
trap "kill -- -$$" SIGINT

MAXTIME="5m"
CORES="1 2 4 6 8"
BASE_ITER=3
RECORD_ITER=5
REPLAY_ITER=10

bench=(dedup ferret) #dedup ferret kdtree
declare -A dirs args

dirs["dedup"]=dedup
args["dedup"]="medium"
dirs["ferret"]=ferret
args["ferret"]="medium"

errcheck () {
    errcode=$1
    logname=$2

    if [[ $errcode -eq 0 ]]; then
        printf "%0.2f" "$(grep 'Processing time' log | tail -1 | cut -d' ' -f 4 | tr -d ' ')"
    else
        printf "\n[Error]:\n"
        if [[ $errcode -eq 124 ]]; then
            printf "Timed out after %s.\n" $MAXTIME
            exit 1
        fi
        cat log
        exit 1
    fi
    # No error, so remove the log file
    rm out
}

runcmd() {
    P=$1
    mode=$2
    name=$3
    args=$4
    CILKRR_MODE=$mode ./$name lock $args out $P &> log
}

runreplay() {
    name=$1
    args=$2
    declare -A vals
    
    for P in $CORES; do
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
    name=$1 # actually, we don't need
    name="run.sh"
    args=$2
    
    printf -- "--- $name $args ---\n"
    header_left="P\tbase\t\trecord"
    printf "${header_left}\t\t\treplayP\n"
    printf "%${#header_left}s\t\t\t\t\t\t\t\t\t" | tr " " "=" | tr "\t" "====="
    for P in $CORES; do printf "\t\t$P"; done;
    printf "\n"

    declare -A vals

    for P in $CORES; do
        printf "$P"
        for i in $(seq $BASE_ITER); do
            runcmd "$P" "none" "run.sh" "$args"
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
        for i in $(seq $RECORD_ITER); do
            runcmd "$P" "record" "$name" "$args"
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
        printf "\n"
    done
    printf "\n-------------------------"
    set -e

}

if [ $# -gt 0 ]; then bench=($@); fi
for b in "${bench[@]}"; do
    cd ${dirs[$b]}
    make
    runall "$b" "${args[$b]}"
    cd -
done
