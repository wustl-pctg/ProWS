#!/bin/sh

echo "16" | tee matmul_z-unstructured_result.txt 
for i in $(seq 1 10); do
taskset -c 0-15 ./matmul_z-unstructured -n 4096 -b 32  | tee -a matmul_z-unstructured_result.txt
done
echo "12" | tee -a  matmul_z-unstructured_result.txt 
for i in $(seq 1 10); do
taskset -c 0-11 ./matmul_z-unstructured -n 4096 -b 32  | tee -a matmul_z-unstructured_result.txt
done
echo "8" | tee -a  matmul_z-unstructured_result.txt 
for i in $(seq 1 10); do
taskset -c 0-7 ./matmul_z-unstructured -n 4096 -b 32  | tee -a matmul_z-unstructured_result.txt
done
echo "4" | tee -a  matmul_z-unstructured_result.txt 
for i in $(seq 1 10); do
taskset -c 0-3 ./matmul_z-unstructured -n 4096 -b 32  | tee -a matmul_z-unstructured_result.txt
done
echo "2" | tee -a  matmul_z-unstructured_result.txt 
for i in $(seq 1 10); do
taskset -c 0-1 ./matmul_z-unstructured -n 4096 -b 32  | tee -a matmul_z-unstructured_result.txt
done
echo "1" | tee -a  matmul_z-unstructured_result.txt 
for i in $(seq 1 10); do
taskset -c 0 ./matmul_z-unstructured -n 4096 -b 32  | tee -a matmul_z-unstructured_result.txt
done
