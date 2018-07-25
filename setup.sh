#!/bin/bash
set -e

BASE_DIR=$(pwd)
rm -f setup.log

function msg() {
    echo "$msg" | tee -a setup.log
}

msg "Begin PORRidge setup at $(date)"

: ${BINUTILS_PLUGIN_DIR:="/usr/local/include"}
if [[ ($BINUTILS_PLUGIN_DIR != "") &&
          (-e $BINUTILS_PLUGIN_DIR/plugin-api.h) ]]; then
    export LTO=1
else
    export LTO=0
    echo "Warning: no binutils plugin found, necessary for LTO"
fi

t=$(ldconfig -p | grep tcmalloc)
if [[ $? != 0 ]]; then
    echo "tcmalloc not found! Install google-perftools"
fi

# Setup and compile our compiler
./build-llvm-linux.sh

msg "Modified clang compiled."
pushd .
cd ./SuperMalloc
git submodule update
cd ./release
make
popd

# # Build the runtime (ability to suspend/resume deques)
cd ./cilkrtssuspend
libtoolize
autoreconf -i
./remake.sh pre opt lto
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
cd src
make -j
cd -

# Check out our fork of PBBS
cd bench
#git clone https://gitlab.com/robertutterback/cilkplus-tests2.git cilkplus-tests

# Compile benchmarks
./build.sh

# Generate data sets for PBBS benchmarks
cd cilkplus-tests
./gendata.sh
cd -

# Download and setup data sets for dedup and ferret
cd $BASE_DIR/bench/dedup
wget www.cse.wustl.edu/~utterbackr/dedup-data.tar.gz
tar -vxzf dedup-data.tar.gz data

cd $BASE_DIR/bench/ferret
wget www.cse.wustl.edu/~utterbackr/ferret-data.tar.gz
tar -vxzf ferret-data.tar.gz data

# Now we're ready to go!
msg "Setup completed. Use bench/bench.sh to run benchmarks"
msg "Running the benchmarks will require setting vm.max_map_count to a high level"
msg "e.g. 'sysctl -w vm.max_map_count=1000000'"
