#!/bin/bash

curr=`pwd`
bindir=$curr/src/build/bin
datadir=$curr/data

if [ $# -le 2 ]; then
echo "Usage: ./run.sh <prog> <data_size> <output_file> <nproc>"
echo "where prog includes: serial, lock, reducer." 
echo "      data_size includes: dev, small, medium, large, native."
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

if [[ "$dsize" != 'native' ]]; then
    dsize="sim$dsize"
fi

cmd="$bindir/ferret-$prog $datadir/$dsize/corel/ lsh $datadir/$dsize/queries/ "
cmd+="10 $outfile"

if [[ "$nproc" != '' ]]; then
    echo "export CILK_NWORKERS=$nproc"
    export CILK_NWORKERS=$nproc
fi
echo "$cmd $*"
$cmd $*
