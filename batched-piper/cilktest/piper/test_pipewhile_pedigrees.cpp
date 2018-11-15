/*  test_pipewhile_pedigrees.cpp                  -*- C++ -*-
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
 * @file test_pipewhile_pedigrees.cpp
 *
 * @brief Simple test of pipelines and pedigrees.
 */

#include <cstdio>
#include <cstdlib>
#include <vector>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/piper.h>

#include "cilktest_harness.h"
#include "fib.h"



// Print current pedigree, and verifies that pedigree has expected
// length, and pedigree[expected_idx] == expected_rank.
void print_current_pedigree_with_check(const char* msg,
                                       int i,
                                       int expected_length,
                                       int expected_idx,
                                       uint64_t expected_rank) 
{
    std::vector<uint64_t> pedigree_terms;
    __cilkrts_pedigree pednode = __cilkrts_get_pedigree();
    const __cilkrts_pedigree* ped = &pednode;

    do {
        pedigree_terms.push_back(ped->rank);
        ped = ped->parent;
    } while (NULL != ped);

    CILKTEST_PRINTF(2,
                    "[%s] -- Wkr %d: Iteration %d: pedigree length = %llu\n",
                    msg,
                    __cilkrts_get_worker_number(),
                    i,
                    (unsigned long long)pedigree_terms.size());

    for (unsigned j = 0; j < pedigree_terms.size(); ++j) {
        CILKTEST_PRINTF(2,
                        "[%s] -- Wkr %d, iteration %d, pedigree[%u]: %llu\n",
                        msg,
                        __cilkrts_get_worker_number(),
                        i,
                        j,
                        (unsigned long long)pedigree_terms[j]);
    }

    if (expected_length > 0) {
        if (pedigree_terms.size() != expected_length) {
            CILKTEST_PRINTF(2, "[%s] --- ERROR. expected pedigree of length %d\n",
                   msg,
                   expected_length);
        }
        TEST_ASSERT(pedigree_terms.size() == expected_length);
    }

    if ((expected_idx < expected_length)  && (expected_idx >= 0)) {
        TEST_ASSERT(pedigree_terms[expected_length-1 - expected_idx] == expected_rank);
    }
}

// Print current pedigree without checking.
void print_current_pedigree(const char* msg, int i) {
    print_current_pedigree_with_check(msg, i, -1, -1, -1);
}


int test_pipe_while_macro(int n) {
    int i = 0;
    int sum = 0;

    __cilkrts_pedigree ped = __cilkrts_get_pedigree();
    uint64_t initial_rank = ped.rank;

    print_current_pedigree_with_check("initial pedigree", -1,
                                      2, 1, initial_rank);
    __cilkrts_bump_worker_rank();
    print_current_pedigree_with_check("initial pedigree after bump", -1,
                                      2, 1, initial_rank+1);
    
    CILK_PIPE_WHILE_BEGIN(i < n) {
        sum += (i+1);
        if (1) {
            CILKTEST_PRINTF(2,
                            "Wkr %d: test_pipe body: i = %d, sum=%d\n",
                            __cilkrts_get_worker_number(),
                            i,
                            sum);
            cilk_ms_sleep(2);
        }

        // Check last two terms in the pedigree.
        print_current_pedigree_with_check("pipe_while", i,
                                          3, 1, initial_rank+1 + i);
        print_current_pedigree_with_check("pipe_while", i,
                                          3, 2, 0);

        __cilkrts_bump_worker_rank();
        print_current_pedigree_with_check("pipe_while_after_inc", i,
                                          3, 1, initial_rank+1 + i);
        print_current_pedigree_with_check("pipe_while_after_inc", i,
                                          3, 2, 1);

        i++;
        if (1) {
            cilk_ms_sleep(0);
        }
    } CILK_PIPE_WHILE_END();
    CILKTEST_PRINTF(2, "Final sum = %d\n", sum);

    
    return 0;
}


