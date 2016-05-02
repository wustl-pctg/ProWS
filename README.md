# Processor Obvivious Record and Replay

A runtime system to deterministically record lock acquires and replay
them on an arbitrary number of cores. Currently, you need to build the
library and link it statically, although there is no technical reason
why this couldn't be a dynamically-loaded library.
