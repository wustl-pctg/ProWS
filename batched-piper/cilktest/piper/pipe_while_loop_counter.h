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
 * @file pipe_while_loop_counter.h
 *
 * @brief Wrapping of an integer type, in a struct that is padded.
 *        This integer type will fall on its own cache line,
 *        assuming 64-byte cache lines. 
 */

#ifndef __PIPE_WHILE_LOOP_COUNTER_H_
#define __PIPE_WHILE_LOOP_COUNTER_H_


/// TBD: I'm sure there is a nicer way to guarantee that an int type
/// is padded away from other structures.  But this is a hack that
/// will do for now.
template <typename IntT>                                                                                                                                                                    struct PipeWhileLoopCounter {
    /// Assumed cache line size.
    static const int CACHE_LINE_SIZE = 64;
    
    /// Padding before. 
    char pre_pad[CACHE_LINE_SIZE];

    /// The actual value. 
    IntT val;
    
    /// Padding after.
    char post_pad[CACHE_LINE_SIZE - sizeof(IntT)];

    /// Construct from IntT
    PipeWhileLoopCounter(IntT v) : val(v) { }

    /// Conversion to IntT
    operator IntT() { return val; }
    
    /// Assignment from IntT
    IntT operator=(IntT v) { return (val = v); }

    /// Updates.
    IntT operator++() { return ++val; }
    IntT operator++(int) { return val++; }
    IntT operator+=(IntT v) { return (val += v); }

    IntT operator--() { return --val; }
    IntT operator--(int) { return val--; }
    IntT operator-=(IntT v) { return (val -= v); }

    /// Define comparison operators.
    bool operator< (IntT v) { return (val < v);  }
    bool operator> (IntT v) { return (val > v);  }
    bool operator==(IntT v) { return (val == v); }

    // Define in terms of the other operators.
    bool operator<=(IntT v) { return !((*this) > v); }
    bool operator>=(IntT v) { return !((*this) < v); }
    bool operator!=(IntT v) { return !((*this)  == v); }
};                    


#endif // __PIPE_WHILE_LOOP_COUNTER_H_
