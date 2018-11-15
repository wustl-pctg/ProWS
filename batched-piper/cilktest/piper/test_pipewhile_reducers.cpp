/*  test_pipewhile_reducers.cpp                  -*- C++ -*-
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
 * @file test_pipewhile_reducers.cpp
 *
 * @brief Simple test program updating reducers in a pipewhile loop.
 */

#include <cstdio>
#include <cstdlib>

#include <cilk/cilk_api.h>
#include <cilk/piper.h>
#include <cilk/reducer_opadd.h>
#include <cilk/reducer_list.h>

#include "cilktest_harness.h"
#include "fib.h"



int test_pipe_while_macro(int n) {
    int i = 0;
    int sum = 0;
    cilk::reducer_opadd<int> rsum(0);
    cilk::reducer_list_append<int> rlist;

    CILK_PIPE_WHILE_BEGIN(i < n) {
        sum += (i+1);
        rsum += (i+1);
        rlist.push_back(i+1);

        CILKTEST_PRINTF(2,
                        "Wkr %d: test_pipe body: i = %d, sum=%d, reducer view has value %d\n",
                        __cilkrts_get_worker_number(),
                        i, sum,
                        rsum.get_value());
        cilk_ms_sleep(1);
        
        i++;
        cilk_ms_sleep(0);

    } CILK_PIPE_WHILE_END();
    CILKTEST_PRINTF(2, "Final reducer sum = %d\n", rsum.get_value());
    TEST_ASSERT(rsum.get_value() == sum);


    std::list<int> flist = rlist.get_value();
    TEST_ASSERT((int)flist.size() == n);


    // Check that the list append reducer has items in order.
    if (n >= 1) {
        std::list<int>::iterator it = flist.begin();
        int initial_value = *it;
        int current_value;
        int item_count = 1;
        ++it;

        CILKTEST_PRINTF(2, "Item 0: %d\n", initial_value);

        while (it != flist.end()) {
            current_value = *it;            
            CILKTEST_PRINTF(2,
                            "Item %d: %d\n",
                            item_count,
                            current_value);
            TEST_ASSERT(current_value > initial_value);
            TEST_ASSERT(current_value == (item_count+1));

            ++it;
            ++item_count;
        }
    }
    
    return 0;
}


int test_pipe_while_macro_with_fib() {
    int i = 0;
    int sum = 0;
    cilk::reducer_opadd<int> rsum(0);

    CILK_PIPE_WHILE_BEGIN(i < 10) {
        int ans;
        int n = 10 + i;
        sum += (i+1);
        rsum += (i+1);
        if (1) {
            CILKTEST_PRINTF(2,
                            "Wkr %d: test_pipe body: i = %d, sum=%d. Reducer view is %d\n",
                            __cilkrts_get_worker_number(),
                            i, sum, rsum.get_value());
            cilk_ms_sleep(0);
        }

        ans = fib(n);
        i++;
        if (1) {
            CILKTEST_PRINTF(2,
                            "Wkr %d: test_pipe body: i = %d, sum=%d. fib(%d) = %d\n",
                            __cilkrts_get_worker_number(),
                            i, sum,
                            n, ans);
            cilk_ms_sleep(0);
        }
    } CILK_PIPE_WHILE_END();
    CILKTEST_PRINTF(2, "Final reducer sum = %d\n", rsum.get_value());
    TEST_ASSERT(rsum.get_value() == sum);
    return 0;
}


int main(void) {
    CILKTEST_BEGIN("pipewhile_reducers");
    test_pipe_while_macro(1);
    test_pipe_while_macro(10);
    test_pipe_while_macro_with_fib();
    CILKTEST_END("pipewhile_reducers");
    return 0;
}

