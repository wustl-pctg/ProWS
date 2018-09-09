
echo 1 | tee -a merge-spawn_result.txt merge-unstructured_result.txt merge-spawn_steals.txt merge-unstructured_steals.txt

  for i in $(seq 1 10); do
  ./merge-spawn -s1 8000000 -s2 4000000 | tee -a merge-spawn_result.txt
  done
  
  for i in $(seq 1 10); do
  ./merge-unstructured -s1 8000000 -s2 4000000 | tee -a merge-unstructured_result.txt
  done
