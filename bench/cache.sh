#!/bin/bash
set -e
ulimit -v 6291456
# Don't forget that we might need to set vm.max_map_limit
trap "kill -- -$$" SIGINT

testdir="$HOME/src/cilkplus-tests"
CORES="1 2 4 6 8"

# MIS BFS matching dict refine dedup ferret
bench=(MIS)
declare -A dirs args cmdnames makecmds

dirs["dedup"]=dedup
args["dedup"]="medium out"
cmdnames["dedup"]="run.sh lock"
makecmds["dedup"]="make"

dirs["ferret"]=ferret
args["ferret"]="medium out"
cmdnames["ferret"]="run.sh lock"
makecmds["ferret"]="make"

dirs["chess"]="${testdir}/chess"
args["chess"]=""
cmdnames["chess"]="chess-cover-locking"
makecmds["chess"]="make CILK=1 CILKRR=1"

dirs["dict"]="${testdir}/pbbs/dictionary/lockingHash"
args["dict"]="-r 1 ../sequenceData/data/randomSeq_100000_1000_int "
cmdnames["dict"]="dict"
makecmds["dict"]="make CILK=1 CILKRR=1"

dirs["MIS"]="${testdir}/pbbs/maximalIndependentSet/lockingMIS"
args["MIS"]="-r 1 ../graphData/data/randLocalGraph_J_5_10000"
cmdnames["MIS"]="MIS"
makecmds["MIS"]="make CILK=1 CILKRR=1"

dirs["matching"]="${testdir}/pbbs/maximalMatching/lockingMatching"
args["matching"]="-r 1 ../graphData/data/randLocalGraph_E_5_10000"
cmdnames["matching"]="matching"
makecmds["matching"]="make CILK=1 CILKRR=1"

dirs["BFS"]="${testdir}/pbbs/breadthFirstSearch/lockingBFS"
args["BFS"]="-r 1 ../graphData/data/randLocalGraph_J_5_10000"
cmdnames["BFS"]="BFS"
makecmds["BFS"]="make CILK=1 CILKRR=1"

dirs["refine"]="${testdir}/pbbs/delaunayRefine/lockingRefine"
args["refine"]="-r 1 ../geometryData/data/2DinCubeDelaunay_10000"
cmdnames["refine"]="refine"
makecmds["refine"]="make CILK=1 CILKRR=1"


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
        # Hack for chess
        # tmp=$(grep "time" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')

        # printf "%-10.2f" $tmp
        # printf "%-10d" $(grep "L1" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        # printf "%-10d" $(grep "L2" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        # printf "%-10d" $(grep "L3" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')

        # # printf "%-10d" $(grep "Steal attempts" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        # # printf "%-10d" $(grep "Steal fails" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        # tot=$(grep "Steal attempts" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        # fails=$(grep "Steal fails" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        # if [[ "$tot" -eq "0" ]]; then
        #    rate=nan
        # else
        #     rate=$(echo "scale=2; $fails / $tot" | bc)
        # fi
        # printf "%-10.2f" $rate

        echo "here"
        printf "%-10d" $(grep "Total" $logname | tail -1 | cut -d':' -f 2 | tr -d ' ')
        echo "here2"

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
        args="$4 $p"
    fi
    SOCKETS=0xff
    if [[ "$P" -gt "8" ]]; then SOCKETS=0xffff; fi
    # cmd="CILK_NWORKERS=$P CILKRR_MODE=$mode taskset $SOCKETS ./$name $args &> log"
    # echo $cmd
    CILK_NWORKERS=$P CILKRR_MODE=$mode taskset $SOCKETS ./$name $args &> log
    args=$oldargs
}

runall () {
    name=$1
    cmdname=$2
    args=$3

    for p in $CORES; do

        printf "%-10s%-5s" "$1" "$p"

        #modes=("none" "record" "replay")
        modes=("none" "record")
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
    #printf "%10s%10s%10s%10s%10s%10s" "time" "L1D" "L2" "L3" "SA" "SF"
    #printf "%10s%10s%10s%10s%10s" "time" "L1D" "L2" "L3" "sfrate"
    printf "%10s" "acquires"
done
printf "\n"
# printf "%10s\n" "suspends"
for ((i=0; i < 160; i++)); do printf "-"; done
printf "\n"

for b in "${bench[@]}"; do
    cd ${dirs[$b]}
    ${makecmds[$b]} 2>&1 > compile.log
    if [[ "$?" -ne 0 ]]; then
        echo "Compile error!"
        cat compile.log
    fi
    runall "$b" "${cmdnames[$b]}" "${args[$b]}"
    cd - >/dev/null
done
