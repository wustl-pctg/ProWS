#!/bin/sh
set -e

# Build all cilkplus-tests benchmarks
./cilkplus-tests/build.sh

cd ferret
make clean
make base -j
make clean
make -j
cd -

cd dedup
make clean
make base -j
make clean
make -j
cd -
