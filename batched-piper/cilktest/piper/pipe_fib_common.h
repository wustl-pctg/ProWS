/*  pipe_fib_common.h                  -*- C++ -*-
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
 * @file pipe_fib_common.h
 *
 * @brief Calculating fib bit by bit using a linear-time pipelined
 * algorithm.
 *
 * TBD: This code isn't quite a true library header file --- including
 * it more than once might not work as you'd expect.  It can be fixed
 * later probably...
 */

#ifndef __PIPE_FIB_COMMON_H_
#define __PIPE_FIB_COMMON_H_

#include <math.h>
#include <cstdio> 
#include <cstdlib> 
#include <cstring> 
#include <cmath>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "cilktest_timer.h"


/********************************************************************/
// This test code was ported from the MIT pipeline parallelism
// benchmark, and modified to use the pipeline constructs.


#ifdef _WIN32
// Yeah, Windows is not defining log2?
inline double log2(double x) {
    return log(x) / log(2.0);
}
#endif

#ifndef TIMES_TO_RUN
#    define TIMES_TO_RUN 10
#endif

#define SERIALIZATION 1
#define END_MARKER    2   // can be anything within uint8_t that's not 0 or 1
#define CHECK_RESULT 1

// #define COARSEN 8

// The primitive type we are using to store a bit.
typedef uint8_t BitVal;

static uint64_t linear_fib(int n) {
    uint64_t vals[3] = {0, 1, 1};
 
    for(int i=3; i <= n; i++) {
        vals[i%3] = vals[(i-1)%3] + vals[(i-2)%3];
    }
    return vals[n%3];
}


// The serial source code for pipe_fib
void serial_pipe_fib(int n, BitVal *fib[3]) {
    
    // we start with the base case
    fib[0][0] = END_MARKER;
    fib[1][0] = 1;
    fib[1][1] = END_MARKER;
    fib[2][0] = 1;
    fib[2][1] = END_MARKER;

    // in parallel version, this should be pipe_for
    for (int i = 3; i <= n; i++) {  
        int j = 0; 
        int overflow = 0;

        while(overflow != END_MARKER) {
            fib[i%3][j] = overflow;
            // take the lower bit, which does the "right" thing even when
            // it is an END_MARKER (i.e., 0)
            fib[i%3][j] += fib[(i-1)%3][j] & 0x1;
            fib[i%3][j] += fib[(i-2)%3][j] & 0x1;
            // for overflow
            overflow = fib[i%3][j] >> 1; 
            fib[i%3][j] = fib[i%3][j] & 0x1;

            if(fib[(i-1)%3][j] == END_MARKER) {
                overflow = END_MARKER;  // be sure to break out of the loop next
                if(fib[i%3][j] == 0) {
                    // mark this j index as END_MARKER if it contains 0;
                    // must set this before we do SNEXT; to be correct
                    fib[i%3][j] = overflow; 
                } else {
                    // otherwise increment j and wait till the end to mark 
                    // j+1 as END_MARKER; can't set the j+1 as END_MARKER 
                    // here; otherwise will race with later iterations 
                    // (race by setting j+1 w/out SNEXT;)
                    j++;  
                }
            } else {
                j++;
            }
            // SNEXT;  // wait for left 
        }
        fib[i%3][j] = overflow;  // redundant if j didn't get increment, but ok.
        // END_ITER;
    }
}

// Create a structure around the loop counter, so that it ends up on
// its own cache line.
typedef struct {
    char pre_pad[64];
    int val;
    char post_pad[64 - sizeof(int)];
} LoopCounter;

// The parallel version of pipe_fib.
void pipe_fib_with_throttling(int n, BitVal *fib[3], int throttle_limit) {
    // we start with the base case
    fib[0][0] = END_MARKER;
    fib[1][0] = 1;
    fib[1][1] = END_MARKER;
    fib[2][0] = 1;
    fib[2][1] = END_MARKER;

    LoopCounter loop_i;
    loop_i.val = 3;
    CILK_PIPE_WHILE_BEGIN_THROTTLED(loop_i.val <= n, throttle_limit) {
        // Stage 0: save my loop index, so there is no race.
        int i = loop_i.val;
        loop_i.val++;

        // Bit index.  We will work with bit j in stage (j+1).
        int j = 0;   
        int overflow = 0;

        CILK_STAGE_WAIT(1); // End of stage 0.

        bool need_to_set_overflow = 0;
        while(overflow != END_MARKER) {
            fib[i%3][j] = overflow;
            // take the lower bit, which does the "right" thing even when
            // it is an END_MARKER (i.e., 0)
            fib[i%3][j] += fib[(i-1)%3][j] & 0x1;
            fib[i%3][j] += fib[(i-2)%3][j] & 0x1;
            // for overflow
            overflow = fib[i%3][j] >> 1; 
            fib[i%3][j] = fib[i%3][j] & 0x1;

            if(fib[(i-1)%3][j] == END_MARKER) {
                overflow = END_MARKER;  // be sure to break out of the loop next
                if(fib[i%3][j] == 0) {
                    // mark this j index as END_MARKER if it contains 0;
                    // must set this before we do SNEXT; to be correct
                    fib[i%3][j] = overflow; 
                    CILK_STAGE_WAIT(j+2);
                } else {
                    // otherwise increment j and wait till the end to mark 
                    // j+1 as END_MARKER; can't set the j+1 as END_MARKER 
                    // here; otherwise will race with later iterations 
                    // (race by setting j+1 w/out SNEXT;)
                    need_to_set_overflow =1 ;
                    j++;
                    CILK_STAGE_WAIT(j+1);
                }
                
            } else {
                j++;
                // The first iteration starts with j = 0, in stage 1.
                CILK_STAGE_WAIT(j+1);
            }
        }

        if (need_to_set_overflow) {
            fib[i%3][j] = overflow;
        }
    } CILK_PIPE_WHILE_END();
}


