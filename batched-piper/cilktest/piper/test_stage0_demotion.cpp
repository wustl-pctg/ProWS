/*  test_stage0_demotion.cpp                  -*- C++ -*-
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
 * @file test_stage0_demotion.cpp
 *
 * @brief Test program that forces a steal from a parallel stage 0 in
 * an iteration, and then tries to suspend and resume the iteration.
 */

#include <cstdio>
#include <cstdlib>

// Dummy include for now, so that we can include our pipeline
// parallelism constructs.
#include <cilk/cilk_api.h>
#include <cilk/cilk.h>
#include <cilk/reducer_opadd.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "sequencer.h"


inline void print_wkr_msg(const char* msg, int iter_num, int stage_num)
{
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    CILKTEST_PRINTF(2,
                    "W=%d: %s (iter %d, stage %d)\n",
                    w->self,
                    msg,
                    iter_num,
                    stage_num);
}


void stage0_par(Sequencer* Q) {
    Q->wait_for_num(1, 0);
    Q->wait_for_num(3, 0);
}


void iter0_stage0_simple_steal(Sequencer* Q) {
    Q->wait_for_num(0, 0);
    // stage0_par will hit sequence points 1 and 3.
    // the continuation will execute points 2 and 4.
    cilk_spawn stage0_par(Q);
    Q->wait_for_num(2, 0);
    Q->wait_for_num(4, 0);
    cilk_sync;
}

// The initialization and stage 0 for the loop to test stage0
// demotion.  This code is a macro, because it isn't obvious at all
// how to easily reuse between these sections.
#define TEST_STAGE0_DEMOTION_LOOP_BEGIN()                            \
    cilk::reducer_opadd<int> R(0);                                   \
    Sequencer Q;                                                     \
    int loop_i = 0;                                                  \
    CILKTEST_PRINTF(2, "Test stage0 demotion...\n");                 \
    CILK_PIPE_WHILE_BEGIN_THROTTLED(loop_i < 2, 4) {                 \
        int i = loop_i;                                              \
        ++loop_i;                                                    \
        int initialR = R.get_value();                                \
        int expected_view_value = initialR;                          \
        print_wkr_msg("starting", i, 0);                             \
        R+= (i+1);                                                   \
        expected_view_value += (i+1)

// Stage 2 + the end of the loop of the pipewhile.
#define TEST_STAGE0_DEMOTION_LOOP_END()               \
        TEST_ASSERT(R.get_value() == expected_view_value); \
        print_wkr_msg("finishing", i, 0);             \
        /* Begin stage 1 */                           \
        CILK_STAGE(1);                          \
        R += (i+1);                                   \
        expected_view_value += (i+1);                 \
        print_wkr_msg("starting", i, 1);              \
        switch (i) {                                  \
        case 0:                                       \
            Q.wait_for_num(6, 10);                    \
            break;                                    \
        case 1:                                       \
            Q.wait_for_num(7, 10);                    \
            break;                                    \
        }                                             \
        TEST_ASSERT(R.get_value() == expected_view_value); \
        print_wkr_msg("finishing", i, 1);             \
        CILK_STAGE(2);                          \
        print_wkr_msg("executing", i, 2);             \
        CILKTEST_PRINTF(2,                            \
                        "Final reducer value is %d\n",\
                        R.get_value());               \
        TEST_ASSERT(R.get_value() == expected_view_value); \
    } CILK_PIPE_WHILE_END()


int test_stage0_demotion() {
    TEST_STAGE0_DEMOTION_LOOP_BEGIN();
    // The interesting part of stage 0.
    switch (i) {
    case 0:
        iter0_stage0_simple_steal(&Q);
        break;
    case 1:
        Q.wait_for_num(5, 10);
        break;
    }

    TEST_STAGE0_DEMOTION_LOOP_END();
    return 0;
}

int test_s0_inline_spawn() {
    TEST_STAGE0_DEMOTION_LOOP_BEGIN();

    // Stage 0, executing the same code as above, except inlined, so
    // that the body of the pipe_while is a Cilk function.
    switch (i) {
    case 0:
    {
        // Inline it, so we force this
        Q.wait_for_num(0, 0);
        cilk_spawn stage0_par(&Q);
        Q.wait_for_num(2, 0);
        Q.wait_for_num(4, 0);
        cilk_sync;
    }
    break;
    case 1:
        Q.wait_for_num(5, 10);
        break;
    }
    TEST_STAGE0_DEMOTION_LOOP_END();
    return 0;
}

int main(void) {

    CILKTEST_BEGIN("stage0_demotion");
    
    int code = __cilkrts_set_param("nworkers","2");
    CILKTEST_PRINTF(2, "Setting params. code = %d\n", code);
    CILKTEST_PRINTF(2, "Testing stage0 demotions on P = %d\n", __cilkrts_get_nworkers());

    test_stage0_demotion();
    test_s0_inline_spawn();

    CILKTEST_END("stage0_demotion");
    return 0;
}


