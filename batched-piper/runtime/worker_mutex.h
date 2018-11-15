/* worker_mutex.h                  -*-C++-*-
 *
 *************************************************************************
 *
 *  @copyright
 *  Copyright (C) 2009-2011
 *  Intel Corporation
 *  
 *  @copyright
 *  This file is part of the Intel Cilk Plus Library.  This library is free
 *  software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *  
 *  @copyright
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  @copyright
 *  Under Section 7 of GPL version 3, you are granted additional
 *  permissions described in the GCC Runtime Library Exception, version
 *  3.1, as published by the Free Software Foundation.
 *  
 *  @copyright
 *  You should have received a copy of the GNU General Public License and
 *  a copy of the GCC Runtime Library Exception along with this program;
 *  see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
 *  <http://www.gnu.org/licenses/>.
 **************************************************************************/

/**
 * @file worker_mutex.h
 *
 * @brief Support for Cilk runtime mutexes.
 *
 * Cilk runtime mutexes are implemented as simple spin loops.
 */

#ifndef INCLUDED_WORKER_MUTEX_DOT_H
#define INCLUDED_WORKER_MUTEX_DOT_H

#include <cilk/common.h>
#include "rts-common.h"

__CILKRTS_BEGIN_EXTERN_C

/**
 * Mutexes are treated as an abstract data type within the Cilk
 * runtime system.  They are implemented as simple spin loops and
 * owned by a __cilkrts_worker.
 */
typedef struct mutex {
    /** Mutex spin loop variable. 0 if unowned, 1 if owned. */
    volatile int lock;

    /** Worker that owns the mutex.  Must be 0 if mutex is unowned. */
    __cilkrts_worker *owner;
} mutex;

/**
 * @brief Initialize a Cilk mutex.
 *
 * @param m Mutex to be initialized.
 */
COMMON_PORTABLE
void __cilkrts_mutex_init(struct mutex *m);

/**
 * @brief Acquire a Cilk mutex.
 *
 * If statistics are being gathered, the time spent
 * acquiring the mutex will be attributed to the specified worker.
 *
 * @param w Worker that will become the owner of this mutex.
 * @param m Mutex to be initialized.
 */
COMMON_PORTABLE
void __cilkrts_mutex_lock(__cilkrts_worker *w,
                          struct mutex *m);
/**
 * @brief Attempt to lock a Cilk mutex and fail if it isn't available.
 *
 * If statistics are being gathered, the time spent acquiring the
 * mutex will be attributed to the specified worker.
 *
 * @param w Worker that will become the owner of this mutex.
 * @param m Mutex to be acquired.
 *
 * @return 1 if the mutex was acquired.
 * @return 0 if the mutex was not acquired.
 */
COMMON_PORTABLE
int __cilkrts_mutex_trylock(__cilkrts_worker *w,
                            struct mutex *m);

/**
 * @brief Release a Cilk mutex.
 * 
 * If statistics are being gathered, the time spent
 * acquiring the mutex will be attributed to the specified worker.
 *
 * @pre The mutex must be owned by the worker.
 *
 * @param w Worker that owns this mutex.
 * @param m Mutex to be released.
 */
COMMON_PORTABLE
void __cilkrts_mutex_unlock(__cilkrts_worker *w,
                            struct mutex *m);

/**
 * @brief Try to speculatively acquire a Cilk mutex.
 *
 * Same as __cilkrts_mutex_lock, except try to use hardware lock
 * elision / transactional memory mechanisms.
 */
COMMON_PORTABLE
void __cilkrts_mutex_lock_speculative(__cilkrts_worker *w,
                                      struct mutex *m);
/**
 * @brief Speculatively attempt to lock a Cilk mutex and fail if it
 * isn't available.
 *
 * Same as __cilkrts_mutex_trylock, except try to use hardware lock
 * elision / transactional memory mechanisms.
 */
COMMON_PORTABLE
int __cilkrts_mutex_trylock_speculative(__cilkrts_worker *w,
                                        struct mutex *m);

/**
 * @brief Release a Cilk mutex.
 *
 * Same as __cilkrts_mutex_unlock, except try to use hardware lock
 * elision / transactional memory mechanisms.
 */
COMMON_PORTABLE
void __cilkrts_mutex_unlock_speculative(__cilkrts_worker *w,
                                        struct mutex *m);


/**
 * @brief Deallocate a Cilk mutex.  Currently does nothing.
 *
 * @param w Unused.
 * @param m Mutex to be deallocated.
 */
COMMON_PORTABLE
void __cilkrts_mutex_destroy(__cilkrts_worker *w,
                             struct mutex *m);

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_WORKER_MUTEX_DOT_H)
