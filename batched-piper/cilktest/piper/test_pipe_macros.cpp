/*  test_pipe_macros.cpp                  -*- C++ -*-
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
 * @file test_pipe_macros.cpp
 *
 * @brief Simple smoke test for compilation using the macros.
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"

/// Test simple macros for begin and end.
int test_pipe_while_macro(int n) {
    int i = 0;
    int sum = 0;

    CILK_PIPE_WHILE_BEGIN(i < n) {
        sum += (i+1);
        if (1) {
            __cilkrts_worker* current_w = __cilkrts_get_tls_worker();
            CILKTEST_PRINTF(2,
                            "Wkr %p (self=%d): test_pipe body: i = %d, sum=%d\n",
                            current_w,
                            current_w ? current_w->self : -1,
                            i, sum);
            cilk_ms_sleep(2);
        }
        i++;
        if (1) {
            cilk_ms_sleep(1);
        }
    } CILK_PIPE_WHILE_END();
    CILKTEST_PRINTF(2, "Final sum = %d\n", sum);
    return 0;
}

/// Test stage advance macros.
int test_pipe_stage_macros(int n) {
    int i = 0;
    int sum = 0;

    CILK_PIPE_WHILE_BEGIN(i < n) {
        int s = 0;
        sum += (i+1);
        if (1) {
            __cilkrts_worker* current_w = __cilkrts_get_tls_worker();
            CILKTEST_PRINTF(2,
                            "Wkr %p (self=%d): test_pipe body: i = %d, sum=%d\n",
                            current_w,
                            current_w ? current_w->self : -1,
                            i, sum);
            cilk_ms_sleep(2);
        }
        i++;
        CILKTEST_PRINTF(2, "HERE: getting stage %d, want s= %d\n", CILK_PIPE_STAGE(), s);
        TEST_ASSERT(CILK_PIPE_STAGE() == s);

        CILK_STAGE(1);
        s++;
        TEST_ASSERT(s == 1);
        TEST_ASSERT(CILK_PIPE_STAGE() == s);


        CILK_STAGE_WAIT(2);
        s++;
        TEST_ASSERT(s == 2);
        TEST_ASSERT(CILK_PIPE_STAGE() == s);

        // Begin stage 3.
        CILK_STAGE_NEXT();
        s++;

        CILKTEST_PRINTF(3,
                        "After stage 3, s = %d. Current stage = %d\n",
                        s,
                        CILK_PIPE_STAGE());
        TEST_ASSERT(s == 3);
        TEST_ASSERT(CILK_PIPE_STAGE() == s);

        // Begin stage 4. 
        CILK_STAGE_WAIT_NEXT();
        s++;

        TEST_ASSERT(s == 4);
        TEST_ASSERT(CILK_PIPE_STAGE() == s);
        
        if (1) {
            cilk_ms_sleep(1);
        }
    } CILK_PIPE_WHILE_END();
    CILKTEST_PRINTF(2, "Final sum = %d\n", sum);
    return 0;

}



int main(void) {
    CILKTEST_BEGIN("pipe_macros");
    test_pipe_while_macro(1);
    test_pipe_stage_macros(2);
    CILKTEST_END("pipe_macros");
    return 0;
}

