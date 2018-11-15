#!/bin/sh

echo "16" | tee -a lcs-sf_result.txt lcs-gf_result.txt lcs-fj_result.txt
taskset -c 0-15 ./lcs-sf -n 32768 -b 512 | tee -a lcs-sf_result.txt
taskset -c 0-15 ./lcs-gf -n 32768 -b 512 | tee -a lcs-gf_result.txt
taskset -c 0-15 ./lcs-fj -n 32768 -b 512 | tee -a lcs-fj_result.txt
echo "12" | tee -a lcs-sf_result.txt lcs-gf_result.txt lcs-fj_result.txt
taskset -c 0-11 ./lcs-sf -n 32768 -b 512 | tee -a lcs-sf_result.txt
taskset -c 0-11 ./lcs-gf -n 32768 -b 512 | tee -a lcs-gf_result.txt
taskset -c 0-11 ./lcs-fj -n 32768 -b 512 | tee -a lcs-fj_result.txt
echo "8" | tee -a lcs-sf_result.txt lcs-gf_result.txt lcs-fj_result.txt
taskset -c 0-7 ./lcs-sf -n 32768 -b 512 | tee -a lcs-sf_result.txt
taskset -c 0-7 ./lcs-gf -n 32768 -b 512 | tee -a lcs-gf_result.txt
taskset -c 0-7 ./lcs-fj -n 32768 -b 512 | tee -a lcs-fj_result.txt
echo "4" | tee -a lcs-sf_result.txt lcs-gf_result.txt lcs-fj_result.txt
taskset -c 0-3 ./lcs-sf -n 32768 -b 512 | tee -a lcs-sf_result.txt
taskset -c 0-3 ./lcs-gf -n 32768 -b 512 | tee -a lcs-gf_result.txt
taskset -c 0-3 ./lcs-fj -n 32768 -b 512 | tee -a lcs-fj_result.txt
echo "2" | tee -a lcs-sf_result.txt lcs-gf_result.txt lcs-fj_result.txt
taskset -c 0-1 ./lcs-sf -n 32768 -b 512 | tee -a lcs-sf_result.txt
taskset -c 0-1 ./lcs-gf -n 32768 -b 512 | tee -a lcs-gf_result.txt
taskset -c 0-1 ./lcs-fj -n 32768 -b 512 | tee -a lcs-fj_result.txt
echo "1" | tee -a lcs-sf_result.txt lcs-gf_result.txt lcs-fj_result.txt
taskset -c 0 ./lcs-sf -n 32768 -b 512 | tee -a lcs-sf_result.txt
taskset -c 0 ./lcs-gf -n 32768 -b 512 | tee -a lcs-gf_result.txt
taskset -c 0 ./lcs-fj -n 32768 -b 512 | tee -a lcs-fj_result.txt
