/*  sequencer.h                  -*- C++ -*-
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
 * @File: sequencer.h
 *
 * @Brief: A object for forcing interleavings of parallel executions.
 *
 */
#ifndef __SEQUENCER_H_
#define __SEQUENCER_H_

#include <cstdio>
#include <cstdlib>
#include <cilk/cilk_api.h>

#include "cilktest_harness.h"


class Sequencer {

    volatile int m_sequence_num;
    
public:
    Sequencer()
        : m_sequence_num(0)
    {
    }

    void wait_for_num(int seq, unsigned delay_count, const char *msg) {
        __cilkrts_worker *w = __cilkrts_get_tls_worker();
        CILKTEST_PRINTF(3,
                        "## ****SEQ**** W=%d: waiting [%s], seq %d.  Current = %d\n",
                        w ? w->self : -1,
                        msg,
                        seq,
                        m_sequence_num);

        while (seq != m_sequence_num) {
            cilk_ms_sleep(2);
        }
        TEST_ASSERT(seq == m_sequence_num);
        m_sequence_num = seq + 1;


        while (delay_count > 0) {
            cilk_ms_sleep(2);
            delay_count--;
        }
        CILKTEST_PRINTF(3,
                        "## ****SEQ**** W=%d. finished [%s], seq %d.  Current = %d\n",
                        w ? w->self : -1,
                        msg,
                        seq,
                        m_sequence_num);
    }

    void wait_for_num(int seq, unsigned delay_count) {
        wait_for_num(seq, delay_count, "generic");
    }
};

#endif // __SEQUENCER_H_
