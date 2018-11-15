/*  test_multiple_pipewhiles.cpp                  -*- C++ -*-
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
 * @file test_multiple_pipewhiles.cpp
 *
 * @brief Many pipewhile loops active simultaneously.
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>
#include <cilk/reducer_opadd.h>

#include "cilktest_harness.h"



int compute_expected_sum(int n, int num_reps) {
    int expected_sum = 0;
    // We expect to get (n*\sum_{i=1}^{n})
    for (int i = 0; i < n; ++i) {
        expected_sum += (i+1);
    }
    expected_sum *= num_reps;
    return expected_sum;
}


void test_consecutive_pipewhiles(int n) {
    int sum = 0;
    int expected_sum = compute_expected_sum(n, n);

    for (int q = 0; q < n; ++q) {
        int i = 0;
        CILK_PIPE_WHILE_BEGIN(i < n) {
            sum += (i+1);

            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            CILKTEST_PRINTF(3,
                            "Wkr %p (self=%d): test_pipe body: i = %d, sum=%d\n",
                            w,
                            w ? w->self : -1, 
                            i, sum);
            i++;
            CILK_STAGE_WAIT(1);
            cilk_ms_sleep(1);
            
        } CILK_PIPE_WHILE_END();
        CILKTEST_PRINTF(2, "Sum after iteration %d: %d\n", q, sum);
    }

    TEST_ASSERT(expected_sum == sum);
}


// Declare this reducer globally, to work around a known issue with
// reducers declared in local scope in Clang.
cilk::reducer_opadd<int> spawn_pipewhile_sum(0);
void test_spawn_pipewhiles(int n) {
    int num_reps = 5;
    int expected_sum = compute_expected_sum(n, 3*num_reps);

    spawn_pipewhile_sum.set_value(0);
    
    // Execute n * num_reps pipe_while loops, as n parallel loops of
    // num_reps consecutive loops.

    cilk_spawn [&]() {
        for (int k = 0; k < num_reps; ++k) {
            int i = 0;
            CILK_PIPE_WHILE_BEGIN(i < n) {
                spawn_pipewhile_sum += (i+1);
                __cilkrts_worker *w = __cilkrts_get_tls_worker();
                CILKTEST_PRINTF(3,
                                "Wkr %p (self=%d): test_pipe body: i = %d, sum=%d\n",
                                w,
                                w ? w->self : -1, 
                                i,
                                spawn_pipewhile_sum.get_value());
                i++;
                CILK_STAGE_WAIT(1);
                cilk_ms_sleep(1);
            } CILK_PIPE_WHILE_END();
        }
        CILKTEST_PRINTF(2,
                        "View in spawned iteration: %d\n",
                        spawn_pipewhile_sum.get_value());
    }();


    for (int k = 0; k < num_reps; ++k) {
        int i = 0;
        CILK_PIPE_WHILE_BEGIN(i < n) {
            spawn_pipewhile_sum += (i+1);
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            CILKTEST_PRINTF(3,
                            "Wkr %p (self=%d): test_pipe body: i = %d, sum=%d\n",
                            w,
                            w ? w->self : -1, 
                            i,
                            spawn_pipewhile_sum.get_value());
            i++;
            CILK_STAGE_WAIT(1);
            cilk_ms_sleep(1);
        } CILK_PIPE_WHILE_END();
    }
    CILKTEST_PRINTF(2,
                    "View in continuation: %d\n",
                    spawn_pipewhile_sum.get_value());
    
    cilk_sync;
    
    CILKTEST_PRINTF(2,
                    "View after sync: %d\n",
                    spawn_pipewhile_sum.get_value());
    for (int k = 0; k < num_reps; ++k) {
        int i = 0;
        CILK_PIPE_WHILE_BEGIN(i < n) {
            spawn_pipewhile_sum += (i+1);
            i++;
            CILK_STAGE_WAIT(1);
            cilk_ms_sleep(1);
        } CILK_PIPE_WHILE_END();
    }
    
    CILKTEST_PRINTF(2,
                    "Final view: %d\n",
                    spawn_pipewhile_sum.get_value());
    CILKTEST_PRINTF(2,"Expected sum: %d\n", expected_sum);
    TEST_ASSERT(spawn_pipewhile_sum.get_value() == expected_sum);
}



void test_parallel_pipewhiles(int n) {
    cilk::reducer_opadd<int> sum(0);
    int num_reps = 5;
    int expected_sum = compute_expected_sum(n, n*num_reps);

    // Execute n * num_reps pipe_while loops, as n parallel loops of
    // num_reps consecutive loops.
    cilk_for (int q = 0; q < n; ++q) {
        for (int k = 0; k < num_reps; ++k) {
            int i = 0;
            CILK_PIPE_WHILE_BEGIN(i < n) {
                sum += (i+1);
                __cilkrts_worker *w = __cilkrts_get_tls_worker();
                CILKTEST_PRINTF(3,
                                "Wkr %p (self=%d): test_pipe body: i = %d, sum=%d\n",
                                w,
                                w ? w->self : -1, 
                                i, sum.get_value());
                i++;
                CILK_STAGE_WAIT(1);
                cilk_ms_sleep(1);
            } CILK_PIPE_WHILE_END();
        }
        CILKTEST_PRINTF(2,
                        "View after iteration %d: %d\n", q, sum.get_value());
    }
    TEST_ASSERT(sum.get_value() == expected_sum);
}


int main(void) {
    CILKTEST_BEGIN("multiple_pipewhiles");
    test_consecutive_pipewhiles(10);
    test_spawn_pipewhiles(10);
    test_parallel_pipewhiles(10);
    CILKTEST_END("multiple_pipewhiles");
    return 0;
}

