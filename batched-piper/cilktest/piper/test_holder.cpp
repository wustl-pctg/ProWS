/*  test_holder.cpp                  -*- C++ -*-
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
 * @file test_holder.cpp
 *
 * @brief Pipeline interactions with a holder.
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "parallel_test_holder.h"
#include "serial_test_holder.h"



template <typename HolderType>
int test_holder_generic(int n) {
    int i = 0;
    HolderType R;
    int last_fval = 0;

    CILK_PIPE_WHILE_BEGIN_THROTTLED(i < n, 4) {
        int loop_i = i;
        // Some periodic amount for stage 0 of every iteration.
        int fval = 25 + (i % 10);
        last_fval = fval;
        ++i;

        CILK_STAGE(1);

        // Iteration i calculates a value of fib in between 21 and 30.
        // Have the iterations decrease but be periodic.

        // Just replace the value.
        R.set_value(fval);
        {
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            int wkr_id = w ? w->self : -1;
            CILKTEST_PRINTF(2,
                            "## W=%d, starting stage 1 on i = %d\n",
                            wkr_id, loop_i);
        }

        if (i == 0) {
            cilk_ms_sleep(40);
        }
        else {
            cilk_ms_sleep(10);
        }

        {
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            int wkr_id = w ? w->self : -1;
            CILKTEST_PRINTF(2,
                            "## W=%d, finishing stage 1 on i = %d\n",
                            wkr_id, loop_i);
        }
        
        CILK_STAGE(2);

        {
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            int wkr_id = w ? w->self : -1;
            CILKTEST_PRINTF(2,
                            "## W=%d, starting and finishing stage 2 on i = %d\n",
                            wkr_id, loop_i);
        }

    } CILK_PIPE_WHILE_END();

    TEST_ASSERT(R.get_value() == last_fval);
    
    return 0;
}



int main(void) {

    CILKTEST_BEGIN("holder");
    CILKTEST_PRINTF(2, "Testing holder on P = %d\n", __cilkrts_get_nworkers());
    const int L = 4;
    int testN[L] = {3, 4, 10, 100};

    for (int i = 0; i < L; ++i)
    {
        int n = testN[i];
        CILKTEST_PRINTF(2, "## Trying n = %d\n", n);
        test_holder_generic<cilk::serial_test_holder>(n);
        test_holder_generic<cilk::parallel_test_holder>(n);
    }
    
    CILKTEST_END("holder");
    return 0;
}

