echo 1
for i in $(seq 1 10); do
taskset -c 0 ./run.sh piper native piper-1-native.out 1 8
done
echo 2
for i in $(seq 1 10); do
taskset -c 0-1 ./run.sh piper native piper-2-native.out 2 16
done
echo 4
for i in $(seq 1 10); do
taskset -c 0-3 ./run.sh piper native piper-4-native.out 4 32
done
echo 8
for i in $(seq 1 10); do
taskset -c 0-7 ./run.sh piper native piper-8-native.out 8 64 
done
echo 12
for i in $(seq 1 10); do
taskset -c 0-11 ./run.sh piper native piper-12-native.out 12 96
done
echo 16
for i in $(seq 1 10); do
taskset -c 0-15 ./run.sh piper native piper-16-native.out 16 128
done
#taskset -c 0-19 ./run.sh piper native piper-20-native.out 20 80
#taskset -c 0-23 ./run.sh piper native piper-24-native.out 24 96
#taskset -c 0-27 ./run.sh piper native piper-28-native.out 28 112
#taskset -c 0-31 ./run.sh piper native piper-32-native.out 32 128
