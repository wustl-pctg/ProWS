/*  test_nested_worker_switch.cpp                  -*- C++ -*-
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
 * @file test_nested_worker_switch.cpp
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



// To force initialization of runtime.
void no_op() { }


// This test is checking that iteration 1 gets resumed after stalling
// on stage 1 of iteration 0, even though iteration 0 stage 2 is busy
// working in a nested function that spawns.
int test_nested_worker_switch() {
    Sequencer Q;
    int i = 0;
    
    CILK_PIPE_WHILE_BEGIN(i < 2) {
        int loop_i = i;

        // Force initialization of __cilkrts_stack_frame and push onto
        // the worker's call chain.  This push makes the test more
        // interesting.
        cilk_spawn no_op();
        cilk_sync;
        
        // Number of events that happen in each iteration of the loop.
        ++i;


        if (loop_i == 0) {
            CILK_STAGE_WAIT(1);

            Q.wait_for_num(1, 100, "Iter 0, stage 1, before switch");
            // Worker switch -- consumes points 3 through 6.

            {
                __cilkrts_worker *start_w = __cilkrts_get_tls_worker();
                CILKTEST_PRINTF(2,
                                "W=%d: worker switch start\n",
                                start_w->self);

                cilk_spawn [&Q]() {
                    Q.wait_for_num(3, 10, "wkr switch: spawn function start");
                }();

                Q.wait_for_num(2, 0, "wkr switch: continuation steal");
                Q.wait_for_num(4, 100, "wkr switch: continuation delay");

                cilk_sync;
                __cilkrts_worker *finish_w = __cilkrts_get_tls_worker();
                CILKTEST_PRINTF(2,
                                "W=%d; worker switch finish\n",
                                finish_w->self);
                Q.wait_for_num(5, 0, "wkr switch: finish switch");
            }

            Q.wait_for_num(6, 0, "Iter 0, stage 1, after switch");
            
            CILK_STAGE_WAIT(2);
            
            Q.wait_for_num(7, 200, "Iter 0, stage 2, after switch");

            Q.wait_for_num(8, 0, "Iter 0, finishing stage 2");
        }
        else {
            TEST_ASSERT(1 == loop_i);
            // Event 0: This should not wait, so that we stall
            // immediately.
            Q.wait_for_num(0,  0, "Iter 1, stage 0 start");

            {
                __cilkrts_worker* stage0_wkr = __cilkrts_get_tls_worker();
                CILKTEST_PRINTF(2,
                                "Iter 1, stage 0: should suspend on wkr %p (%d)\n",
                                stage0_wkr,
                                stage0_wkr->self);
            }

            CILK_STAGE_WAIT(1);

            // Resume, hopefully on a different worker.
            {
                __cilkrts_worker* stage1_wkr = __cilkrts_get_tls_worker();
                CILKTEST_PRINTF(2,
                                "Iter 1, stage 1: starting on wkr %p (%d)\n",
                                stage1_wkr,
                                stage1_wkr->self);
            }
        }
    } CILK_PIPE_WHILE_END();
    return 0;
}


int main(void) {
    CILKTEST_BEGIN("nested_worker_switch");

    int code = __cilkrts_set_param("nworkers","2");
    TEST_ASSERT(0 == code);
    
    CILKTEST_PRINTF(2, "Testing nested worker switch on P = %d\n", __cilkrts_get_nworkers());

    test_nested_worker_switch();

    CILKTEST_END("nested_worker_switch");
    return 0;
}

