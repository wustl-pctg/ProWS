/*  test_parallel_iters.cpp                  -*- C++ -*-
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
 * @file test_parallel_iters.cpp
 *
 * @brief Benchmarking various kinds of parallel loops in Cilk.
 */

#include <cstdio> 
#include <cstdlib> 
#include <cstring> 
#include <cmath>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>
#include <cilk/reducer_opadd.h>
#include "cilktest_harness.h"
#include "cilktest_timer.h"

#include "pipe_while_loop_counter.h"

#ifndef TIMES_TO_RUN
#    define TIMES_TO_RUN 4
#endif

int WORK_PER_STAGE = 10000;
int NUM_STAGES = 20;

typedef enum RunType {
    ALL = 0,
    CILK_FOR = 1,
    LINEAR_SPAWN = 2,
    PIPE_CONTINUES = 3,
    PIPE_WAITS = 4, 
} RunType;


#if COLLECT_SAMPLES
    const int NUM_SAMPLES = 500;
    double start_times[NUM_SAMPLES];
    double end_times[NUM_SAMPLES];
#endif


// Borrowed from DotMix in Cilkpub.
uint64_t swap_halves(uint64_t x)
{
    return (x >> (4 * sizeof(uint64_t))) | (x << (4 * sizeof(uint64_t)));
}

uint64_t mix_iter(uint64_t x) {
    x = x*(2*x + 1);
    x = swap_halves(x);
    return x;
}

// Dummy work is to mix "x" for n iterations.
uint64_t dummy_work(uint64_t x, int n) {
    uint64_t val = x;
    for (int i = 0; i < n; ++i) {
        val = mix_iter(val);
    }
    return val;
}


// The parallel version of pipe_fib.
uint64_t test_parallel_iters_pipe_while(int n) {
    
    cilk::reducer_opadd<uint64_t> ans(0);

    PipeWhileLoopCounter<int> loop_i = 0;
    CILK_PIPE_WHILE_BEGIN(loop_i < n) {
        // Stage 0: save my loop index, so there is no race.
        int i = loop_i;
        loop_i++;
        uint64_t sum = 0;
        uint64_t val = i;

#if COLLECT_SAMPLES        
        if (i < NUM_SAMPLES) {
            start_times[i] = Cilk_get_wall_time();
        }
#endif
        
        CILK_STAGE(1);
        for (int S = 2; S < NUM_STAGES; ++S) {
            val = dummy_work(S, WORK_PER_STAGE);
            sum += val;
            CILK_STAGE(S);
        }

        ans += sum;

#if COLLECT_SAMPLES
        if (i < NUM_SAMPLES) {
            end_times[i] = Cilk_get_wall_time();
        }
#endif
    } CILK_PIPE_WHILE_END();
    return ans.get_value();
}


// The parallel version of pipe_fib.
uint64_t test_parallel_iters_pipe_while_with_waits(int n) {
    
    cilk::reducer_opadd<uint64_t> ans(0);

    PipeWhileLoopCounter<int> loop_i = 0;
    CILK_PIPE_WHILE_BEGIN(loop_i < n) {
        // Stage 0: save my loop index, so there is no race.
        int i = loop_i;
        loop_i++;
        uint64_t sum = 0;
        uint64_t val = i;

#if COLLECT_SAMPLES        
        if (i < NUM_SAMPLES) {
            start_times[i] = Cilk_get_wall_time();
        }
#endif
        
        CILK_STAGE_WAIT(1);

        for (int S = 2; S < NUM_STAGES; ++S) {
            val = dummy_work(S, WORK_PER_STAGE);
            sum += val;
            CILK_STAGE_WAIT(S);
        }

        ans += sum;
#if COLLECT_SAMPLES        
        if (i < NUM_SAMPLES) {
            end_times[i] = Cilk_get_wall_time();
        }
#endif
        
    } CILK_PIPE_WHILE_END();
    return ans.get_value();
}

