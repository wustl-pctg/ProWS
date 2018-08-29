#echo 1
#for i in $(seq 1 3); do
#taskset -c 0 ./run.sh cilk-future native cilk-future-1-native.out 1 4
#done
echo 2
for i in $(seq 1 3); do
taskset -c 0-1 ./run.sh cilk-future native cilk-future-2-native.out 2 8
done
echo 4
for i in $(seq 1 3); do
taskset -c 0-3 ./run.sh cilk-future native cilk-future-4-native.out 4 16
done
echo 8
for i in $(seq 1 3); do
taskset -c 0-7 ./run.sh cilk-future native cilk-future-8-native.out 8 32 
done
echo 12
for i in $(seq 1 3); do
taskset -c 0-11 ./run.sh cilk-future native cilk-future-16-native.out 12 48
done
echo 16
for i in $(seq 1 3); do
taskset -c 0-15 ./run.sh cilk-future native cilk-future-16-native.out 16 64
done
