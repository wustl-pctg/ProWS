#!/bin/bash
curr=`pwd`
rundir=$curr/src
datadir=$curr/data

if [ $# -le 2 ]; then
echo "Usage ./run.sh <prog> <data size> <output file> [nproc] [depth] where"
echo "      prog includes: serial, cilk, reducer, pthreads, tbb"
echo "      data size includes dev, small, medium, large, and native."
exit 0
fi

prog=$1
shift;
dsize=$1
shift
outfile=$1
shift;

if [ "$prog" != 'reducer' ]; then
    nproc=$1
    shift;
    depth=$1
fi

if [ "$nproc" = "" ]; then
    nproc=1
fi

if [[ "$dsize" = 'dev' || "$dsize" = 'small' || "$dsize" = 'medium' || "$dsize" = 'large' ]]; then
    dsize="sim$dsize"
fi

# For P=1, set maxP to be small.
if [[ $1 = 'serial' ]]; then
    nproc=1
fi

if [ "$prog" = 'serial' ]; then
    cmd="$rundir/cilk/dedup-serial "  
elif [ "$prog" = 'parsec-serial' ]; then
    cmd="$rundir/parsec3/dedup-serial "
elif [ "$prog" = 'cilk' ]; then
    cmd="$rundir/cilk/dedup-cilk --nproc $nproc "  
elif [ "$prog" = 'reducer' ]; then
    cmd="$rundir/cilk/dedup-reducer "
elif [ "$prog" = 'null' ]; then
    export CILK_NWORKERS=1
    cmd="$rundir/cilk/dedup "
elif [ "$prog" = 'pthreads' ]; then
    cmd="$rundir/parsec3/dedup-pthreads -t $nproc "  
    if [[ "$depth" != "" ]]; then
         cmd+=" -q $depth "
    fi
elif [ "$prog" = 'tbb' ]; then
    cmd="$rundir/cilk/dedup-tbb -t $nproc "  
fi

cmd+="-c -i $datadir/$dsize/media.dat -o $outfile"

echo "$cmd $*"
$cmd $*

rm -f $outfile
