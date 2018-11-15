/*  test_stage_count.cpp                  -*- C++ -*-
 *
 *  @copyright
 *  Copyright (C) 2013, Intel Corporation
 *  All rights reserved.
 *  
 *  @copyright
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *  
 *  @copyright
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 *  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file test_stage_count.cpp
 *
 * @brief Testing a simple cilk_stage statement.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"


void test_stage_count(int num_stages, int num_iters, int do_rand_sleep)
{
    TEST_ASSERT((num_stages > 0) && (num_iters > 0));

    int num_errors = 0;    
    int* total_count = new int[num_stages * num_iters];
    int* stage_count = new int[num_stages];
    int* expected_stage_count = new int[num_stages];
    int* iter_count = new int[num_iters];
    int* expected_iter_count = new int[num_iters];

    memset(total_count, 0, sizeof(int) * num_stages * num_iters);
    memset(stage_count, 0, sizeof(int) * num_stages);
    memset(expected_stage_count, 0, sizeof(int) * num_stages);
    memset(iter_count, 0, sizeof(int) * num_iters);
    memset(expected_iter_count, 0, sizeof(int) * num_iters);
    
    int loop_i = 0;

    CILKTEST_PRINTF(3,
                    "## Testing stage count: num_stages = %d, num_iters = %d, do_rand_sleep = %d\n",
                    num_stages, num_iters, do_rand_sleep);
    CILK_PIPE_WHILE_BEGIN_THROTTLED(loop_i < num_iters, 6) {
        int s = 1;
        int i = loop_i;
        loop_i++;
        CILK_STAGE_WAIT(s);

        while (s <= num_stages) {
            if (do_rand_sleep) {
                unsigned sleep_time = (s* (i+1)) % 4;
                if (sleep_time >= 4) {
                    CILKTEST_PRINTF(0, "ERROR: bug in sleep time\n");
                    exit(1);
                }
                cilk_ms_sleep(sleep_time);
            }
            stage_count[s-1] += s;
            iter_count[i] +=  (i+1);
            
            CILKTEST_PRINTF(4,
                            "Here: total_count[%d] updated by %d * %d. previous val was %d\n",
                            num_stages * i + (s-1),
                            s, (i+1),
                            total_count[num_stages * i + (s-1)]);
            
            total_count[num_stages * i + (s-1)] += s *(i+1);

            CILKTEST_PRINTF(4,
                            "Here: total_count[%d] updated by %d * %d. new val is %d\n",
                            num_stages * i + (s-1),
                            s, (i+1),
                            total_count[num_stages * i + (s-1)]);

            ++s;
            
            CILK_STAGE_WAIT(s);
        }
    } CILK_PIPE_WHILE_END();

    CILKTEST_PRINTF(3,
                    "## Checking execution values: num_stages = %d, num_iters = %d, do_rand_sleep=%d\n",
                    num_stages, num_iters, do_rand_sleep);

    for (int i = 0; i < num_iters; ++i) {
        for (int s = 1; s <= num_stages; ++s) {

            CILKTEST_PRINTF(4,
                            "Testing i=%d, s=%d. total_count = %d, expected_stage = %d, expected_iter = %d\n",
                            i, s, total_count[num_stages * i + (s-1)],
                            expected_stage_count[s-1],
                            expected_iter_count[i]);
            
            // Verify each stage executed.

            TEST_ASSERT(s *(i+1) == total_count[num_stages * i + (s-1)]);
            if ( s *(i+1) != total_count[num_stages * i + (s-1)] ) {
                CILKTEST_PRINTF(1,
                                "ERROR: incorrect execution of (i=%d, s=%d). idx = %d, Got %d, expected %d\n",
                                i, s,
                                num_stages * i + (s-1),
                                total_count[num_stages * i + (s-1)], s*(i+1));
                num_errors++;
            }
            expected_stage_count[s-1] += s;
            expected_iter_count[i] += (i+1);
        }
    }


    for (int i = 0; i < num_iters; ++i) {
        TEST_ASSERT(expected_iter_count[i] == iter_count[i]);
        if (expected_iter_count[i] != iter_count[i]) {
            CILKTEST_PRINTF(3, "ERROR: iter_count[%d] = %d, expected is %d\n",
                            i, iter_count[i], expected_iter_count[i]);
            num_errors++;
        }
    }


    for (int s = 1; s <= num_stages; ++s) {
        TEST_ASSERT(expected_stage_count[s-1] == stage_count[s-1]);
    }

    TEST_ASSERT(0 == num_errors);
    delete[] total_count;
    delete[] stage_count;
    delete[] iter_count;
    delete[] expected_stage_count;
    delete[] expected_iter_count;
}



int main(int argc, char* argv[]) {
    CILKTEST_BEGIN("stage_count");
    int P = __cilkrts_get_nworkers();
    if (argc >= 3) {
        int S = atoi(argv[1]);
        int N = atoi(argv[2]);
        CILKTEST_PRINTF(2, "Testing stage count, P = %d, num_stages=%d, num_iters=%d\n",
                        P,
                        S,
                        N);
        test_stage_count(S, N, 0);
        test_stage_count(S, N, 1);
    }
    else {   
        int max_num_stages = 128;
        int max_num_iters = 2048;
        CILKTEST_PRINTF(2, "Testing stage count... P = %d\n", P);
        for (int do_rand_sleep = 0; do_rand_sleep <= 1; ++do_rand_sleep) {
            int I = 1;
            while (I <= max_num_iters) {
                if ((do_rand_sleep) && (I > 128)) {
                    break;
                } 
                CILKTEST_PRINTF(2, "## test_stage_count: do_rand_sleep=%d, I=%d\n", do_rand_sleep, I);
                int S = 1;
                while (S <= max_num_stages) {
                    test_stage_count(S, I, do_rand_sleep);
                    S *= 2;
                }
                I *= 2;
            }
        }
    }
    CILKTEST_END("stage_count");
    return 0;
}
