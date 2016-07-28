#!/bin/bash
set -e
ulimit -v 6291456
# Don't forget that we might need to set vm.max_map_limit
trap "kill -- -$$" SIGINT

MAXTIME="2m"
CORES="1 2 4 6 8"
NITER=3

bench=(ferret) #dedup ferret kdtree
declare -A dirs args

dirs["dedup"]=dedup
args["dedup"]="small" # native
dirs["ferret"]=ferret
args["ferret"]="small"

errcheck () {
    errcode=$1
    logname=$2

    if [[ $errcode -eq 0 ]]; then
        printf "%0.2f" "$(grep 'Processing time' log | tail -1 | cut -d' ' -f 4 | tr -d ' ')"
    else
        printf "\n[Error]:\n"
				if [[ $errcode -eq 124 ]]; then
						printf "Timed out after %s.\n" $MAXTIME
						exit 1
				fi
        cat log
        exit 1
    fi
    # No error, so remove the log file
    rm out
}

runcmd() {
		P=$1
		mode=$2
		name=$3
		args=$4
    CILKRR_MODE=$mode ./$name lock $args out $P &> log
}

runreplay() {
    name=$1
    args=$2
    REPLAYITER=$NITER

    for P in $CORES; do
				avg=0
				for i in $(seq $REPLAYITER); do
						runcmd "$P" "replay" "$name" "$args"
						val=$(errcheck $? "log")
						if [[ $? -ne 0 ]]; then
								printf "Error\n"
								exit 1
						fi
						avg=$( echo "scale=2; $avg + $val" | bc )
				done
				avg=$( echo "scale=2; $avg / $REPLAYITER" | bc )
				printf "\t%s" "$avg"
    done

}


runall () {
    name=$1 # actually, we don't need
    name="run.sh"
    args=$2
    
    printf -- "--- $name ---\n"
    header_left="P\tbase\trecord"
		printf "${header_left}\t\t\treplayP\n"
		printf "%${#header_left}s\t" | tr " " "=" | tr "\t" "====="
		for P in $CORES; do printf "\t$P"; done;
		printf "\n"

    for P in $CORES; do
				printf "$P"
				avg=0
				for i in $(seq $NITER); do
						runcmd "$P" "none" "run.sh" "$args"
						val=$(errcheck $? "log")
						if [[ $? -ne 0 ]]; then
								printf "Error\n"
								exit 1
						fi
						avg=$( echo "scale=2; $avg + $val" | bc )
				done
				avg=$( echo "scale=2; $avg / $NITER" | bc )
				printf "\t%s" "$avg"

				avg=0
				for i in $(seq $NITER); do
						runcmd "$P" "record" "$name" "$args"
						val=$(errcheck $? "log")
						if [[ $? -ne 0 ]]; then
								printf "Error\n"
								exit 1
						fi

						avg=$( echo "scale=2; $avg + $val" | bc )
				done
				avg=$( echo "scale=2; $avg / $NITER" | bc )
				printf "\t%s" "$avg"

				runreplay "$name" "$args"
				printf "\n"
		done
    printf "\n-------------------------"
    set -e

}

if [ $# -gt 0 ]; then bench=($@); fi
for b in "${bench[@]}"; do
    cd ${dirs[$b]}
    make
    runall "$b" "${args[$b]}"
    cd -
done
