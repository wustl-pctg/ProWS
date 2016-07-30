#!/bin/bash
set -e
ulimit -v 6291456
# Don't forget that we might need to set vm.max_map_limit
trap "kill -- -$$" SIGINT

testdir="$HOME/src/cilkplus-tests"

bench=(matching)
declare -A dirs args cmdnames makecmds

dirs["dedup"]=dedup
args["dedup"]="small out 1"
cmdnames["dedup"]="run.sh lock"
makecmds["dedup"]="make"

dirs["ferret"]=ferret
args["ferret"]="small out 1"
cmdnames["ferret"]="run.sh lock"
makecmds["ferret"]="make"

dirs["chess"]="${testdir}/chess"
args["chess"]=""
cmdnames["chess"]="chess-cover-locking"
makecmds["chess"]="../../make CILK=1 CILKRR=1"

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

    if [[ $errcode -eq 0 ]]; then
        printf "%d\t%d\t%.2f\t%.2f\t%d" $(grep '{' .cilkrecord | cut -d':' -f 2 | datamash min 1 median 1 mean 1 sstdev 1 max 1 | tr '\t' ' ')
        # line="$(grep '{' .cilkrecord | cut -d':' -f 2 | datamash min 1 median 1 mean 1 sstdev 1 max 1)"
        line="\t$(tail -1 .cilkrecord | cut -d':' -f 2)"
        printf "$line"

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
    rm -f out
}

runcmd() {
    P=$1
    mode=$2
    name=$3
    args=$4
    CILK_NWORKERS=$P CILKRR_MODE=$mode taskset 0xff ./$name $args &> log
}

runall () {
    name=$1
    cmdname=$2
    args=$3
    
    runcmd "1" "record" "$cmdname" "$args"
    val=$(errcheck $? "log")

    # why am I forced to use $1 here?!
    printf "%-10s\t${val}\n" "$1" 
}

if [ $# -gt 0 ]; then bench=($@); fi

printf "%-10s\t%s\t%s\t%s\t%s\t%s\t%s\n" "bench" "min" "median" "mean" "stdev" "max" "conflicts"
printf -- "----------------------------------------------------------------------\n"

for b in "${bench[@]}"; do
    cd ${dirs[$b]}
    ${makecmds[$b]} $b 2>&1 > compile.log
    if [[ "$?" -ne 0 ]]; then
        echo "Compile error!"
        cat compile.log
    fi
    runall "$b" "${cmdnames[$b]}" "${args[$b]}"
    cd - >/dev/null
done
