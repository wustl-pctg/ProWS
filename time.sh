#!/bin/bash

PROG=${1:-cilkfor}
N=${2:-100000000}
RESERVE=$N

# PROG=${1:-fib}
# N=${2:-38}
# RESERVE=$(echo "s=1.618 ^ $N; scale = 0; s /= 1; s += 100; print s" | bc)

export PROG
ITER=${3:-3}
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

		cmd="CILK_NWORKERS=$P $E /usr/bin/time -f '%E' -- ./$PROG $N 2>&1"
		# cmd="CILK_NWORKERS=$P $E ./$PROG $N 2>&1"
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
				# tmp=$(eval "echo '$tmp' | tail -1")
				result=$(echo "scale=2; $tmp + $result" | bc)
		done
		echo $(echo "scale=2; $result / $ITER" | bc)
}

printf "Running %s %d, P=%d, %d iterations \n" $PROG $N $P $ITER

clean
base=$(runcmd $ITER $N $P "CILKRR_MODE=none")
printf "Base: %f\n" $base

clean "PARAMS='-DPTYPE=0 -DIMETHOD=0'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Call only: %f\n" $record

clean "PARAMS='-DPTYPE=1 -DIMETHOD=0'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Walk only: %f\n" $record

clean "PARAMS='-DPTYPE=2 -DIMETHOD=0'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Array,none: %f\n" $record

clean "PARAMS='-DPTYPE=2 -DIMETHOD=1 -DUSE_STL=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Array,STL list: %f\n" $record

clean "PARAMS='-DPTYPE=2 -DIMETHOD=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Array,LL: %f\n" $record

clean "PARAMS='-DPTYPE=2 -DIMETHOD=2'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Array,CLL: %f\n" $record

clean "PARAMS='-DPTYPE=2 -DIMETHOD=2 -DRESERVE_SIZE=$RESERVE'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Array,CLL,reserve: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=0'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,none: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=2'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,CLL only: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=3 -DUSE_STL=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,STL hash: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=3 -DUSE_STL=1 -DRESERVE_SIZE=$RESERVE'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,STL hash, reserve: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=3 -DUSE_STL=1 -DCONFLICT_CHECK=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,STL hash,conflict: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=3 -DUSE_STL=1 -DRESERVE_SIZE=$RESERVE -DCONFLICT_CHECK=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,STL hash, reserve, conflict: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=5'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,Mixed: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=5 -DRESERVE_SIZE=$RESERVE'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,Mixed, reserve: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=5 -DCONFLICT_CHECK=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,Mixed,conflict: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=5 -DRESERVE_SIZE=$RESERVE -DCONFLICT_CHECK=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,Mixed, reserve, conflict: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=4'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,myhash: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=4 -DRESERVE_SIZE=$RESERVE'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,myhash,reserve: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=4 -DCONFLICT_CHECK=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,myhash-conflict: %f\n" $record

clean "PARAMS='-DPTYPE=3 -DIMETHOD=4 -DRESERVE_SIZE=$RESERVE -DCONFLICT_CHECK=1'"
record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
printf "Dot,myhash-conflict,reserve: %f\n" $record

# clean "PARAMS='-DPTYPE=4 -DIMETHOD=0 -DPRECOMPUTE_PEDIGREES=1'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,none: %f\n" $record

# clean "PARAMS='-DPTYPE=4 -DIMETHOD=4 -DPRECOMPUTE_PEDIGREES=1'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,myhash: %f\n" $record

# clean "PARAMS='-DPTYPE=4 -DIMETHOD=4 -DRESERVE_SIZE=$RESERVE -DPRECOMPUTE_PEDIGREES=1'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,myhash,reserve: %f\n" $record

# clean "PARAMS='-DPTYPE=4 -DIMETHOD=4 -DCONFLICT_CHECK=1 -DPRECOMPUTE_PEDIGREES=1'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,myhash-conflict: %f\n" $record

# clean "PARAMS='-DPTYPE=4 -DIMETHOD=4 -DRESERVE_SIZE=$RESERVE -DCONFLICT_CHECK=1 -DPRECOMPUTE_PEDIGREES=1'"
# record=$(runcmd $ITER $N $P "CILKRR_MODE=record")
# printf "Pre,myhash-conflict,reserve: %f\n" $record

unset PROG
