/*  sample_spsps.cpp                  -*- C++ -*-
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
 * @file: sample_spsps.cpp
 *
 * @brief: A sample SPSPS parallel pipeline using a pipe-while loop.
 */


#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

// Include file to bring in to use Piper Cilk Plus.
#include <cilk/piper.h>

// For timing. 
#include "cilktest_timer.h"

int WORK_PARAM = 100000;

// A dummy work-load. 
// It's really an identity function, but the compiler is unlikely to figure that out.
int dummy_work( int i ) {
    const int n = WORK_PARAM;
    for( int k=-n; k<=n; ++k ) 
        i ^= k*k;
    return i;
}

// State type for unit testing.
struct state_type {
    int next;       // Index of next item to be consumed
    explicit state_type( int i ) : next(i) {}
};

// Item type for unit testing.
struct item_type {    
    int index;      // Index of item
    explicit item_type( int i ) : index(i) {}
};

// Pipeline is s1-s2-s3-s4-s5, where s2 and s4 are parallel stages.
// Stages are declared in reverse order.
// s1 produces natural numbers.
// s2 doubles its input numbers
// s3 triples its input numbers, and checks that they arrive in expected order
// s4 quaduples its input numbers.
// s5 checks its input numbers.

// State for stage 5
state_type state5(0);

// Function for stage 5
void stage5( state_type* s, item_type item ) {
    assert( item.index==2*3*4*s->next );
    s->next++;
}

// Function for stage 4
item_type stage4( item_type i ) {
    return item_type(dummy_work(4*i.index));
}
 
// State for stage 3
state_type state3(0);

// Function for stage 3
item_type stage3( state_type* s, item_type i ) {
    assert(i.index==2*s->next);
    s->next++;
    return item_type(3*i.index);
}

// Function for stage2
item_type stage2( item_type i ) {
    return item_type(dummy_work(2*i.index));
}

// State for stage0. 
state_type state1(0);

// Function for stage0.  It creates items from its corresponding state.
item_type stage1() {
    return item_type(state1.next++); 
}

// A pipe-while loop for a simple SPSPS pipeline.
int run_spsps_pipeline( int n ) {
    // Reset state of serial stages
    state1.next = 0;
    state3.next = 0;
    state5.next = 0;

    int i_loop = 0;
    CILK_PIPE_WHILE_BEGIN(i_loop < n) {
        // Stage 0: increments loop variable, and processes first
        // stage.
        i_loop++;
        item_type x = stage1();

        // Skipping to stage 2, just to keep the numbering easier.  We
        // could just as well use "1" as the stage number argument.
        CILK_STAGE(2);
        x = stage2(x);

        // Serial stage 3. 
        CILK_STAGE_WAIT(3);
        x = stage3(&state3, x);

        // Parallel stage 4.
        CILK_STAGE(4);
        x = stage4(x);

        // Serial stage 5.
        CILK_STAGE_WAIT(5);
        stage5(&state5, x);
    } CILK_PIPE_WHILE_END();

    assert( state1.next==n );
    assert( state3.next==n );
    assert( state5.next==n );

    return (state1.next + state3.next + state5.next);
}


int main(int argc, char* argv[]) {
    int P = __cilkrts_get_nworkers();
    int length = 1000;
    int NUM_TRIALS = 10;
    unsigned long long tm_begin, tm_elapsed;
    int result = 0;
    
    if (argc >= 2) {
	WORK_PARAM = std::atoi(argv[1]);
    }

    std::printf("Sample: testing SPSPS pipeline, work = %d, trials=%d\n",
                WORK_PARAM,
                NUM_TRIALS);

    // Warmup runs.
    for( int trial=0; trial< 5; ++trial ) {
        result += run_spsps_pipeline(length);
    }
    result = 0;
    
    // Timing runs.
    tm_begin = Cilk_get_wall_time();
    for( int trial=0; trial<NUM_TRIALS; ++trial ) {
        result += run_spsps_pipeline(length);
    }
    tm_elapsed = Cilk_get_wall_time() - tm_begin;

    std::printf("P=%d, time in seconds=%f, work=%d, [prog=%s, num_trials=%d, result=%d]\n",
                P,
                1.0e-3 * tm_elapsed,
                WORK_PARAM, 
                argv[0],
                NUM_TRIALS,
                result);

    std::printf("PASSED\n");
    return 0;
}
