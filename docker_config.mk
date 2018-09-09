BUILD_DIR=/mnt/cilk-plus-futures/build
RUNTIME_HOME=/mnt/cilk-plus-futures/cilkrtssuspend
COMPILER_HOME=/mnt/cilk-plus-futures/llvm-cilk
RTS_LIB=$(COMPILER_HOME)/lib/libcilkrts.a
LTO=1
ftype ?= structured
