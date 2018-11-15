/*  sample_grid.cpp                  -*- C++ -*-
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
 * @file: sample_grid.cpp
 *
 * @brief: A sample program executing a simple dynamic program on a
 * 2-d grid n by m grid.
 *
 * The computation of GRID(i, j) depends on GRID(i-1, j), GRID(i,
 * j-1), and GRID(i-1, j-1).
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


#define GRID(i, j) grid[(i)*m + (j)]

inline void compute_point(int* grid, int n, int m, int i, int j) {
    if (i == 0) {
        GRID(i, j) = j;
    } else if (j == 0) {
        GRID(i, j) = i;
    } else {
        GRID(i, j) = GRID(i, j-1) + GRID(i-1, j) + GRID(i-1, j-1);
    }
}

inline void compute_block(int* grid, int n, int m,
                          int start_i, int stop_i,
                          int start_j, int stop_j) {
    for (int i = start_i; i < stop_i; ++i) {
        for (int j = start_j; j < stop_j; ++j) {
            compute_point(grid, n, m, i, j);
        }
    }
}

// Serial version of the computation.
int serial_grid_test(int* grid,
                     int n, int m) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            compute_point(grid, n, m, i, j);
        }
    }
    return GRID(n-1, m-1);
}

// Simple pipeline loop version, no blocking.
int run_grid_test(int* grid,
                  int n, int m) {
    int i_loop = 0;
    CILK_PIPE_WHILE_BEGIN(i_loop < n) {
        int i = i_loop++;
        for (int j = 0; j < m; ++j) {
            compute_point(grid, n, m, i, j);
            CILK_STAGE_WAIT(j+1);
        }
    } CILK_PIPE_WHILE_END();
    return GRID(n-1, m-1);
}

// Blocked pipe-while loop.
int run_grid_test_blocked(int* grid,
                          int n, int m,
                          int Bn, int Bm) {
    int block_i_loop = 0;
    CILK_PIPE_WHILE_BEGIN(block_i_loop < n) {
        int block_i = block_i_loop;
        block_i_loop += Bn;
        int stop_i = ((block_i + Bn) < n) ? (block_i + Bn) : n;
        for (int block_j = 0; block_j < m; block_j += Bm) {
            int stop_j = ((block_j + Bm) < m) ? (block_j + Bm) : m;
            // Process a block.
            compute_block(grid, n, m,
                          block_i, stop_i,
                          block_j, stop_j);

            CILK_STAGE_WAIT(block_j + Bm);
        }
    } CILK_PIPE_WHILE_END();
    return GRID(n-1, m-1);
}


// Helper method for a 2-day divide-and-conquer algorithm for
// executing this 2d grid.
template <int Bn, int Bm, typename BlockFunc>
void run_grid_blocked_divide_and_conquer_helper(const BlockFunc& block_func,
                                                int start_i, int stop_i,
                                                int start_j, int stop_j)
{
    bool base_i = ((stop_i - start_i) <= Bn);
    bool base_j = ((stop_j - start_j) <= Bm);

    if (base_i && base_j) {
        // Don't need to split either. Execute base case.
        block_func(start_i, stop_i, start_j, stop_j);
    } else if (base_j) {
        // Split only i.  Make sure we round left piece to be in block
        // size Bn.
        int split_i = (start_i + stop_i)/2;
        if (split_i % Bn) {
            split_i += Bn - (split_i % Bn);
        }
        run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             start_i, split_i,
             start_j, stop_j);
        run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             split_i, stop_i,
             start_j, stop_j);
    } else if (base_i) {
        // Split only j.
        int split_j = (start_j + stop_j)/2;
        if (split_j % Bm) {
            split_j += Bm - (split_j % Bm);
        }
        run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             start_i, stop_i,
             start_j, split_j);
        run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             start_i, stop_i,
             split_j, stop_j);
    } else {
        // Recursively split on both.
        int split_i = (start_i + stop_i)/2;
        if (split_i % Bn) {
            split_i += Bn - (split_i % Bn);
        }
        int split_j = (start_j + stop_j)/2;
        if (split_j % Bm) {
            split_j += Bm - (split_j % Bm);
        }

        // Execute upper left corner.
        run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             start_i, split_i,
             start_j, split_j);

        // Execute the upper right corner and lower left corner in
        // parallel.
        cilk_spawn run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             split_i, stop_i,
             start_j, split_j);
        run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             start_i, split_i,
             split_j, stop_j);
        cilk_sync;

        // Finally, execute the lower right corner.
        run_grid_blocked_divide_and_conquer_helper<Bn, Bm, BlockFunc>
            (block_func,
             split_i, stop_i,
             split_j, stop_j);
    }
}

template <int Bn, int Bm>
int run_grid_blocked_divide_and_conquer(int* grid,
                                        int n, int m)
{
    auto block_func =
        [grid, n, m](int start_i, int stop_i,
                     int start_j, int stop_j) {
        compute_block(grid, n, m, start_i, stop_i, start_j, stop_j);
    };

    run_grid_blocked_divide_and_conquer_helper<Bn, Bm>(block_func,
                                                       0, n,
                                                       0, m);
    return GRID(n-1, m-1);
}



int main(int argc, char* argv[]) {
    int P = __cilkrts_get_nworkers();
    int NUM_TRIALS = 10;
    unsigned long long tm_begin, tm_elapsed;
    int result = 0;

    int n = 10000;
    int m = 5000;
    int Bn = 64;
    int Bm = 64;

    // Parse n and m if arguments are specified.
    if (argc >= 2) {
        n = std::atoi(argv[1]);
    }
    if (argc >= 3) {
        m = std::atoi(argv[2]);
    }
    
    std::printf("Sample: testing 2d grid n by m = (%d by %d), blocks = (%d by %d)\n",
                n, m,
                Bn, Bm);

    int* grid = new int[n * m];
    int serial_ans = serial_grid_test(grid, n, m);
    
    // Warmup runs.
    for( int trial=0; trial< 5; ++trial ) {
        int ans = run_grid_test(grid, n, m);
        assert(ans == serial_ans);
        result += ans;
    }

    // Timing runs for divide-and-conquer version.
    const int BN = 64;  // Is a constant for template arguments.
    const int BM = 64;  // Is a constant for template arguments.
    result = 0;
    tm_begin = Cilk_get_wall_time();
    for( int trial=0; trial<NUM_TRIALS; ++trial ) {
        int ans = run_grid_blocked_divide_and_conquer<BN, BM>(grid, n, m);
        assert(ans == serial_ans);
        result += ans;
    }
    tm_elapsed = Cilk_get_wall_time() - tm_begin;
    std::printf("P=%d, time in seconds=%f, [%d by %d, divide-and-conquer, blocks are %d by %d, result=%d]\n",
                P,
                1.0e-3 * tm_elapsed,
                n, m,
                BN, BM,
                result);

    // Timing runs for unblocked.
    result = 0;
    tm_begin = Cilk_get_wall_time();
    for( int trial=0; trial<NUM_TRIALS; ++trial ) {
        int ans = run_grid_test(grid, n, m);
        assert(ans == serial_ans);
        result += ans;
    }
    tm_elapsed = Cilk_get_wall_time() - tm_begin;
    std::printf("P=%d, time in seconds=%f, [%d by %d, pipe-while, no blocking, result=%d]\n",
                P,
                1.0e-3 * tm_elapsed,
                n, m,
                result);

    // Timing runs for blocked version.
    result = 0;
    tm_begin = Cilk_get_wall_time();
    for( int trial=0; trial<NUM_TRIALS; ++trial ) {
        int ans = run_grid_test_blocked(grid, n, m, Bn, Bm);
        assert(ans == serial_ans);
        result += ans;
    }
    tm_elapsed = Cilk_get_wall_time() - tm_begin;
    std::printf("P=%d, time in seconds=%f, [%d by %d, pipe-while, blocks are %d by %d, result=%d]\n",
                P,
                1.0e-3 * tm_elapsed,
                n, m,
                Bn, Bm,
                result);

    delete grid;

    std::printf("PASSED\n");
    return 0;
}
