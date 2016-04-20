#!/bin/bash

# N=${1:-10000000}
# export PROG=cilkfor
N=${1:-38}
export PROG=fib
ITER=${2:-5}
P=1 # Fixed, for now

clean() {
		make clean > /dev/null 2>&1
		echo "Compiling with $1"
		cmd="$1 make -j8 $PROG > log 2>&1"
		eval $cmd
		if [[ "$?" -ne 0 ]]; then
				echo "Compilation failed with $1";
				echo "See log";
				exit 1;
		fi
}


# 1=num iter, 2=n, 3=P, 4=extra env vars
runcmd() {
		local ITER=$1
		local N=$2
		local P=$3
		local E=$4

		#cmd="CILK_NWORKERS=$P $E /usr/bin/time -f "%E" -- ./$PROG $N 2>&1 | tail -1 | cut -d ':' -f 2"
		cmd="CILK_NWORKERS=$P $E /usr/bin/time -f '%E' -- ./$PROG $N 2>&1"
		local result=0
		local tmp=0
		for i in `seq 1 $ITER`; do
				tmp=$(eval $cmd)
				if [[ "$?" -ne 0 ]]; then
						echo "Execution failed:";
						echo "$tmp";
						exit 1;
				fi
				tmp=$(eval "echo '$tmp' | tail -1 | awk -F : '{ print (\$1 * 60) + \$2 }'")
				result=$(echo "scale=2; $tmp + $result" | bc)
		done
		echo $(echo "scale=2; $result / $ITER" | bc)
}

printf "Running %s %d, P=%d, %d iterations \n" $PROG $N $P $ITER

# clean "" mv fib fib_base

# Just doing a normal make results in slower (non-recording) code that
#using these params... this is strange, and something I ought to
#figure out
# clean "PARAMS='-DPTYPE=0 -DPMETHOD=0 -DIMETHOD=0'"
# base=$(runcmd $ITER $N $P "CILKRR_MODE=none")
# printf "Base: %f\n" $base

# clean "PARAMS='-DPTYPE=0 -DPMETHOD=0 -DIMETHOD=0'"
# # mv fib fib_call
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Call only: %f\n" $record

# clean "PARAMS='-DPTYPE=0 -DPMETHOD=1 -DIMETHOD=0'"
# # mv fib fib_walk
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Walk only: %f\n" $record

# clean "PARAMS='-DPTYPE=1 -DPMETHOD=2 -DIMETHOD=0'"
# # mv fib fib_str_none
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "String,none: %f\n" $record

# clean "PARAMS='-DPTYPE=1 -DPMETHOD=2 -DIMETHOD=1'"
# # mv fib fib_str_hash
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "String,hash: %f\n" $record

# clean "PARAMS='-DPTYPE=1 -DPMETHOD=2 -DIMETHOD=2'"
# # # mv fib fib_str_ll
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "String,LL: %f\n" $record

clean "PARAMS='-DPTYPE=2 -DPMETHOD=2 -DIMETHOD=0'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Array,none: %f\n" $record

# Too slow...which is fine, we only NEED hash for compressed pedigrees
# clean "PARAMS='-DPTYPE=2 -DPMETHOD=2 -DIMETHOD=1'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Array,hash: %f\n" $record

clean "PARAMS='-DPTYPE=2 -DPMETHOD=2 -DIMETHOD=2'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Array,LL: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DPMETHOD=3 -DIMETHOD=0'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,none: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DPMETHOD=3 -DIMETHOD=1'"
# cp fib fib_dot_hash
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,hash: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DPMETHOD=3 -DIMETHOD=3'"
# cp fib fib_dot_conflict
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,hash-conflict: %f\n" $record

# clean "PARAMS='-DPTYPE=3 -DPMETHOD=4 -DIMETHOD=0'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,none: %f\n" $record

# clean "PARAMS='-DPTYPE=3 -DPMETHOD=4 -DIMETHOD=1'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,hash: %f\n" $record

# clean "PARAMS='-DPTYPE=3 -DPMETHOD=4 -DIMETHOD=3'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,hash-conflict: %f\n" $record

# overhead=$(echo "scale=2; $record / $base" | bc)
# printf "Overhead: %.2fx\n" $overhead
unset PROG