uint64_t test_parallel_iters_cilk_for(int n) {
    cilk::reducer_opadd<uint64_t> ans(0);
    cilk_for(int i = 0; i < n; ++i) {
        uint64_t sum = 0;
        uint64_t val = i;

        for (int S = 2; S < NUM_STAGES; ++S) {
            val = dummy_work(S, WORK_PER_STAGE);
            sum += val;
        }
        ans += sum;
    }
    return ans.get_value();
}

// Declare this reducer globally, to work around a known issue with
// reducers declared in local scope in Clang.
cilk::reducer_opadd<uint64_t> linear_spawn_ans(0);
uint64_t test_parallel_iters_linear_spawn(int n) {
    linear_spawn_ans.set_value(0);
    for (int i = 0; i < n; ++i) {
        _Cilk_spawn [i]() {
            uint64_t sum = 0;
            uint64_t val = i;

            for (int S = 2; S < NUM_STAGES; ++S) {
                val = dummy_work(S, WORK_PER_STAGE);
                sum += val;
            }
            linear_spawn_ans += sum;
        }();
    }
    _Cilk_sync;
    return linear_spawn_ans.get_value();
}

void test_run(const char* msg,
              int n,
              uint64_t (*test_func)(int)) {
    double tm_begin;
    double tm_elapsed[TIMES_TO_RUN];

    double avg_time = 0.0;
    int P = __cilkrts_get_nworkers();

    for(int i = 0; i < TIMES_TO_RUN; i++) {
        uint64_t ans;
        tm_begin = Cilk_get_wall_time();
        ans = test_func(n);
        tm_elapsed[i] = Cilk_get_wall_time() - tm_begin;
        CILKTEST_PRINTF(2,
                        "P = %d: %s run %d, time = %g ms, ans = %llu\n",
                        P, msg, i, tm_elapsed[i], ans);
        avg_time += tm_elapsed[i];
        
#if COLLECT_SAMPLES
        int K = (NUM_SAMPLES < n) ? NUM_SAMPLES : n;
        for (int j = 0; j < K; ++j) {
            CILKTEST_PRINTF(3,
                            "Iteration %d: start = %g, end = %g. Time = %g, offset = %g\n",
                            j,
                            start_times[j] - start_times[0],
                            end_times[j] - start_times[0],
                            end_times[j] - start_times[j],
                            (j > 0) ? (start_times[j] - start_times[j-1]) : 0);
        }
#endif
    }

    avg_time /= TIMES_TO_RUN;

    CILKTEST_PRINTF(2,
                    "P = %d: %s average time = %g ms\n",
                    P, msg, avg_time);
}



int main_wrapper(int argv, char *args[]) {
    int n = 1000;
    RunType typ = ALL;

    if (argv >= 2) {
        n = atoi(args[1]);
    }
    if (argv >= 3) {
        WORK_PER_STAGE = atoi(args[2]);
    }
    if (argv >= 4) {
        typ = (RunType)atoi(args[3]);
    }

    CILKTEST_PRINTF(2,
                    "Test on P = %d, num_iters = %d, work_per_stage = %d, typ=%d\n",
                    __cilkrts_get_nworkers(),
                    n,
                    WORK_PER_STAGE,
                    typ);

    switch (typ) {
    case CILK_FOR:
        test_run("cilk_for", n, test_parallel_iters_cilk_for);
        break;
    case LINEAR_SPAWN:
        test_run("linear_spawn", n, test_parallel_iters_linear_spawn);
        break;
    case PIPE_CONTINUES:
        test_run("pipe_while_continues", n, test_parallel_iters_pipe_while);
        break;
    case PIPE_WAITS:
        test_run("pipe_while_waits", n, test_parallel_iters_pipe_while_with_waits);
        break;
    case ALL:
    default:
        test_run("cilk_for", n, test_parallel_iters_cilk_for);
        test_run("linear_spawn", n, test_parallel_iters_linear_spawn);
        test_run("pipe_while_continues", n, test_parallel_iters_pipe_while);
        test_run("pipe_while_waits", n, test_parallel_iters_pipe_while_with_waits);
    }

    return 0;
}

int main(int argc, char* argv[]) {
    CILKTEST_BEGIN("parallel_iters");
    main_wrapper(argc, argv);
    CILKTEST_END("parallel_iters");
    return 0;
}

