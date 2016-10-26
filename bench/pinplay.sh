#!/bin/bash
set -e

logname=drlog
prog=./refine
args="-r 1 ../geometryData/data/2DinCubeDelaunay_1000000"
# args="-r 1 ../geometryData/data/2DinCubeDelaunay_10000"
func=refine
pindir=$HOME/src/pinplay
libdir=$pindir/extras/pinplay/bin/intel64
tool="$pindir/pin -mt -t $libdir/pinplay-driver.so"

# Record only part of the run. Works for logging but replay doesn't output anything ?!
#pinopts="-log:control start:enter_func:$func,stop:exit_func:$func"

function parse() {
    # echo "$1" | grep "PBBS-time" | tail -1 | cut -d':' -f2 | tr -d " "
    echo "$1" | tail -1
}

for P in 1; do

    CILK_NWORKERS=$P /usr/bin/time -f'%E' \
                 $prog $args &> base.log
    CILK_NWORKERS=$P /usr/bin/time -f'%E' \
                 $tool -log -log:basename pinball/$logname $pinopts \
                 -- $prog $args &> record.log
    CILK_NWORKERS=$P /usr/bin/time -f'%E' \
                 $tool -replay -replay:basename pinball/$logname -replay:addr_trans \
                 -- false &> replay.log

    printf "%s\t%s\t%s\n" "$(tail -1 base.log)" "$(tail -1 record.log)" "$(tail -1 replay.log)"

    rm -rf pinball
done

# tmp="$(CILK_NWORKERS=$P $tool -replay -replay:basename pinball/$logname -replay:addr_trans -- false)"
# echo "$tmp"
# replay_time=$(parse "$tmp")
# echo "$replay_time"
