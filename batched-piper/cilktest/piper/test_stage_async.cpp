/*  test_stage_async.cpp                  -*- C++ -*-
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
 * @file test_stage_async.cpp
 *
 * @brief Testing a simple cilk_stage.
 */

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>
#include <cilk/reducer_opadd.h>

#include "cilktest_harness.h"
#include "sequencer.h"


int test_stage_async(int use_reducer) {
    cilk::reducer_opadd<int> R(0);
    Sequencer Q;
    int loop_i = 0;

    CILKTEST_PRINTF(2, "Test stage async...\n");
    
    CILK_PIPE_WHILE_BEGIN_THROTTLED(loop_i < 2, 4) {
        int i = loop_i;
        ++loop_i;
        {
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            CILKTEST_PRINTF(2,
                            "W=%d, starting i = %d\n",
                            w ? w->self : -1,
                            i);
        }
        switch (i) {
        case 0:
            Q.wait_for_num(0, 10);
            break;
        case 1:
            Q.wait_for_num(1, 10);
            break;
        }

        CILK_STAGE(1);

        if (use_reducer) {
            __cilkrts_worker *w = __cilkrts_get_tls_worker();
            R += loop_i;
            int view = R.get_value();
            CILKTEST_PRINTF(2, "W=%d, in stage 1 on loop_i = %d, view = %d, i=%d\n",
                            w ? w->self : -1,
                            loop_i,
                            view, i);
        }
        
        switch (i) {
        case 1:
            Q.wait_for_num(2, 10);
            break;
        case 0:
            Q.wait_for_num(3, 10);
            break;
        }

        CILK_STAGE(2);
        switch(i) {
        case 1:
            Q.wait_for_num(4, 10);
            break;
        case 0:
            cilk_ms_sleep(10);
            break;
        }

    } CILK_PIPE_WHILE_END();

    if (use_reducer) {
        CILKTEST_PRINTF(2, "Final reducer value is %d\n", R.get_value());
    }
    return 0;
}



int main(void) {
    CILKTEST_BEGIN("stage_async");

    int code = __cilkrts_set_param("nworkers","2");
    CILKTEST_PRINTF(2, "Setting params. code = %d\n", code);
    CILKTEST_PRINTF(2, "Testing stage async on P = %d\n", __cilkrts_get_nworkers());
    
    test_stage_async(0); // No reducer.
    test_stage_async(1); // With a reducer.
    
    CILKTEST_BEGIN("end");
    return 0;
}

