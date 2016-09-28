#!/bin/bash
set -e
# ulimit -v 6291456
# Don't forget that we might need to set vm.max_map_limit
trap "kill -- -$$" SIGINT

: ${BENCH_LOG_FILE:=$HOME/tmp/log}
logfile=$BENCH_LOG_FILE
echo "********** Begin script: $(realpath $0) **********" >> $logfile

MAXTIME="5m"
ITER=5
CORES="1"

#refine MIS matching BFS chess dedup ferret
bench=(fib)
basedir=../src
source config.sh

errcheck () {
    errcode=$1
    logname=$2

    #cat $logname
    if [[ $errcode -eq 0 ]]; then
        printf "%0.2f" "$(grep 'time' $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')"
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
    rm $logname
}

runcmd() {
    P=$1
    mode=$2
    name=$3
    args=$4
    if [[ $name =~ "run.sh" ]]; then
        args="$args out $P"
    fi
    cmd="CILK_NWORKERS=$P PORR_MODE=$mode ./$name $args"
    echo "$cmd" >> $logfile
    
    CILK_NWORKERS=$P PORR_MODE=$mode ./$name $args 2>&1 | tee -a $logfile &> .log
    echo "------------------------------------------------------------" >> $logfile
}

compile() {
    b=$1
    (${makecmds[$b]} -j 2>&1) > compile.log
    if [[ "$?" -ne 0 ]]; then
        echo "Compile error!"
        cat compile.log
        exit 1
    fi
}

runmode () {
    P=$1
    name=$2
    cmdname=$3
    mode=$4
    args=$5
    declare -A vals
    cd ${dirs[$name]}
    compile $name

    vals=()
    for i in $(seq $ITER); do
        runcmd "$P" "$mode" "$cmdname" "$args"
        val=$(errcheck $? ".log")
        if [[ $? -ne 0 ]]; then
            printf "Error\n"
            exit 1
        fi
        vals[$i]=$val
        #echo $val
    done
    avg=$( echo ${vals[@]} | tr " " "\n" | datamash mean 1 | tr -s " ")
    stdev=$( echo ${vals[@]} | tr " " "\n" | datamash sstdev 1 | tr -s " ")
    printf "%10.2f(%5.2f)" "$avg" "$stdev"
    # printf "(%.2f)" "$stdev"

    make clean &> /dev/null
    cd - >/dev/null
}

newstage () {
    stage=$1
    ptype=$2
    cd $basedir
    make clean &> /dev/null

    make STAGE=$stage PTYPE=$ptype -j &> compile.log
    if [[ "$?" -ne 0 ]]; then
        echo "Compile error!"
        cat compile.log
        exit 1
    fi
    cd - >/dev/null
}


if [ $# -gt 0 ]; then bench=($@); fi

printf "%-10s%5s%10s%15s%15s%15s%15s\n" "bench" "P" "ped" "none" "get" "insert" "conflicts"
#printf "%-10s%5s%15s%15s%15s%15s\n" "bench" "P" "pre-none" "pre-record" "array-none" "array-record"

for b in "${bench[@]}"; do
    
    for P in $CORES; do
        printf "%-10s" "$b"
        printf "%5s" $P
        printf "%10s" "array"
        newstage "0" "2"
        runmode "$P" "$b" "${cmdnames[$b]}" "none" "${args[$b]}" # baseline
        newstage "1" "2"
        runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}" # get pedigree
        newstage "2" "2"
        runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}" # insert
        newstage "3" "2"
        runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}" # conflict

        printf "\n"
        printf "%-10s" "$b"
        printf "%5s" $P
        printf "%10s" "pre"
        newstage "0" "1"
        runmode "$P" "$b" "${cmdnames[$b]}" "none" "${args[$b]}" # baseline
        newstage "1" "1"
        runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}" # get pedigree
        newstage "2" "1"
        runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}" # insert
        newstage "3" "1"
        runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}" # conflict


        # newstage "3" "1"
        # runmode "$P" "$b" "${cmdnames[$b]}" "none" "${args[$b]}"
        # newstage "3" "1"
        # runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}"
        
        # newstage "3" "2"
        # runmode "$P" "$b" "${cmdnames[$b]}" "none" "${args[$b]}"
        # newstage "3" "2"
        # runmode "$P" "$b" "${cmdnames[$b]}" "record" "${args[$b]}"
        
        printf "\n"
    done
done

echo "********** End script: $(realpath $0) **********" >> $logfile
