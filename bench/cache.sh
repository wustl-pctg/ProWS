#!/bin/bash
set -e
trap "kill -- -$$" SIGINT

: ${BENCH_LOG_FILE:=$HOME/tmp/log}
logfile=$BENCH_LOG_FILE
echo "********** Begin script: $(realpath $0) **********" >> $logfile

CORES="1"
ITER=1

# MIS BFS matching dict refine dedup ferret
bench=(refine)
source config.sh

errcheck () {
    errcode=$1
    logname=$2
    mode=$3

    if [[ $errcode -eq 0 ]]; then

        # tmp=$(grep "time" $logname | tail -1)
        # if [[ $tmp =~ "PBBS" ]]; then
        #     tmp=$(grep "time" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        # else
        #     tmp=$(grep "time" $logname | tail -1 | cut -d' ' -f 4 | tr -d ' ')
        # fi
        # # Hack for chess
        tmp=$(grep "time" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')

        printf "%-10.2f" $tmp
        tmp=$(grep "L1" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        printf "%-10.2f" $(echo "scale=2; $tmp / 1000000" | bc)
        tmp=$(grep "L2" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        printf "%-10.2f" $(echo "scale=2; $tmp / 1000000" | bc)
        tmp=$(grep "L3" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        printf "%-10.2f" $(echo "scale=2; $tmp / 1000000" | bc)
        
        if [[ "$mode" = "replay" ]]; then
            printf "%-10d" $(grep "Suspensions" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        fi

    else
        printf "\n[Error]:\n"
        if [[ $errcode -eq 124 ]]; then
            printf "Timed out after %s.\n" $MAXTIME
            exit 1
        fi
        cat $logname
        exit 1
    fi
    # No error, so remove the log file
    rm -f out
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
    # hack
    SOCKETS=0x01
    CILK_NWORKERS=$P CILKRR_MODE=$mode taskset $SOCKETS ./$name $args &> log
    args=$oldargs
}

runall () {
    name=$1
    cmdname=$2
    args=$3

    for p in $CORES; do

        printf "%-10s%-5s" "$1" "$p"

        modes=("none" "record" "replay")
        for mode in "${modes[@]}"; do
            runcmd "$p" "$mode" "$cmdname" "$args"
            val=$(errcheck $? "log" "$mode")
            printf "${val}"
        done
        printf "\n"
    done

}

if [ $# -gt 0 ]; then bench=($@); fi

printf "%-10s%-5s%40s%40s%40s\n" "bench" "P" "normal" "record" "replay"
printf "%-10s" ""
for ((i=0; i < 3; i++)); do
    printf "%10s%10s%10s%10s" "time" "L1D" "L2" "L3"
done
printf "%10s\n" "suspends"
for ((i=0; i < 160; i++)); do printf "-"; done
printf "\n"

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
