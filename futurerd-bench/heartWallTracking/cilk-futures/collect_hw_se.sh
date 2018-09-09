date | tee hw-se_result.txt
for i in $(seq 1 10); do
  ./hw-se ../data/test.avi 104 1 | tee -a hw-se_result.txt
done