int test_pipe_while_macro_with_fib() {
    int i = 0;
    int sum = 0;

    __cilkrts_pedigree ped = __cilkrts_get_pedigree();
    uint64_t initial_rank = ped.rank;
    uint64_t rank_delta;

    print_current_pedigree_with_check("initial pedigree", -1,
                                      2, 1, initial_rank);
    fib(4);

    __cilkrts_pedigree ped2 = __cilkrts_get_pedigree();
    rank_delta = ped2.rank - initial_rank;

    print_current_pedigree_with_check("initial pedigree after bump", -1,
                                      2, 1, initial_rank+rank_delta);

    CILK_PIPE_WHILE_BEGIN(i < 10) {
        int ans = 0;
        int n = 10 + i;
        sum += (i+1);
        if (1) {
            CILKTEST_PRINTF(2,
                            "Wkr %d: test_pipe body: i = %d, sum=%d\n",
                            __cilkrts_get_worker_number(),
                            i,
                            sum);
            cilk_ms_sleep(0);
        }


        // Check last two terms in the pedigree.
        print_current_pedigree_with_check("pipe_while", i,
                                          3, 1, ped2.rank + i);
        print_current_pedigree_with_check("pipe_while", i,
                                          3, 2, 0);
        ans = fib(4);

        // After fib, the next to last term is the same, the last term
        // should increment by rank_delta.
        print_current_pedigree_with_check("pipe_while_after_fib", i,
                                          3, 1, ped2.rank + i);
        print_current_pedigree_with_check("pipe_while_after_fib", i,
                                          3, 2, rank_delta);

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
    CILKTEST_PRINTF(2, "Final sum = %d\n", sum);
    return 0;
}



int test_pipe_while_macro_with_inline_spawn() {
    int i = 0;
    int sum = 0;

    __cilkrts_pedigree ped = __cilkrts_get_pedigree();
    uint64_t initial_rank = ped.rank;
    uint64_t rank_delta;

    print_current_pedigree_with_check("initial pedigree", -1,
                                      2, 1, initial_rank);
    int tmp = 0;
    tmp = cilk_spawn fib(4);
    cilk_sync;
    TEST_ASSERT(tmp == 3);

    __cilkrts_pedigree ped2 = __cilkrts_get_pedigree();
    rank_delta = ped2.rank - initial_rank;

    print_current_pedigree_with_check("initial pedigree after bump", -1,
                                      2, 1, initial_rank+rank_delta);

    CILK_PIPE_WHILE_BEGIN(i < 10) {
        int ans = 0;
        int n = 10 + i;
        sum += (i+1);
        if (1) {
            CILKTEST_PRINTF(2, "Wkr %d: test_pipe body: i = %d, sum=%d\n",
                   __cilkrts_get_worker_number(),
                   i, sum);
            cilk_ms_sleep(0);
        }

        // Check last two terms in the pedigree.
        print_current_pedigree_with_check("pipe_while", i,
                                          3, 1, ped2.rank + i);
        print_current_pedigree_with_check("pipe_while", i,
                                          3, 2, 0);
        ans = cilk_spawn fib(n);
        cilk_sync;
        
        // After fib, the next to last term is the same, the last term
        // should increment by rank_delta.
        print_current_pedigree_with_check("pipe_while_after_inline_spawn", i,
                                          3, 1, ped2.rank + i);
        print_current_pedigree_with_check("pipe_while_after_inline_spawn", i,
                                          3, 2, rank_delta);

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
    CILKTEST_PRINTF(2,
                    "Final sum = %d\n",
                    sum);
    return 0;
}


int main(void) {
    CILKTEST_BEGIN("pipewhile_pedigrees");

    test_pipe_while_macro(1);
    test_pipe_while_macro(10);
    test_pipe_while_macro_with_fib();
    test_pipe_while_macro_with_inline_spawn();
    
    CILKTEST_END("pipewhile_pedigrees");
    return 0;
}

