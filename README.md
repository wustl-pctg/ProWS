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

Before installation, make sure to install the Docker container system.

The script _build-docker.sh_ will build the Docker container that has
all the libraries needed to build and run any code contained in this
repository.

The script _start-docker.sh_ will start the Docker container and set up
the Docker container environment. It is strongly recommended that you use
this script to start the Docker container.

The script _setup.sh_ should build the modified compiler, the SuperMalloc
library, all required Cilk Plus runtimes, and all benchmarks.
It is specifically set up to enforce use of the Docker container.

## Using

It is recommended that the benchmarks are run within the provided Docker
container. When the Docker container is started with the _start-docker.sh_
script, the location of the _SuperMalloc_ library is added to the
LD\_LIBRARY\_PATH to allow the linker to locate it when the benchmarks are
loaded for execution.

TODO: Add information about how to run all the benchmarks using Justin's scripts.

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
