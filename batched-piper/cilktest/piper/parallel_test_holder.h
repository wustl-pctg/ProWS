/*  parallel_test_holder.h                  -*- C++ -*-
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

/*
 * @File: parallel_test_holder.h
 *
 * @Brief: A simple reducer which computes fib() inside the reduction.
 *
 */

#ifndef __PARALLEL_TEST_HOLDER_H_
#define __PARALLEL_TEST_HOLDER_H_

#include <cstdio>
#include <cstdlib>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer_opadd.h>
#include "cilktest_harness.h"
#include "cilktest_timer.h"


namespace cilk {
    class parallel_test_holder 
    {
    public:

    	class MyMonoid : public monoid_base<int>
    	{

	private:
	    static int test_fib(int n) {
		if (n < 2) {
		    return n;
		}
		else {
		    int x=0, y=0;
		    x = _Cilk_spawn test_fib(n-1);
		    y = test_fib(n-2);
		    _Cilk_sync;
		    return (x+y);
		}
	    };

    	public:
    	    static void reduce(int* left, int* right) {
    		int n = *right;
    		int ans;
                double start, end;

                CILKTEST_PRINTF(2,
                                "## Parallel reduce right holder: start --- *left = %d (%p), *right=%d (%p)\n",
                                *left, left, *right, right);
                if (*right > 50) {
                    CILKTEST_PRINTF(2,
                                    "## ERROR reducer computing with value *right = %d that is too large...\n",
                                    *right);
                    TEST_ASSERT(0);
                }
		start = Cilk_get_wall_time();
		ans = test_fib(n);
		end = Cilk_get_wall_time();

		CILKTEST_PRINTF(2,
                                "## Parallel reduce right holder: end --- Fib(%d) = %d, P=%d, Time=%f ms\n",
                                n, ans,
                                __cilkrts_get_nworkers(),
                                1.0*(end - start));
    		*left = *right;
    	    }
    	};

    	parallel_test_holder() : imp_(int())
    	{
    	}

	int get_value() {
	    return imp_.view();
	}

	void set_value(int x) {
	    imp_.view() = x;
	}

    private:
    	reducer<MyMonoid> imp_;
    };
}

#endif  // PARALLEL_TEST_HOLDER
