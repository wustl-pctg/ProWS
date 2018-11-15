/* spin_mutex.h                  -*-C++-*-
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
 * @file spin_mutex.h
 *
 * @brief Support for Cilk runtime mutexes.
 *
 * Cilk runtime mutexes are implemented as simple spin loops.
 *
 * This file is similar to a worker_mutex, except it does not have an
 * owner field.
 *
 * TBD: This class, worker_mutex, and os_mutex overlap quite a bit in
 * functionality.  Can we unify these mutexes somehow?
 */
#ifndef INCLUDED_SPIN_MUTEX_DOT_H
#define INCLUDED_SPIN_MUTEX_DOT_H

#include <cilk/common.h>
#include "rts-common.h"
#include "cilk_malloc.h"

__CILKRTS_BEGIN_EXTERN_C

/**
 * Mutexes are treated as an abstract data type within the Cilk
 * runtime system.  They are implemented as simple spin loops.
 */
typedef struct spin_mutex {
    /** Mutex spin loop variable. 0 if unowned, 1 if owned. */
    volatile int lock;

    /** Padding so the mutex takes up a cache line. */
    char pad[64/sizeof(int) - 1];
} spin_mutex;


/**
 * @brief Create a new Cilk spin_mutex.
 *
 * @return Returns an initialized spin mutex.  
 */
COMMON_PORTABLE
spin_mutex* spin_mutex_create();

/**
 * @brief Initialize a Cilk spin_mutex.
 *
 * @param m Spin_Mutex to be initialized.
 */
COMMON_PORTABLE
void spin_mutex_init(spin_mutex *m);

/**
 * @brief Acquire a Cilk spin_mutex.
 *
 * If statistics are being gathered, the time spent
 * acquiring the spin_mutex will be attributed to the specified worker.
 *
 * @param m Spin_Mutex to be initialized.
 */
COMMON_PORTABLE
void spin_mutex_lock(struct spin_mutex *m);
/**
 * @brief Attempt to lock a Cilk spin_mutex and fail if it isn't available.
 *
 * @param m Spin_Mutex to be acquired.
 *
 * @return 1 if the spin_mutex was acquired.
 * @return 0 if the spin_mutex was not acquired.
 */
COMMON_PORTABLE
int spin_mutex_trylock(struct spin_mutex *m);

/**
 * @brief Release a Cilk spin_mutex.
 *
 * @param m Spin_Mutex to be released.
 */
COMMON_PORTABLE
void spin_mutex_unlock(struct spin_mutex *m);

/**
 * @brief Deallocate a Cilk spin_mutex.  Currently does nothing.
 *
 * @param m Spin_Mutex to be deallocated.
 */
COMMON_PORTABLE
void spin_mutex_destroy(struct spin_mutex *m);

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_SPIN_MUTEX_DOT_H)
