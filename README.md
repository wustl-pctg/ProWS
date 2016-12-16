# Processor Obvivious Record and Replay

A runtime system to deterministically record lock acquires and replay
them on an arbitrary number of cores. If you know your program has an
atomicity bug, this system allows you to record execution repeatedly
until you see the failure. Then you can replay the system (possibly in
a debugger) on as many cores as you like, achieving good speedup if
your program has ample parallelism. Our system ensures weak
determinism during replay. Specifically, lock acquisitions occur in
the same order as recording. You can find more details in the PPoPP
2017 paper *Processor-Oblivious Record and Replay*.

Currently, you need to build the library and link it statically,
although there is no technical reason why this couldn't be a
dynamically-loaded library. This library relies on modifications to
the Cilk runtime library (provided in the cilkrtssuspend directory),
as well as a special compiler that won't inline some Cilk helper
functions. You can find such a compiler
[here](https://gitlab.com/wustl-pctg/llvm-cilk).

## License

The code in this repository is licensed under The MIT License (see
LICENSE) except for the runtime modifications in cilkrtssupend, which
is separately licensed. Note that some scripts in this repository
access other open source projects, which are licensed separately.

## Dependencies

A relatively recent version of Linux is recommended, with a C++11
version of the C++ standard library. PORRidge was tested on Ubuntu
16.04, but is expected to run correctly in other Linux distributions.

To reproduce performance results, Google's _tcmalloc_ should be
installed. Also, the GNU _gold_ linker should be installed as
_ld_. This requirement may be eliminated as the project is
refactored. The benchmark script requires GNU _datamash_. Compiling
the _dedup_ and _ferret_ benchmarks requires additional dependencies.

## Installation

The script _setup.sh_ should build the modified compiler, the modified
Cilk Plus runtime, and all benchmarks.

Before running, make sure to install GNU _gold_ as your linker -- the
system _ld_ should point to _gold_. Installing _gold_ also installs a
header called plugin-api.h, usually in either /usr/include or
/usr/local/include. Find this file and replace the BINUTILS_PLUGIN_DIR
variable in _setup.sh_ with this path.

Some of the benchmarks require many _mmap_ calls when replaying on large data sets. To account for this, you may need to change the number of _mmap_ calls allowed, e.g.:

	sudo sysctl -w vm.max_map_count=1000000

## Using

The _bench/bench.sh_ script will run all benchmarks using
PORRidge. You can change the variable CORES to a list of cores to run
the benchmarks on. By default it runs on 1,2,4,8,12, and 16 cores. The
results are printed directly to the screen by default.

Currently, the provided interface is basically that of
pthread\_spinlock, so just replace calls to pthread\_spinlock\_<func>
with porr::spinlock::<func>. That namespace will probably change.

By default, once you've linked, nothing will actually happen. You'll
need to set the environment variable PORR_MODE to tell the library
to either record or replay. You can record with

    PORR_MODE=record ./prog

which will write out the results to ".cilkrecord". To replay, use

	PORR_MODE=replay ./prog
    
You can use the PORR_FILE environment variable to change this
filename used by both recording and replaying.

If you dynamically allocate memory for locks, you should call the

	porr::reserve_locks(<num locks>)
	
function beforehand. For any cilk_for loops you should also manually specify the cilk grainsize, e.g.:

	#pragma cilk grainsize = 1024
	
These restrictions allowed for an easier implementation; they are no
inherent to the design of PORRidge.
