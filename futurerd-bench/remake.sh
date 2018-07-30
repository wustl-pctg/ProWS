#!/bin/bash
set -e

PROJECT_DIR=$(dirname $(readlink -f $0))/..
BENCH_DIR=$PROJECT_DIR/bench

# 1 - mode
function runtime {
    pushd $PROJECT_DIR/runtime >/dev/null
    local args=
    if [[ "$1" == "release" ]]; then
        args="opt lto"
    fi
    ./remake.sh $args
    popd >/dev/null
}

# 1 - mode
function tool {
    pushd $PROJECT_DIR/src >/dev/null
    make clean
    STATS=${STATS:-0}
    STATS=$STATS make mode=$1 -j
    popd #>/dev/null
}

function system {
    runtime $1
    tool $1
}

# 1 - mode
# 2 - rdalg
# 3 - ftype
function basic {
    pushd $BENCH_DIR/basic >/dev/null
    make clean
    make mode=$1 rdalg=$2 ftype=$3 -j
    popd >/dev/null
}

function bintree {
    pushd $BENCH_DIR/bintree >/dev/null
    make clean
    make mode=$1 rdalg=$2 ftype=$3 -j
    popd >/dev/null
}

function heartwall {
    pushd $BENCH_DIR/heartWallTracking/cilk-futures >/dev/null
    make clean
    make mode=$1 rdalg=$2 ftype=$3 -j
    popd >/dev/null
}

function bodytrack {
    pushd $BENCH_DIR/pipeline/bodytrack/src >/dev/null
    make clean
    make mode=$1 rdalg=$2 ftype=$3 -j
    popd >/dev/null
}

function dedup {
    pushd $BENCH_DIR/pipeline/dedup/src >/dev/null
    make clean
    make mode=$1 rdalg=$2 ftype=$3 -j
    popd >/dev/null
}

function allbench {
    basic $1 $2 $3
    bintree $1 $2 $3
    heartwall $1 $2 $3
    dedup $1 $2 $3
    #bodytrack $1 $2
}

function all {
    system $1
    allbench $1 $2 $3
}
