/*  test_pipewhile_lib.cpp                  -*- C++ -*-
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
 * @file test_pipewhile_lib.cpp
 *
 * @brief Testing a simple library interface for pipelines.
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>
#include <internal/piper_lib.h>

#include "cilktest_harness.h"
#include "fib.h"

struct UserData {
    int ans;
    __cilkrts_pipe_iter_num_t y;
    
    UserData(__cilkrts_pipe_iter_num_t i)  {
        (void)i;
    }
};

/**
 * Sample pipe_while loop using the library interface.
 */
void test_pipewhile_lib(int is_serial) {
    int x = 0;
    // A pipe_while that should behave like a serial while, because
    // the iteration body does not have any stage or stage_wait
    // statements.
    pipelib::__cilkrts_pipe_while_loop<UserData>(
        [&x]() {    // The test condition.
            return (x < 10);
        },

        [&x, is_serial](UserData& data,
                        __cilkrts_pipe_iter_num_t iter_num,
                        __cilkrts_pipe_stage_num_t stage_num) -> pipelib::stage_status
        {
            if (stage_num == 0) {
                data.y = (__cilkrts_pipe_iter_num_t)x;
                x++;
                if (is_serial) {
                    data.ans = serial_fib(25);
                }
                else {
                    data.ans = fib(25);
                }
                CILKTEST_PRINTF(2,
                                "Current iteration is %lld. fib(25) = %d\n",
                                data.y,
                                data.ans);
                TEST_ASSERT(data.ans == 75025);
                return pipelib::stage_async(1);
            }
            else {
                CILKTEST_PRINTF(2,
                                "Executing (%lld, %lld) of pipe_loop. y = %lld\n",
                                iter_num,
                                stage_num,
                                data.y);
                TEST_ASSERT(data.ans == 75025);
                if ((stage_num + 1) < 4) {
                    if ((stage_num+1) % 2) {
                        return pipelib::stage_wait(stage_num+1);
                    }
                    else {
                        return pipelib::stage_async(stage_num+1);
                    }
                }
                else {
                    return pipelib::pipe_finish();
                }
            }
        },
        10);
}


void dummy_printf(const char* msg)
{
    CILKTEST_PRINTF(2, "%s\n", msg);
}

int main(void) {
    CILKTEST_BEGIN("pipewhile_lib");
    _Cilk_spawn dummy_printf("Dummy init of runtime...");
    _Cilk_sync;

    test_pipewhile_lib(1);
    test_pipewhile_lib(0);
    CILKTEST_END("pipewhile_lib");
    return 0;
}

