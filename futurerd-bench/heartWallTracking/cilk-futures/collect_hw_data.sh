echo "16" | tee hw-structured_result.txt hw-unstructured_result.txt hw-spawn_result.txt
for i in $(seq 1 10); do
taskset -c 0-15 ./hw-structured ../data/test.avi 104 16 | tee -a hw-structured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-15 ./hw-unstructured ../data/test.avi 104 16 | tee -a hw-unstructured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-15 ./hw-spawn ../data/test.avi 104 16 | tee -a hw-spawn_result.txt
done
echo "12" | tee -a hw-structured_result.txt hw-unstructured_result.txt hw-spawn_result.txt
for i in $(seq 1 10); do
taskset -c 0-11 ./hw-structured ../data/test.avi 104 12 | tee -a hw-structured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-11 ./hw-unstructured ../data/test.avi 104 12 | tee -a hw-unstructured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-11 ./hw-spawn ../data/test.avi 104 12 | tee -a hw-spawn_result.txt
done
echo "8" | tee -a hw-structured_result.txt hw-unstructured_result.txt hw-spawn_result.txt
for i in $(seq 1 10); do
taskset -c 0-7 ./hw-structured ../data/test.avi 104 8 | tee -a hw-structured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-7 ./hw-unstructured ../data/test.avi 104 8 | tee -a hw-unstructured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-7 ./hw-spawn ../data/test.avi 104 8 | tee -a hw-spawn_result.txt
done
echo "4" | tee -a hw-structured_result.txt hw-unstructured_result.txt hw-spawn_result.txt
for i in $(seq 1 10); do
taskset -c 0-3 ./hw-structured ../data/test.avi 104 4 | tee -a hw-structured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-3 ./hw-unstructured ../data/test.avi 104 4 | tee -a hw-unstructured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-3 ./hw-spawn ../data/test.avi 104 4 | tee -a hw-spawn_result.txt
done
echo "2" | tee -a hw-structured_result.txt hw-unstructured_result.txt hw-spawn_result.txt
for i in $(seq 1 10); do
taskset -c 0-1 ./hw-structured ../data/test.avi 104 2 | tee -a hw-structured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-1 ./hw-unstructured ../data/test.avi 104 2 | tee -a hw-unstructured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0-1 ./hw-spawn ../data/test.avi 104 2 | tee -a hw-spawn_result.txt
done
echo "1" | tee -a hw-structured_result.txt hw-unstructured_result.txt hw-spawn_result.txt
for i in $(seq 1 10); do
taskset -c 0 ./hw-structured ../data/test.avi 104 1 | tee -a hw-structured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0 ./hw-unstructured ../data/test.avi 104 1 | tee -a hw-unstructured_result.txt
done
for i in $(seq 1 10); do
taskset -c 0 ./hw-spawn ../data/test.avi 104 1 | tee -a hw-spawn_result.txt
done
