#!/bin/bash
set -e

SCRIPT_DIR=$(readlink -f $(dirname $0))
CONFIG_FILE=$SCRIPT_DIR/cilkplus-tests/myconfig.mk

if [ ! -e $CONFIG_FILE ]; then
    echo "COMPILER = LLVM" >> $CONFIG_FILE
    echo "include $SCRIPT_DIR/bench.mk" >> $CONFIG_FILE
fi

function ferret() {
    cd ferret
    make clean; make base -j
    make clean; make -j
    cd -
}

function dedup() {
    cd dedup
    make clean; make base -j
    make clean; make -j
    cd -
}

pushd $SCRIPT_DIR

if [ $# -gt 0 ]; then
    eval $1 # only works for ferret and dedup, use
    # cilkplus-tests/build.sh for others
else
    # Build all cilkplus-tests benchmarks
    ./cilkplus-tests/build.sh
    ferret
    dedup
fi

popd
