/*  test_simple_pipewhile.cpp                  -*- C++ -*-
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
 * @file test_simple_pipewhile.cpp
 *
 * @brief Simple pipewhile loop. 
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "fib.h"


int test_pipe_while_macro(int n) {
    int i = 0;
    int sum = 0;

    CILK_PIPE_WHILE_BEGIN(i < n) {
        sum += (i+1);
        if (1) {
            CILKTEST_PRINTF(2,
                            "Wkr %p (self=%d): test_pipe body: i = %d, sum=%d\n",
                            __cilkrts_get_tls_worker(),
                            __cilkrts_get_tls_worker()->self,
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


int test_pipe_while_macro_with_fib() {
    int i = 0;
    int sum = 0;

    CILK_PIPE_WHILE_BEGIN(i < 10) {
        int ans;
        int n = 10 + i;
        sum += (i+1);
        if (1) {
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            CILKTEST_PRINTF(2,
                            "Wkr %p (%d): test_pipe body: i = %d, sum=%d\n",
                            w, w->self,
                            i, sum);
            cilk_ms_sleep(2);
        }

        ans = fib(n);
        i++;
        if (1) {
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            CILKTEST_PRINTF(2,
                            "Wkr %p (%d): test_pipe body: i = %d, sum=%d. fib(%d) = %d\n",
                            w,
                            w->self,
                            i, sum,
                            n, ans);
            cilk_ms_sleep(2);
        }
    } CILK_PIPE_WHILE_END();
    CILKTEST_PRINTF(2, "Final sum = %d\n", sum);
    return 0;
}


// Dummy printf is here because Clang currently doesn't support
// spawning of a printf.
void dummy_printf(const char* msg) {
    CILKTEST_PRINTF(2, "%s", msg);
}

int main(void) {
    CILKTEST_BEGIN("simple_pipewhile");

    _Cilk_spawn dummy_printf("Initializing runtime...");
    _Cilk_sync;

    test_pipe_while_macro(1);
    test_pipe_while_macro(10);
    test_pipe_while_macro_with_fib();

    CILKTEST_END("simple_pipewhile");
    return 0;
}

