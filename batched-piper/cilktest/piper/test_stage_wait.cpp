/*  test_stage_wait.cpp                  -*- C++ -*-
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
 * @file test_stage_wait.cpp
 *
 * @brief Testing a simple cilk_stage_wait
 */

#include <cstdio>
#include <cstdlib>

#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "sequencer.h"


int test_stage_wait(int n, int use_stage_wait) {
    Sequencer Q;
    int i = 0;

    CILK_PIPE_WHILE_BEGIN(i < 2) {
        int loop_i = i;
        __cilkrts_worker *w  = __cilkrts_get_tls_worker();
        int count_idx = 0;
        int z = 0;
        ++i;

        int stage_num = 1;

        if (use_stage_wait && (loop_i == 0)) {
            // Advance iteration 0 so it is one stage "ahead" of
            // iteration 1, if we are using stage_waits.
            //
            // To be explained more below.
            stage_num++;
            CILK_STAGE_WAIT(stage_num);
        }
        else {
            CILK_STAGE(stage_num);
        }

        for (count_idx = 0; count_idx < n; ++count_idx) {
            z++;
            switch (loop_i) {
                // This section of the loop is designed to make
                // control flow alternate between iterations 0 and 1
                // as follows:

                //  0. 4*count_idx     on iteration 0
                //  1. 4*count_idx + 1 on iteration 1
                //  2. 4*count_idx + 2 on iteration 1
                //  3. 4*count_idx + 3 on iteration 0
                //
                // Normally, if the loops on iterations 0 and 1 lined
                // up exactly, (i.e., stage number == count_idx on
                // both loops), then iteration 1, stage x would wait
                // for iteration 0, stage x, and we have a deadlock.
                // Thus, if we are using stage_waits, we advance the
                // stage for iteration 0 to be one ahead, so that
                // iteration 1, stage x == count_idx+1 waits for
                // iteration 0, stage x == count_idx

                // For example, stage 2 for iteration 1 is count_idx
                // 1, which executes "5" and "6"
                // 
                // while stage 2 for iteration 0 is count_idx 0, which
                // executes "0" and "3".
            case 0:
            {
                Q.wait_for_num(4*count_idx,    10);
                Q.wait_for_num(4*count_idx +3, 10);
                break;
            }
            case 1:
            {
                Q.wait_for_num(4*count_idx + 1, 10);
                Q.wait_for_num(4*count_idx + 2, 10);
                break;
            }
            default:
                CILKTEST_PRINTF(3,
                                "## W=%d: starting iter %d, count_idx=%d\n",
                                w->self,
                                loop_i,
                                count_idx);
            }

            stage_num++;
            if (use_stage_wait) {
                CILK_STAGE_WAIT(stage_num);
            }
            else {
                // NO stage_waits between the iterations.  The
                // iteration are effectively running in parallel
                // (except for the alternation we just enforced
                // above).
                CILK_STAGE(stage_num);
            }
        }

        __cilkrts_worker *final_wkr = __cilkrts_get_tls_worker();
        CILKTEST_PRINTF(3,
                        "## Iter %d: started on wkr %d, finished on %d. z = %d, count_idx = %d, n = %d\n",
                        loop_i, w->self, final_wkr->self, z, count_idx, n);
        TEST_ASSERT(z == count_idx);
    } CILK_PIPE_WHILE_END();
    return 0;
}


int main(void) {
    CILKTEST_BEGIN("stage_wait");

    int code = __cilkrts_set_param("nworkers","2");
    TEST_ASSERT(0 == code);
    CILKTEST_PRINTF(2,  "Testing stage_wait on P = %d\n", __cilkrts_get_nworkers());


    const int L = 3;
    int testN[L] = {1, 10, 30};
    for (int use_stage_wait = 0; use_stage_wait <= 1; ++use_stage_wait)
    {
        for (int i = 0; i < L; ++i)
        {
            int n = testN[i];
            CILKTEST_PRINTF(2,  "## Trying n = %d\n", n);
            test_stage_wait(n, use_stage_wait);
        }
    }

    CILKTEST_END("stage_wait");
    return 0;
}

