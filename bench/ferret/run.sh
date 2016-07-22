#!/bin/bash
cilkview="/export/opt0/angelee/cilk_dev/piper_cilkview/cilkutil/bin/cilkview"

curr=`pwd`
bindir=$curr/src/build/bin
datadir=$curr/data

if [ $# -le 2 ]; then
echo "Usage: ./run.sh <prog> <data_size> <output_file> <nproc>"
echo "where prog includes: serial, lock, reducer." 
echo "      data_size includes: simdev, simsmall, simmedium, simlarge, native."
exit 0
fi

prog=$1;
shift;
dsize=$1
shift;
outfile=$1
shift;
nproc=$1
shift;

cmd="$bindir/ferret-$prog $datadir/$dsize/corel/ lsh $datadir/$dsize/queries/ "
cmd+="10 $outfile"

echo "$cmd $*"
export CILK_NWORKERS=$nproc
$cmd $*
