#!/bin/bash


declare -A dirs args cmdnames makecmds

dirs["fib"]=micro
args["fib"]="38"
cmdnames["fib"]="fib"
makecmds["fib"]="make fib"

dirs["fib1"]=micro
args["fib1"]="30"
cmdnames["fib1"]="fib"
makecmds["fib1"]="make fib"

dirs["fib2"]=micro
args["fib2"]="35"
cmdnames["fib2"]="fib"
makecmds["fib2"]="make fib"


dirs["cbt"]=micro
args["cbt"]=""
cmdnames["cbt"]="cbt"
makecmds["cbt"]="make cbt"

dirs["cilkfor"]=micro
args["cilkfor"]="50000000"
cmdnames["cilkfor"]="cilkfor"
makecmds["cilkfor"]="make cilkfor"

dirs["cilkfor1"]=micro
args["cilkfor1"]="50000000 100"
cmdnames["cilkfor1"]="cilkfor"
makecmds["cilkfor1"]="make cilkfor"

dirs["cilkfor2"]=micro
args["cilkfor2"]="50000000 100000"
cmdnames["cilkfor2"]="cilkfor"
makecmds["cilkfor2"]="make cilkfor"

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
makecmds["chess"]="make CILK=1 PORR=1 -j"

dirs["dict"]=${pbbsdir}/dictionary/lockingHash
args["dict"]="-r 1 ../sequenceData/data/randomSeq_10M_100K_int "
cmdnames["dict"]="dict"
makecmds["dict"]="make CILK=1 PORR=1 -j"

dirs["MIS1"]=${pbbsdir}/maximalIndependentSet/lockingMIS
args["MIS1"]="-r 1 ../graphData/data/randLocalGraph_J_5_10000"
makecmds["MIS1"]="make CILK=1 PORR=1 -j"
cmdnames["MIS1"]="MIS"

dirs["MIS2"]=${pbbsdir}/maximalIndependentSet/lockingMIS
args["MIS2"]="-r 1 ../graphData/data/randLocalGraph_J_5_100000"
makecmds["MIS2"]="make CILK=1 PORR=1 -j"
cmdnames["MIS2"]="MIS"

dirs["MIS3"]=${pbbsdir}/maximalIndependentSet/lockingMIS
args["MIS3"]="-r 1 ../graphData/data/randLocalGraph_J_5_10000000"
makecmds["MIS3"]="make CILK=1 PORR=1 -j"
cmdnames["MIS3"]="MIS"

dirs["MIS"]=${pbbsdir}/maximalIndependentSet/lockingMIS
args["MIS"]="-r 1 ../graphData/data/randLocalGraph_J_5_5000000"
makecmds["MIS"]="make CILK=1 PORR=1 -j"
cmdnames["MIS"]="MIS"

dirs["matching"]=${pbbsdir}/maximalMatching/lockingMatching
# args["matching"]="-r 1 ../graphData/data/randLocalGraph_E_5_5000000"
args["matching"]="-r 1 ../graphData/data/randLocalGraph_E_5_1000000"
makecmds["matching"]="make CILK=1 PORR=1 -j"
cmdnames["matching"]="matching"

dirs["BFS"]=${pbbsdir}/breadthFirstSearch/lockingBFS
args["BFS"]="-r 1 ../graphData/data/randLocalGraph_J_5_5000000"
# args["BFS"]="-r 1 ../graphData/data/randLocalGraph_J_5_100000"
makecmds["BFS"]="make CILK=1 PORR=1 -j"
cmdnames["BFS"]="BFS"

dirs["refine"]=${pbbsdir}/delaunayRefine/lockingRefine
args["refine"]="-r 1 ../geometryData/data/2DinCubeDelaunay_1000000"
# args["refine"]="-r 1 ../geometryData/data/2DinCubeDelaunay_10000"
makecmds["refine"]="make CILK=1 PORR=1 -j"
cmdnames["refine"]="refine"
