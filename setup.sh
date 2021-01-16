#!/bin/bash
set -e

if [[ $(grep docker /proc/1/cgroup) == "" ]]; then
  echo "Must run this script in the repo's docker container!"
  echo "Please: 1) install the docker container system,"
  echo "        2) install our docker image by running './build_docker.sh',"
  echo "        3) start the docker container by running './start_docker.sh'"
  exit
fi

BASE_DIR=$(pwd)
rm -f setup.log

function msg() {
    echo "$msg" | tee -a setup.log
}

msg "Begin PORRidge setup at $(date)"

: ${BINUTILS_PLUGIN_DIR:="/usr/include"}
if [[ ($BINUTILS_PLUGIN_DIR != "") &&
          (-e $BINUTILS_PLUGIN_DIR/plugin-api.h) ]]; then
    export LTO=1
else
    export LTO=0
    echo "Warning: no binutils plugin found, necessary for LTO"
fi

# Setup and compile our compiler
./build-llvm-linux.sh

msg "Modified clang compiled."

git submodule update --init ./SuperMalloc
cd ./SuperMalloc/release
make
cd -

cd ./cilkplus-rts
libtoolize
autoreconf -i
./remake.sh opt lto
cd -

./piper-setup.sh

# # Build the runtime (ability to suspend/resume deques)
cd ./cilkrtssuspend
libtoolize
autoreconf -i
./remake.sh opt lto
cd -

msg "Suspendable work-stealing runtime built"

# Compile library
mkdir -p build
BASE_DIR=$(pwd)
if [ ! -e config.mk ]; then
    echo "BUILD_DIR=$BASE_DIR/build" >> config.mk
    echo "RUNTIME_HOME=$BASE_DIR/cilkrtssuspend" >> config.mk
    echo "COMPILER_HOME=$BASE_DIR/llvm-cilk" >> config.mk
    echo "RTS_LIB=\$(COMPILER_HOME)/lib/libcilkrts.a" >> config.mk
    echo "LTO=$LTO" >> config.mk
fi

cd ferret
#wget www.cse.wustl.edu/~utterbackr/ferret-data.tar.gz
cat split-data/ferret-data-00 > ferret-data.tar.gz
cat split-data/ferret-data-0{1..7} >> ferret-data.tar.gz
tar -vxzf ferret-data.tar.gz data
rm ferret-data.tar.gz

# Now build all of the benchmarks (starting with ferret)
cd src/
make piper
make cilk-future
make serial
cd ../../future-bench
make all
cd ../futurerd-bench/basic
make all
cd ../heartWallTracking/cilk-futures
make all
cd ../../bintree
make all
