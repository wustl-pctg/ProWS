#./build-llvm-linux.sh

WORK_DIR=`pwd`

cd ./batched-piper

libtoolize
aclocal
automake --add-missing
autoconf
./configure --prefix=$WORK_DIR/piper-rts CC=$WORK_DIR/llvm-cilk/bin/clang CXX=$WORK_DIR/llvm-cilk/bin/clang++
sed -i 's/-std=c99/ /' Makefile
sed -i 's/CXXFLAGS = -g -O2/CXXFLAGS = -g -O3 -fcilkplus  -std=c++11 -D__CILKRTS_ABI_VERSION=1 -fcilk-no-inline -fno-omit-frame-pointer/' Makefile
sed -i 's/CFLAGS = -g -O2/CXXFLAGS = -g -O3 -fcilkplus  -D__CILKRTS_ABI_VERSION=1 -fcilk-no-inline -fno-omit-frame-pointer/' Makefile
make
make install
