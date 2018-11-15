/*  test_nested_chain.cpp                  -*- C++ -*-
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
 * @file test_nested_chain.cpp
 *
 * @brief A test with some simple nested parallelism + call chain of
 * Cilk function.
 */

#include <cstdio>
#include <cstdlib>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "sequencer.h"



// This test is checking that iteration 1 gets resumed after stalling
// on stage 1 of iteration 0, even though iteration 0 stage 2 is busy
// working in a nested function that spawns.

int test_nested_chain(int n) {
    Sequencer Q;
    int i = 0;
    
    CILK_PIPE_WHILE_BEGIN(i < 2) {
        int loop_i = i;

        // Number of events that happen in each iteration of the loop.
        const int C = 8;
        ++i;

        // Keep looping, executing 2 stages in each iteration.
        for (int rep_count = 0; rep_count < n; ++rep_count) {
            
            if (loop_i == 0) {
                // Odd stage.
                CILK_STAGE_WAIT(2*rep_count + 1);

                // Event 1:
                Q.wait_for_num(C*rep_count+1, 50, "iter 0 starting stage 1, delaying for iter 1 to stall");
                
                // Even stage.
                CILK_STAGE_WAIT(2*rep_count+2);

                // Stage 2:
                cilk_spawn [&Q, C, rep_count]() {
                    Q.wait_for_num(C*rep_count + 3, 100, "iter 0 stage 2 starting spawned function");
                    // We want event 4 from iteration 1 to interleave
                    // here.
                    Q.wait_for_num(C*rep_count + 5, 0, "iter 0 stage 2 finishing spawned function");
                }();

                // Continuation.
                // 
                // Event 2: Force the continuation to get stolen
                // first.
                Q.wait_for_num(C*rep_count+2, 0, "iter 0 stage 2, continuation stolen");
                cilk_sync;

                // Event 6.  Finish iteration 0.
                Q.wait_for_num(C*rep_count + 6, 2, "iter 0, finishing stage 2");
                
            }
            else {
                TEST_ASSERT(1 == loop_i);
                // Event 0: This should not wait, so that we stall
                // immediately.
                Q.wait_for_num(C*rep_count, 0);

                // Odd stage. Stage 1 on iteration 1 should stall on
                // the wait because iteration 0 delays in finishing
                // stage 1 yet.
                CILK_STAGE_WAIT(2*rep_count + 1);

                // Event 4.  We want this operation to interleave in
                // between Events 3 and 5 on iteration 0.
                Q.wait_for_num(C*rep_count+4, 0, "iter 1 stage 2 interleaved while spawned function in iter 1, stage 2 executing");

                CILK_STAGE_WAIT(2*rep_count + 2);
                
                // Event 7.  Finish iteration 1.
                Q.wait_for_num(C*rep_count + 7, 2, "iter 1, finishing stage 2");
            }
        }        
    } CILK_PIPE_WHILE_END();
    return 0;
}


int main(void) {
    CILKTEST_BEGIN("nested_chain");
    int code = __cilkrts_set_param("nworkers","3");
    TEST_ASSERT(0 == code);

    CILKTEST_PRINTF(2, "Testing nested chain on P = %d\n", __cilkrts_get_nworkers());

    const int L = 3;
    int testN[L] = {1, 2, 10};
    for (int i = 0; i < L; ++i)
    {
        int n = testN[i];
        CILKTEST_PRINTF(3, "## Trying n = %d\n", n);
        test_nested_chain(n);
    }

    CILKTEST_END("nested_chain");
    return 0;
}

