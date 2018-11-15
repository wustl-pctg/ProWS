/*  test_no_inline_iter_helper.cpp                  -*- C++ -*-
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
 * @file test_no_inline_check.cpp
 *
 * @brief Test program that verifies that the iteration helper is not
 *        being inlined.
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "sequencer.h"


/// Remember the stack pointers for two consecutive pipeline
/// iterations.
__cilkrts_stack_frame* sf_ptrs[2];


/**
 * Get the stack frames for two pipeline iterations, when a steal is
 * forced to happen.  These stack frames should be different!  If the
 * compiler is mistakenly inlining the iteration helper, then the
 * stack frames may appear to be the same.
 */
int test_no_inline_iter_helper() {
    int i = 0;
    Sequencer Q;

    for (int i = 0; i < 2; ++i) {
        sf_ptrs[i] = NULL;
    }

    CILK_PIPE_WHILE_BEGIN(i < 2) {
        int loop_i = i;
        ++i;
        CILK_STAGE(1);

        fprintf(stderr, "Starting loop_i = %d\n", loop_i);
        switch (loop_i) {
        case 0:
        {
            Q.wait_for_num(0, 10);
            __cilkrts_worker* w = __cilkrts_get_tls_worker();
            sf_ptrs[0] = w->current_stack_frame;
            CILKTEST_PRINTF(2,
                            "W=%d: iteration %d got stack frame %p\n",
                            w->self,
                            0,
                            sf_ptrs[0]);
            Q.wait_for_num(2, 10);
            break;
        }

        case 1:
        {
            Q.wait_for_num(1, 10);
            __cilkrts_worker* w = __cilkrts_get_tls_worker();
            sf_ptrs[1] = w->current_stack_frame;
            CILKTEST_PRINTF(2, "W=%d: iteration %d got stack frame %p\n",
                            w->self, 1, sf_ptrs[1]);
            Q.wait_for_num(3, 10);
        }
        }
    } CILK_PIPE_WHILE_END();

    CILKTEST_PRINTF(2,
                    "Final pointers: ptr[0] = %p, ptr[1] = %p\n",
                    sf_ptrs[0],
                    sf_ptrs[1]);
    TEST_ASSERT(sf_ptrs[0] != sf_ptrs[1]);
    return 0;
}


int main(void) {
    CILKTEST_BEGIN("no_inline_iter_helper");
    int code = __cilkrts_set_param("nworkers","2");
    TEST_ASSERT(code == 0);
    test_no_inline_iter_helper();
    CILKTEST_END("no_inline_iter_helper");
    return 0;
}

