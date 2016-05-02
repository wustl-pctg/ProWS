# Processor Obvivious Record and Replay

A runtime system to deterministically record lock acquires and replay
them on an arbitrary number of cores. Currently, you need to build the
library and link it statically, although there is no technical reason
why this couldn't be a dynamically-loaded library.

This library relies on modifications to the Cilk runtime library
(provided in the cilkrtssuspend directory), as well as a special
compiler that won't inline some Cilk helper functions. You can find
such a compiler [here](https://gitlab.com/wustl-pctg/llvm-cilk).
