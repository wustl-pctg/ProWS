#!/bin/bash
set -e

EXTRA=""
OPT=""
LTO=""
cd $(dirname $0)
LLVM_HOME=$(pwd)/../llvm-cilk

# -march=native -m64
NORM="-g -fcilk-no-inline"

if [[ "$1" = "opt" || "$2" = "opt" || "$3" = "opt" ]]; then
		OPT=" -O3 " #OPT=" -O3 -DNDEBUG "
else
    OPT=" -O0 "
fi
if [[ "$1" = "lto" || "$2" = "lto" || "$3" = "lto" ]]; then
		LTO=" -flto "
		EXTRA=" AR_FLAGS=\"cru --plugin=$LLVM_HOME/lib/LLVMgold.so\" RANLIB=/bin/true "
fi
if [[ "$1" = "pre" || "$2" = "pre" || "$3" = "pre" ]]; then
		OPT+=" -DPRECOMPUTE_PEDIGREES=1 "
fi

cmd="./configure --prefix=$LLVM_HOME CC=$LLVM_HOME/bin/clang CXX=$LLVM_HOME/bin/clang++ CFLAGS=\"$NORM $OPT $LTO\" CXXFLAGS=\"$NORM  $OPT  $LTO\" $EXTRA"

#make clean && make distclean
echo $cmd
eval $cmd
make -j
make install
cd -
