#!/bin/bash
curr=`pwd`
rundir=$curr/src
datadir=$curr/data

if [ $# -le 2 ]; then
echo "Usage ./run.sh <prog> <data size> <output file> [nproc] where"
echo "      prog includes: serial, lock, reducer"
echo "      data size includes dev, small, medium, large, and native."
exit 0
fi

prog=$1
shift;
dsize=$1
shift
outfile=$1
shift;
nproc=$1
shift;

if [[ "$dsize" != 'native' ]]; then
    dsize="sim$dsize"
fi

cmd="$rundir/cilk/dedup-$prog "
cmd+="-c -i $datadir/$dsize/media.dat -o $outfile"

if [[ "$nproc" != '' ]]; then
    echo "export CILK_NWORKERS=$nproc"
    export CILK_NWORKERS=$nproc
fi
echo "$cmd $*"
$cmd $*

# rm -f $outfile