// The parallel version of pipe_fib, with the optimization that we
// check for detach after stage 0.
void pipe_fib_with_throttling_detach_opt(int n, BitVal *fib[3], int throttle_limit) {
    // we start with the base case
    fib[0][0] = END_MARKER;
    fib[1][0] = 1;
    fib[1][1] = END_MARKER;
    fib[2][0] = 1;
    fib[2][1] = END_MARKER;

    LoopCounter loop_i;
    loop_i.val = 3;
    CILK_PIPE_WHILE_BEGIN_THROTTLED(loop_i.val <= n, throttle_limit) {
        // Stage 0: save my loop index, so there is no race.
        int i = loop_i.val;
        loop_i.val++;

        // Bit index.  We will work with bit j in stage (j+1).
        int j = 0;   
        int overflow = 0;

        CILK_STAGE_WAIT(1); // End of stage 0.

        bool need_to_set_overflow = 0;
        while(overflow != END_MARKER) {
            fib[i%3][j] = overflow;
            // take the lower bit, which does the "right" thing even when
            // it is an END_MARKER (i.e., 0)
            fib[i%3][j] += fib[(i-1)%3][j] & 0x1;
            fib[i%3][j] += fib[(i-2)%3][j] & 0x1;
            // for overflow
            overflow = fib[i%3][j] >> 1; 
            fib[i%3][j] = fib[i%3][j] & 0x1;

            if(fib[(i-1)%3][j] == END_MARKER) {
                overflow = END_MARKER;  // be sure to break out of the loop next
                if(fib[i%3][j] == 0) {
                    // mark this j index as END_MARKER if it contains 0;
                    // must set this before we do SNEXT; to be correct
                    fib[i%3][j] = overflow; 
                    CILK_STAGE_WAIT_AFTER_STAGE_0(j+2);
                } else {
                    // otherwise increment j and wait till the end to mark 
                    // j+1 as END_MARKER; can't set the j+1 as END_MARKER 
                    // here; otherwise will race with later iterations 
                    // (race by setting j+1 w/out SNEXT;)
                    need_to_set_overflow =1 ;
                    j++;
                    CILK_STAGE_WAIT_AFTER_STAGE_0(j+1);
                }
                
            } else {
                j++;
                // The first iteration starts with j = 0, in stage 1.
                CILK_STAGE_WAIT_AFTER_STAGE_0(j+1);
            }
        }

        if (need_to_set_overflow) {
            fib[i%3][j] = overflow;
        }
    } CILK_PIPE_WHILE_END();
}

static void intToBinaryString(char *bStr, int bstr_len, uint64_t val) {
    uint64_t mask = 0x1;
    for(int i = bstr_len-2; i >= 0; i--) {
        bStr[i] = ((val & mask) == 0) ? '0' : '1'; 
        mask = mask << 1;
    }
    bStr[bstr_len-1] = '\0';
}

// check_result returns the number of errors detected (each bit that
// differs counts as one error).
static int check_result(BitVal *fib, char *bStr, int array_size) {

    int error = 0;

    for(int i=array_size-2, j=0; i >= 0; i--) {
        if(fib[i] != END_MARKER) {
            if( (fib[i] && bStr[j] == '0') ||  
               (!fib[i] && bStr[j] == '1') ) {
               error++;
            } 
        } else if (bStr[j] != '0') {
            error++;
        }
        j++;
    }

    return error;
}

static void compute_serial_fib(BitVal *serial_fib[3],
                               int array_size,
                               int n) 
{
    double tm_begin;
    double tm_elapsed[2];
    for (int i = 0; i < 2; i++) {
        memset(serial_fib[0], 0, array_size);
        memset(serial_fib[1], 0, array_size);
        memset(serial_fib[2], 0, array_size);
        tm_begin = Cilk_get_wall_time();
        serial_pipe_fib(n, serial_fib);
        tm_elapsed[i] = Cilk_get_wall_time() - tm_begin;
        CILKTEST_PRINTF(2,
                        "Serial run %d: time = %g ms\n",
                        i, tm_elapsed[i]);
    }
}



#endif  // defined(__PIPE_FIB_COMMON_H_)
