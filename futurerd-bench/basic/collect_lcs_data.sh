#!/bin/sh

echo "16" | tee -a lcs-structured_result.txt lcs-unstructured_result.txt lcs-cilk-for_result.txt
taskset -c 0-15 ./lcs-structured -n 32768 -b 512 | tee -a lcs-structured_result.txt
taskset -c 0-15 ./lcs-unstructured -n 32768 -b 512 | tee -a lcs-unstructured_result.txt
taskset -c 0-15 ./lcs-cilk-for -n 32768 -b 512 | tee -a lcs-cilk-for_result.txt
echo "12" | tee -a lcs-structured_result.txt lcs-unstructured_result.txt lcs-cilk-for_result.txt
taskset -c 0-11 ./lcs-structured -n 32768 -b 512 | tee -a lcs-structured_result.txt
taskset -c 0-11 ./lcs-unstructured -n 32768 -b 512 | tee -a lcs-unstructured_result.txt
taskset -c 0-11 ./lcs-cilk-for -n 32768 -b 512 | tee -a lcs-cilk-for_result.txt
echo "8" | tee -a lcs-structured_result.txt lcs-unstructured_result.txt lcs-cilk-for_result.txt
taskset -c 0-7 ./lcs-structured -n 32768 -b 512 | tee -a lcs-structured_result.txt
taskset -c 0-7 ./lcs-unstructured -n 32768 -b 512 | tee -a lcs-unstructured_result.txt
taskset -c 0-7 ./lcs-cilk-for -n 32768 -b 512 | tee -a lcs-cilk-for_result.txt
echo "4" | tee -a lcs-structured_result.txt lcs-unstructured_result.txt lcs-cilk-for_result.txt
taskset -c 0-3 ./lcs-structured -n 32768 -b 512 | tee -a lcs-structured_result.txt
taskset -c 0-3 ./lcs-unstructured -n 32768 -b 512 | tee -a lcs-unstructured_result.txt
taskset -c 0-3 ./lcs-cilk-for -n 32768 -b 512 | tee -a lcs-cilk-for_result.txt
echo "2" | tee -a lcs-structured_result.txt lcs-unstructured_result.txt lcs-cilk-for_result.txt
taskset -c 0-1 ./lcs-structured -n 32768 -b 512 | tee -a lcs-structured_result.txt
taskset -c 0-1 ./lcs-unstructured -n 32768 -b 512 | tee -a lcs-unstructured_result.txt
taskset -c 0-1 ./lcs-cilk-for -n 32768 -b 512 | tee -a lcs-cilk-for_result.txt
echo "1" | tee -a lcs-structured_result.txt lcs-unstructured_result.txt lcs-cilk-for_result.txt
taskset -c 0 ./lcs-structured -n 32768 -b 512 | tee -a lcs-structured_result.txt
taskset -c 0 ./lcs-unstructured -n 32768 -b 512 | tee -a lcs-unstructured_result.txt
taskset -c 0 ./lcs-cilk-for -n 32768 -b 512 | tee -a lcs-cilk-for_result.txt
