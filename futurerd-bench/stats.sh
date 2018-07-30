#!/bin/bash
set -e

# T=$(date -d "today" +"%Y%m%d%H%M")
# echo $T
# sleep 5
# echo $T
# exit

PROJECT_DIR=$(dirname $(readlink -f $0))/..
BENCH_DIR=$PROJECT_DIR/bench

declare -A ARGS
TREE_SIZE=$((8192 * 512))
ARGS=([lcs]="-n $((1024 * 16))"
      [sw]="-n 2048"
      [matmul_z]="-n 2048"
      [hw]="../data/test.avi 10 1"
      [bt]="../data/simlarge/sequenceB_4 4 4 4000 5 4 1 1"
      [dedup]="-c -i ../data/simlarge/media.dat -o ../out"
      [merge]="-s1 $TREE_SIZE -s2 $(( $TREE_SIZE / 2 )) -kmax $(( $TREE_SIZE * 4 ))"
     )

# TREE_SIZE=$((1024 * 64))
# ARGS=([lcs]="-n 32"
#       [sw]="-n 32"
#       [matmul_z]="-n 32"
#       [hw]="../data/test.avi 1 1"
#       [bt]="../data/simdev/sequenceB_1 4 1 100 3 4 1 1" # See run.sh in bodytrack
#       [dedup]="-c -i ../data/simsmall/media.dat -o ../out"
#       [merge]="-s1 $TREE_SIZE -s2 $(( $TREE_SIZE / 2 )) -kmax $(( $TREE_SIZE * 4 ))"
#      )


declare -A DIRS
DIRS=([lcs]="basic/" [sw]="basic/" [matmul_z]="basic/"
      [hw]="heartWallTracking/cilk-futures/"
      [bt]="pipeline/bodytrack/build/"
      [dedup]="pipeline/dedup/build/"
      [merge]="bintree/"
     )

PROGS=(merge)

function getstats() {
    local timestamp=$(date -d "today" +"%Y%m%d%H%M")
    local outputlog=$(pwd)/output-${timestamp}.log
    #local statslog=$(pwd)/stats-${timestamp}.log
    local statslog=$1
    rm -f $statslog

    for bench in ${PROGS[@]}; do
        pushd ${DIRS[$bench]} >/dev/null
        local run="./${bench}-rd 2>&1 ${ARGS[$bench]}"
        echo "$run"

        local output=$(eval $run)
        echo "$tmp" >> $outputlog
        if [[ "$?" -ne 0 ]]; then printf "Execution failed:\n$tmp"; exit 1; fi

        gather="grep '^>>>'"
        result=$(eval "echo \"$output\" | $gather")
        outstr="----- $run -----\n${result}\n"
        printf -- "$outstr" >> $statslog
        popd >/dev/null
    done
    cat $statslog
}

STATS=1
source remake.sh

all release structured
getstats $(pwd)/stats-structured.log

all release nonblock
getstats $(pwd)/stats-nonblock.log
