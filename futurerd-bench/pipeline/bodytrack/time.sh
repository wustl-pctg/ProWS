#!/bin/bash
set -e

bindir='./build'
datadir='./data'

btypes=(base reach inst rd)
iter=1
log=times.csv
dsize=simsmall
if [ $# -ge 1 ]; then dsize=$1; fi
outputBMP=1
model=4
nproc=1

# args = <data path> <# camera> <# frames> <# particles> <annealing layers>
# <thread model> <nproc> <output>
case "$dsize" in
    "simdev")
        dpath="$datadir/$dsize/sequenceB_1"
        args="$dpath 4 1 100 3 $model $nproc $outputBMP"
    ;;
    "simsmall")
        dpath="$datadir/$dsize/sequenceB_1"
        args="$dpath 4 1 1000 5 $model $nproc $outputBMP"
    ;;
    "simmedium")
        dpath="$datadir/$dsize/sequenceB_2"
        args="$dpath 4 2 2000 5 $model $nproc $outputBMP"
    ;;
    "simlarge")
        dpath="$datadir/$dsize/sequenceB_4"
        args="$dpath 4 4 4000 5 $model $nproc $outputBMP"
    ;;
    "native")
        dpath="$datadir/$dsize/sequenceB_261"
        args="$dpath 4 261 4000 5 $model $nproc $outputBMP"
    ;;
esac

sep=","
headers=(Benchmark Type Iter MS)
rm -f $log
for h in ${headers[@]}; do
    printf "${h}${sep}" >> $log
done
printf "\n" >> $log


for btype in ${btypes[@]}; do
    printf "Running bt-$btype $dsize $iter times\n"

    for i in $(seq 1 $iter); do
        cmd="/usr/bin/time -o ${log} $bindir/bt-$btype $args 2>&1"
        # printf "$cmd\n"
        tmp=$(eval $cmd)

        if [[ "$?" -ne 0 ]]; then
            echo "Execution failed:"
            echo "$tmp";
            exit 1;
        fi

        # printf "\nChecking results ... \n"
        # printf "diff $dpath/poses.txt $dpath/correct-poses.txt\n"
        # diff $dpath/poses.txt $dpath/correct-poses.txt > ${btype}.diff

        # if [[ "$?" -ne 0 ]]; then
        #     echo "Incorrect results! See ${btype}.diff"
        #     exit 1;
        # else
        #     rm ${btype}.diff
        # fi
        cat $log
    done
done

# printf -- "-------------------- Results --------------------\n"
# cat $log #| column -s $SEP -t
