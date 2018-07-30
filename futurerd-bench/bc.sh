#!/bin/bash
set -e

PROGS=(matmul_z)
BTYPES=(base reach)
BCASES=(32 64 128)
ITER=3
SEP=","

SIZES=([lcs]="32"
      [sw]="32"
      [matmul_z]="512"
      )

function basecases {
    pushd basic >/dev/null
    rm -f bc.out.log bc.times.csv
    for bench in ${PROGS[@]}; do
        for bc in ${BCASES[@]}; do
            for btype in ${BTYPES[@]}; do
                total=0
                for i in $(seq $ITER); do
                    run="./${bench}-${btype} 2>&1 -n ${SIZES[$bench]} -r ${bc}"
                    echo "$run"
                    tmp=$(eval $run)
                    echo "$tmp" >> bc.out.log

                    if [[ "$?" -ne 0 ]]; then
                        echo "Execution failed:"
                        echo "$tmp";
                        exit 1;
                    fi

                    gather="grep 'Benchmark time' | cut -d ':' -f 2 | cut -d ' ' -f 2"
                    result=$(eval "echo \"$tmp\" | $gather")
                    total=$(echo "scale=2; $total + $result" | bc)
                done
                avg=$(echo "scale=2; $total / $ITER" | bc)
                outstr="${bench}${SEP}${SIZES[$bench]}${SEP}"
                outstr="${outstr}${bc}${SEP}${btype}${SEP}%.2f${SEP}\n"
                printf "$outstr" ${avg} >> bc.times.csv # Use %.2f in outstr

            done
        done
    done
    popd >/dev/null
}


source remake.sh
system release
rm -f matmul.struct.log matmul.nonblock.log

basic release structured structured
basecases
mv basic/bc.times.csv ./bc.struct.log

basic release nonblock structured
basecases
mv basic/bc.times.csv ./bc.nonblock.log

printf -- "----- Structured -----"
cat bc.struct.log
printf -- "----- Nonblock -----"
cat bc.nonblock.log
