# ProWS - Proactive Work Stealing for Futures
A runtime system to schedule futures in a non-parsimonious manner.
Instead of popping tasks off of the bottom of the worker's deque
when touching a future that is not yet ready, this runtime will
suspend the entire deque and look to steal. Counterintuitively,
this results in a better bound on the number of deviations incurred
when running a program on multiple cores. You can find more details
in the PPoPP 2019 paper *Proactive Work Stealing for Futures*.

Currently, you need to build the library and link it statically,
although there is no technical reason why this couldn't be a
dynamically-loaded library. This library relies on modifications to
the Cilk runtime library (provided in the cilkrtssuspend directory),
as well as a special compiler that won't inline some Cilk helper
functions. You can find such a compiler
[here](https://gitlab.com/wustl-pctg-pub/llvm-cilk).

## License

The code in this repository is licensed under The MIT License (see
LICENSE) except for the runtime modifications in cilkrtssupend, which
is separately licensed. Note that some scripts in this repository
access other open source projects, which are licensed separately.

## Dependencies

A relatively recent version of Linux is recommended. ProWS was tested
on Ubuntu 16.04, but is expected to run correctly in other Linux
distributions.

In its current state, it is expected (and strongly encouraged) that the
system is built within a Docker container, for which scripts to both
create and launch have been provided. The included Docker container has
all libraries required to both build and run all compilers, runtimes,
and benchmarks included in this project. All scripts and Makefile resources
have been optimized for the included Docker container environment. Most
of our testing used Docker version 18.09.0, build 4d60db4, but was also
tested for functionality in one of the 17.xx.x releases provided in the
Ubuntu 16.04 repositories (package docker.io). This reliance on Docker is
not inherent to the system, but should make the artifact evalutaion process
easier.

For those interested in compiling the software in their operating system
rather than in a Docker container, the GNU _gold_ linker should be
installed as _ld_. This requirement may be eliminated as the project is
refactored. Compiling the _ferret_ benchmark requires additional dependencies.
A full list of package dependencies for an Ubuntu 16.04 system can be found by
looking at the included _Dockerfile_.

## Installation

Before installation, make sure to install the Docker container system. Docker
can be obtained either through your operating systems package manager or by
downloading from the Docker website
[here](https://store.docker.com/search?type=edition&offering=community&operating_system=linux).

The script _build-docker.sh_ will build the Docker container that has
all the libraries needed to build and run any code contained in this
repository.

The script _start-docker.sh_ will start the Docker container and set up
the Docker container environment. It is strongly recommended that you use
this script whenever you start the Docker container.

The script _setup.sh_ should build the modified compiler, the SuperMalloc
library, all required Cilk Plus runtimes, and all benchmarks.
It is specifically set up to enforce use of the Docker container.

## Using

It is recommended that the benchmarks are run within the provided Docker
container. When the Docker container is started with the _start-docker.sh_
script, the location of the _SuperMalloc_ library is added to the
LD\_LIBRARY\_PATH to allow the linker to locate it when dynamic libraries are
loaded as part of the binary execution process.

In order to run all benchmarks related to the *Proactive Work-Stealing for Futures*
paper, run the _run-benchmarks.py_ script from the root directory of the
repository (from within the Docker container). This will both run all of the
benchmarks and store the data in a file named _all\_data.csv_. The data for each
benchmark also gets stored in individual csv files within a directory named
bench-results.

The _run-benchmarks.py_ script can be customized by modifying the _benchargs.py_
configuration script in the root of the repository. This file contains a list of
the CPU counts to run the benchmark on (the _core\_counts_ list), As well as a
dictionary that maps binary names to both the arguments to use for the binary and
the number of times to run that binary (the _bench\_args_ dictionary). Within the
_bench\_args_ dictionary, the keys are the names of what to run and the entries 
are themselves dictionaries. The dictionaries stored in the _bench\_args_
dictionary contain a mapping from the string 'args' to the argument string used
on the command line, as well as a mapping from the string 'runs' to the number
of times to run the benchmark. More benchmarks can be added to the script by
adding them to this dictionary, and the arguments to each benchmark can be
changed by modifying the string in the 'args' mapping. For a description of how
the provided benchmarks are invoked, see the included _benchmark\_list.txt_ file.
In order to run the same benchmark multiple times but with different arguments,
simply add a new entry for the benchmark with a number appended to the name. An
example of this can be seen in the included _benchargs.py_ file, where there are
two different configurations for the lcs benchmark. Note that this means the
script requires that all binary names end with non-numeric text, as they get
stripped from the binary name provided in the dictionary (i.e. the script will
execute _lcs-fj_ rather than a binary named _lcs-fj2_ in the example in
_benchargs.py_).

The Docker container is recommended because it has all the dependencies
installed, and the overhead of the container is so low that we did not
notice any difference between our benchmark results when run in the
container versus when run directly in the host Linux machine. Even though
the use of a Docker container allows for the benchmarks to be run under
any operating system that supports the use of Docker containers, it is
recommended that the Docker container is only run from within a Linux
operating system. Only in Linux operating systems do the host operating
system and the guest operating system (the one in the container) share
the same kernel process. For all other operating systems, Docker runs
a Linux virtual machine on top of which the containers run.

## Source Files of Interest

cilkrtssuspend is the folder containing the Cilk-F runtime source code.

The code changes most relevant to scheduling occur in:

    cilkrtssuspend/runtime/deque.c
    cilkrtssuspend/runtime/deque_pool.c
    cilkrtssuspend/runtime/scheduler.c
    cilkrtssuspent/include/cilk/future.h

In scheduler.c, perhaps the most relevant function to Cilk-F's ProWS scheduling
algorithm is the random_steal function.