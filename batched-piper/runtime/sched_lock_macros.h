/* sched_lock_macros.h                  -*-C-*-
 *
 *************************************************************************
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
 *
 **************************************************************************/

/**
 * @file sched_lock_macros.h
 *
 * @brief Common preprocessor macros for locking various data
 * structures.
 */

#ifndef INCLUDED_SCHED_LOCK_MACROS_DOT_H
#define INCLUDED_SCHED_LOCK_MACROS_DOT_H


/* Lock macro Usage:
    BEGIN_WITH_WORKER_LOCK(w) {
        statement;
        statement;
        BEGIN_WITH_FRAME_LOCK(w, ff) {
            statement;
            statement;
        } END_WITH_FRAME_LOCK(w, ff);
    } END_WITH_WORKER_LOCK(w);
 */

/// Acquire of worker lock.
#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
/// Release of worker lock. 
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)

// TBD(jsukha): These are worker lock acquistions on
// a worker whose deque is empty.  My conjecture is that we
// do not need to hold the worker lock at these points.
// I have left them in for now, however.
//
// #define REMOVE_POSSIBLY_OPTIONAL_LOCKS
#ifdef REMOVE_POSSIBLY_OPTIONAL_LOCKS
    /// Omitted acquire of worker lock. 
    #define BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) do
    /// Omitted release of worker lock. 
    #define END_WITH_WORKER_LOCK_OPTIONAL(w)   while (0)
#else
    /// Acquire of worker lock. 
    #define BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) __cilkrts_worker_lock(w); do
    /// Release of worker lock. 
    #define END_WITH_WORKER_LOCK_OPTIONAL(w)   while (__cilkrts_worker_unlock(w), 0)
#endif

/// Acquire of frame lock. 
#define BEGIN_WITH_FRAME_LOCK(w, ff)                                     \
    do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

/// Release of frame lock. 
#define END_WITH_FRAME_LOCK(w, ff)                       \
    while (__cilkrts_frame_unlock(w, _locked_ff), 0); } while (0)

//#define DEBUG_LOCKS 1
#ifdef DEBUG_LOCKS
/// Check that the currently executing worker owns this worker's lock.
#   define ASSERT_WORKER_LOCK_OWNED(w) \
        { \
            __cilkrts_worker *tls_worker = __cilkrts_get_tls_worker(); \
            CILK_ASSERT((w)->l->lock.owner == tls_worker); \
        }
#else
/// Null check of owner of worker lock.
#   define ASSERT_WORKER_LOCK_OWNED(w)
#endif // DEBUG_LOCKS

/// Acquire of lock on data block.
#define BEGIN_WITH_PIPE_DATA_LOCK(iter_data)  spin_mutex_lock(&(iter_data)->iter_ff_link->pipe_data_lock); do

/// Release of lock on data block.
#define END_WITH_PIPE_DATA_LOCK(iter_data)    while (spin_mutex_unlock(&((iter_data)->iter_ff_link->pipe_data_lock)), 0)

/// Aqcuire of lock on data block, if @c need_lock is true.
#define BEGIN_WITH_PIPE_DATA_LOCK_CONDITIONAL(iter_data, need_lock)    \
    if (need_lock) {                                                   \
        spin_mutex_lock(&((iter_data)->iter_ff_link->pipe_data_lock)); \
    }                                                                  \
    do

/// Release of lock on data block, if @c need_lock is true.
#define END_WITH_PIPE_DATA_LOCK_CONDITIONAL(iter_data, need_lock)    \
    while ( (need_lock ? (spin_mutex_unlock(&((iter_data)->iter_ff_link->pipe_data_lock)), 0) : 0) )

#endif // ! defined(INCLUDED_SCHED_LOCK_MACROS_DOT_H)
