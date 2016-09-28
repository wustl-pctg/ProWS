#!/bin/bash

trap "kill -- -$$" SIGINT

# Defaults
STARTN=1
ENDN=15
ITER=100
NUM_RECORDS=2

case $# in
		0)
				# Defaults already set
				;;
		1)
				ITER=$1
				;;
		2)
				STARTN=$1
				ENDN=$2
				;;
		3)
				STARTN=$1
				ENDN=$2
				ITER=$3
				;;
		4)
				STARTN=$1
				ENDN=$2
				ITER=$3
				NUM_RECORDS=$4
				;;

		*)
				echo "Incorrect number of arguments"
				exit 1
				;;
esac

ITER_PER_RECORD=$(( $ITER / $NUM_RECORDS ))

maxtime=5
line="                                                                               "

declare verbose
if [[ "$#" -eq 1 && "$1" -eq "v" ]]; then
		verbose=1
else
		verbose=0
fi


function clearline() {
		printf "\r$line \r"
}

function showstatus() {
		if [[ $verbose -eq 0 ]]; then
				clearline
				printf "$status"
		fi
}

# 1=n, 2=recordp, 3=replayp
function runcmd() {

		cmd="CILK_NWORKERS=$3 PORR_MODE=replay timeout --signal=SIGSTOP ${maxtime}s ./cilkfor $1"
		msg="cilkfor($1) recorded with $2 workers, replayed with $3 workers"
		suffix=""
		if [[ "$verbose" -eq 0 ]]; then
				echo $msg > log
				suffix=" >> log 2>&1"
		fi
		eval $cmd $suffix
		res=$?

		if [[ $res -ne 0 ]]; then
				printf "\n"
				if [[ $res -eq 124 ]]; then
						printf "Timeout (stopped): "
				else
						printf "Failed: "
				fi
				printf "$msg\nSee log for details.\n"
				exit 1
		fi
}

make cilkfor
rm -f core

status=""
for n in `seq $STARTN $ENDN`; do
		status="cilkfor($n):"
		for recordp in `seq 1 8`; do
				for i in `seq 1 $NUM_RECORDS`; do
						showstatus
						CILK_NWORKERS=$recordp PORR_MODE=record ./cilkfor $n >& log
						if [[ $? -ne 0 ]]; then
								echo "Problem with record!"
								exit 1
						fi
						for replayp in `seq 1 8`; do
								for j in `seq 1 $ITER_PER_RECORD`; do
                    status="\rcilkfor($n), recordp=${recordp}-${i}, replayp=$replayp, iter=$j / $ITER_PER_RECORD"
								    showstatus
										runcmd $n $recordp $replayp
								done
                status="cilkfor($n), recordp=${recordp}-${i}, replayp=$replayp"
								showstatus
						done
				done
		done
		clearline
		printf "\rDone with cilkfor($n)\n"
done
