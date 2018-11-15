#!/bin/sh

INPUT_SIZE=4096
BASE_CASE=64

echo "16" | tee sw-sf_result.txt sw-gf_result.txt sw-fj_result.txt
taskset -c 0-15 ./sw-sf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-sf_result.txt
taskset -c 0-15 ./sw-gf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-gf_result.txt
taskset -c 0-15 ./sw-fj -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-fj_result.txt
echo "12" | tee -a sw-sf_result.txt sw-gf_result.txt sw-fj_result.txt
taskset -c 0-11 ./sw-sf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-sf_result.txt
taskset -c 0-11 ./sw-gf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-gf_result.txt
taskset -c 0-11 ./sw-fj -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-fj_result.txt
echo "8" | tee -a sw-sf_result.txt sw-gf_result.txt sw-fj_result.txt
taskset -c 0-7 ./sw-sf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-sf_result.txt
taskset -c 0-7 ./sw-gf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-gf_result.txt
taskset -c 0-7 ./sw-fj -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-fj_result.txt
echo "4" | tee -a sw-sf_result.txt sw-gf_result.txt sw-fj_result.txt
taskset -c 0-3 ./sw-sf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-sf_result.txt
taskset -c 0-3 ./sw-gf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-gf_result.txt
taskset -c 0-3 ./sw-fj -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-fj_result.txt
echo "2" | tee -a sw-sf_result.txt sw-gf_result.txt sw-fj_result.txt
taskset -c 0-1 ./sw-sf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-sf_result.txt
taskset -c 0-1 ./sw-gf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-gf_result.txt
taskset -c 0-1 ./sw-fj -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-fj_result.txt
echo "1" | tee -a sw-sf_result.txt sw-gf_result.txt sw-fj_result.txt
taskset -c 0 ./sw-sf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-sf_result.txt
taskset -c 0 ./sw-gf -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-gf_result.txt
taskset -c 0 ./sw-fj -n $INPUT_SIZE -b $BASE_CASE | tee -a sw-fj_result.txt
