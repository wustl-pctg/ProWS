echo "16" | tee hw-sf_result.txt hw-gf_result.txt hw-fj_result.txt
for i in $(seq 1 10); do
taskset -c 0-15 ./hw-sf ../data/test.avi 104 16 | tee -a hw-sf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-15 ./hw-gf ../data/test.avi 104 16 | tee -a hw-gf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-15 ./hw-fj ../data/test.avi 104 16 | tee -a hw-fj_result.txt
done
echo "12" | tee -a hw-sf_result.txt hw-gf_result.txt hw-fj_result.txt
for i in $(seq 1 10); do
taskset -c 0-11 ./hw-sf ../data/test.avi 104 12 | tee -a hw-sf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-11 ./hw-gf ../data/test.avi 104 12 | tee -a hw-gf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-11 ./hw-fj ../data/test.avi 104 12 | tee -a hw-fj_result.txt
done
echo "8" | tee -a hw-sf_result.txt hw-gf_result.txt hw-fj_result.txt
for i in $(seq 1 10); do
taskset -c 0-7 ./hw-sf ../data/test.avi 104 8 | tee -a hw-sf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-7 ./hw-gf ../data/test.avi 104 8 | tee -a hw-gf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-7 ./hw-fj ../data/test.avi 104 8 | tee -a hw-fj_result.txt
done
echo "4" | tee -a hw-sf_result.txt hw-gf_result.txt hw-fj_result.txt
for i in $(seq 1 10); do
taskset -c 0-3 ./hw-sf ../data/test.avi 104 4 | tee -a hw-sf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-3 ./hw-gf ../data/test.avi 104 4 | tee -a hw-gf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-3 ./hw-fj ../data/test.avi 104 4 | tee -a hw-fj_result.txt
done
echo "2" | tee -a hw-sf_result.txt hw-gf_result.txt hw-fj_result.txt
for i in $(seq 1 10); do
taskset -c 0-1 ./hw-sf ../data/test.avi 104 2 | tee -a hw-sf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-1 ./hw-gf ../data/test.avi 104 2 | tee -a hw-gf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-1 ./hw-fj ../data/test.avi 104 2 | tee -a hw-fj_result.txt
done
echo "1" | tee -a hw-sf_result.txt hw-gf_result.txt hw-fj_result.txt
for i in $(seq 1 10); do
taskset -c 0 ./hw-sf ../data/test.avi 104 1 | tee -a hw-sf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0 ./hw-gf ../data/test.avi 104 1 | tee -a hw-gf_result.txt
done
for i in $(seq 1 10); do
taskset -c 0 ./hw-fj ../data/test.avi 104 1 | tee -a hw-fj_result.txt
done
