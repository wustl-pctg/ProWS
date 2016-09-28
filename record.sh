#!/bin/bash
set -e
# Don't forget that we might need to set vm.max_map_limit
trap "kill -- -$$" SIGINT

bench=(cbt)
NITER=3

args=("5" "10" "15" "20")

errcheck () {
    errcode=$1
    logname=$2

    if [[ $errcode -eq 0 ]]; then
        printf "%d" "$(grep 'time' $logname | tail -1 | cut -d' ' -f 2 | tr -d ' ')"
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

runcmd() {
    P=$1
    mode=$2
    name=$3
    args=$4
    str="CILK_NWORKERS=$P PORR_MODE=$mode ./$name $args &> .log"
    # printf "$str --- "
    CILK_NWORKERS=$P PORR_MODE=$mode ./$name $args &> .log
}

runall () {
    name=$1
    mode=$2
    P=1
    declare -A vals

    for arg in "${args[@]}"; do
        vals=()
        for i in $(seq $NITER); do
            runcmd "$P" "$mode" "$name" "$arg"
            val=$(errcheck $? ".log")
            if [[ $? -ne 0 ]]; then
                printf "Error\n"
                exit 1
            fi
            vals[$i]=$val
            #printf -- "--%5d" "$val"
        done
        avg=$( echo ${vals[@]} | tr " " "\n" | datamash mean 1 | tr -s " ")
        stdev=$( echo ${vals[@]} | tr " " "\n" | datamash sstdev 1 | tr -s " ")
        printf "%10.2f(%5.2f)" "$avg" "$stdev"
    done
    printf "\n"
}

if [ $# -gt 0 ]; then bench=($@); fi
for b in "${bench[@]}"; do

    printf -- "--- $b ---\n"
    printf "%-10s" "type"
    for arg in "${args[@]}"; do
        printf "%10s" "$arg"
    done
    printf -- "\n------------------------------------\n"

    # Array
    make clean &> compile.log
    make -j PTYPE=2 &> compile.log
    printf "%-10s" "base"
    runall "$b" "none"

    printf "%-10s" "array"
    runall "$b" "record"

    # Precomputed
    make clean &> compile.log
    make -j PTYPE=1 &> compile.log
    printf "%-10s" "pre"
    runall "$b" "record"

done
