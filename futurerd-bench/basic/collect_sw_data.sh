#!/bin/sh

echo "16" | tee sw-structured_result.txt sw-unstructured_result.txt sw-cilk-for_result.txt
taskset -c 0-15 ./sw-structured -n 2048 | tee -a sw-structured_result.txt
taskset -c 0-15 ./sw-unstructured -n 2048 | tee -a sw-unstructured_result.txt
taskset -c 0-15 ./sw-cilk-for -n 2048 | tee -a sw-cilk-for_result.txt
echo "12" | tee -a sw-structured_result.txt sw-unstructured_result.txt sw-cilk-for_result.txt
taskset -c 0-11 ./sw-structured -n 2048 | tee -a sw-structured_result.txt
taskset -c 0-11 ./sw-unstructured -n 2048 | tee -a sw-unstructured_result.txt
taskset -c 0-11 ./sw-cilk-for -n 2048 | tee -a sw-cilk-for_result.txt
echo "8" | tee -a sw-structured_result.txt sw-unstructured_result.txt sw-cilk-for_result.txt
taskset -c 0-7 ./sw-structured -n 2048 | tee -a sw-structured_result.txt
taskset -c 0-7 ./sw-unstructured -n 2048 | tee -a sw-unstructured_result.txt
taskset -c 0-7 ./sw-cilk-for -n 2048 | tee -a sw-cilk-for_result.txt
echo "4" | tee -a sw-structured_result.txt sw-unstructured_result.txt sw-cilk-for_result.txt
taskset -c 0-3 ./sw-structured -n 2048 | tee -a sw-structured_result.txt
taskset -c 0-3 ./sw-unstructured -n 2048 | tee -a sw-unstructured_result.txt
taskset -c 0-3 ./sw-cilk-for -n 2048 | tee -a sw-cilk-for_result.txt
echo "2" | tee -a sw-structured_result.txt sw-unstructured_result.txt sw-cilk-for_result.txt
taskset -c 0-1 ./sw-structured -n 2048 | tee -a sw-structured_result.txt
taskset -c 0-1 ./sw-unstructured -n 2048 | tee -a sw-unstructured_result.txt
taskset -c 0-1 ./sw-cilk-for -n 2048 | tee -a sw-cilk-for_result.txt
echo "1" | tee -a sw-structured_result.txt sw-unstructured_result.txt sw-cilk-for_result.txt
taskset -c 0 ./sw-structured -n 2048 | tee -a sw-structured_result.txt
taskset -c 0 ./sw-unstructured -n 2048 | tee -a sw-unstructured_result.txt
taskset -c 0 ./sw-cilk-for -n 2048 | tee -a sw-cilk-for_result.txt
