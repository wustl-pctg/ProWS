#!/bin/bash


declare -A dirs args cmdnames makecmds

dirs["fib"]=micro
args["fib"]="30"
cmdnames["fib"]="fib"
makecmds["fib"]="make fib"

dirs["cbt"]=micro
args["cbt"]=""
cmdnames["cbt"]="cbt"
makecmds["cbt"]="make cbt"

dirs["cilkfor"]=micro
args["cilkfor"]="10000000"
cmdnames["cilkfor"]="cilkfor"
makecmds["cilkfor"]="make cilkfor"

dirs["dedup"]=dedup
args["dedup"]="large"
cmdnames["dedup"]="run.sh lock"
makecmds["dedup"]="make"
                  
dirs["ferret"]=ferret
args["ferret"]="large"
cmdnames["ferret"]="run.sh lock"
makecmds["ferret"]="make"

pbbsdir=$HOME/devel/cilkplus-tests/pbbs
dirs["chess"]=${pbbsdir}/../chess
args["chess"]=""
cmdnames["chess"]="chess-cover-locking"
makecmds["chess"]="make CILK=1 CILKRR=1 -j"

dirs["dict"]=${pbbsdir}/dictionary/lockingHash
args["dict"]="-r 1 ../sequenceData/data/randomSeq_10M_100K_int "
cmdnames["dict"]="dict"
makecmds["dict"]="make CILK=1 CILKRR=1 -j"

dirs["MIS"]=${pbbsdir}/maximalIndependentSet/lockingMIS
args["MIS"]="-r 1 ../graphData/data/randLocalGraph_J_5_5000000"
# args["MIS"]="-r 1 ../graphData/data/randLocalGraph_J_5_100000"
makecmds["MIS"]="make CILK=1 CILKRR=1 -j"
cmdnames["MIS"]="MIS"

dirs["matching"]=${pbbsdir}/maximalMatching/lockingMatching
args["matching"]="-r 1 ../graphData/data/randLocalGraph_E_5_5000000"
# args["matching"]="-r 1 ../graphData/data/randLocalGraph_E_5_100000"
makecmds["matching"]="make CILK=1 CILKRR=1 -j"
cmdnames["matching"]="matching"

dirs["BFS"]=${pbbsdir}/breadthFirstSearch/lockingBFS
args["BFS"]="-r 1 ../graphData/data/randLocalGraph_J_5_5000000"
# args["BFS"]="-r 1 ../graphData/data/randLocalGraph_J_5_100000"
makecmds["BFS"]="make CILK=1 CILKRR=1 -j"
cmdnames["BFS"]="BFS"

dirs["refine"]=${pbbsdir}/delaunayRefine/lockingRefine
args["refine"]="-r 1 ../geometryData/data/2DinCubeDelaunay_1000000"
# args["refine"]="-r 1 ../geometryData/data/2DinCubeDelaunay_10000"
makecmds["refine"]="make CILK=1 CILKRR=1 -j"
cmdnames["refine"]="refine"
