#!/bin/bash
#set -x
cilkview="../../../../piper_cilkview/cilkutil/bin/cilkview"

curr=`pwd`
bindir=$curr/src/build/bin
datadir=$curr/data

if [ $# -le 2 ]; then
echo "Usage: ./run.sh <prog> <data size> <output_file> [nproc] [pipe_depth]
[yield] where"
echo "where prog includes: serial, cilk, pthreads, tbb, parsec3-tbb, piper, cilkview" 
echo "      and data size includes: simdev, simsmall, simmedium, simlarge, native."
echo "NOTE: for cilk version, pipe_depth is used as a multiplier to set the "
echo "      actul depth, i.e., depth = \$(pipe_depth) * \$(nproc)."
echo "NOTE: for every other version, including Cilk Plus Piper, pipe_depth is"
echo "      used as the actual throttling limit of the pipeline."
exit 0
fi

prog=$1
dsize=$2
outfile=$3
nproc=$4
depth=$5
yield=$6

cmd="$bindir/ferret-$prog $datadir/$dsize/corel/ lsh $datadir/$dsize/queries/ "
if [[ $1 = 'serial' || $1 = 'cilkview' || $1 = 'serial2' ]]; then
cmd+="10 $outfile"
elif [[ $1 = 'cilk' ]]; then
    if [[ "$depth" != "" ]]; then
        cmd="$bindir/ferret-$prog --nproc $nproc --throttle $depth "
    else
        cmd="$bindir/ferret-$prog --nproc $nproc "
    fi
    if [[ "$yield" != "" ]]; then
        cmd="$cmd --yield $yield "
    fi
cmd+="$datadir/$dsize/corel/ lsh $datadir/$dsize/queries/ 10 $outfile"
else
cmd+="10 $nproc $outfile $depth"
fi

if [[ $1 = 'cilkview' ]]; then
    cv_cmd="$cilkview --debug -- $cmd"
    cmd=$cv_cmd
fi

echo "$cmd"
echo `$cmd`
