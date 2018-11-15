/* spin_mutex.c                  -*-C-*-
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

#include "spin_mutex.h"
#include "bug.h"
#include "os.h"
#include "stats.h"

// TBD (11/30/12): We should be doing a conditional test-xchg instead
// of an unconditional xchg operation for the spin mutex.

/* m->lock == 1 means that mutex M is locked */
#define TRY_ACQUIRE(m) (__cilkrts_xchg(&(m)->lock, 1) == 0)

/* ICC 11.1+ understands release semantics and generates an
   ordinary store with a software memory barrier. */
#if __ICC >= 1110
#define RELEASE(m) __sync_lock_release(&(m)->lock)
#else
#define RELEASE(m) __cilkrts_xchg(&(m)->lock, 0)
#endif


spin_mutex* spin_mutex_create() 
{
    spin_mutex* mutex = (spin_mutex*)__cilkrts_malloc(sizeof(spin_mutex));
    spin_mutex_init(mutex);
    return mutex;
}

void spin_mutex_init(struct spin_mutex *m)
{
    // Use a simple assignment so Inspector doesn't bug us about the
    // interlocked exchange doing a read of an uninitialized variable.
    // By definition there can't be a race when we're initializing the
    // lock...
    m->lock = 0;
}

void spin_mutex_lock(struct spin_mutex *m)
{
    int count;
    const int maxspin = 1000; /* SWAG */
    if (!TRY_ACQUIRE(m)) {
        count = 0;
        do {
            do {
                __cilkrts_short_pause();
                if (++count >= maxspin) {
                    /* let the OS reschedule every once in a while */
                    __cilkrts_yield();
                    count = 0;
                }
            } while (m->lock != 0);
        } while (!TRY_ACQUIRE(m));
    }
}

int spin_mutex_trylock(struct spin_mutex *m)
{
    return TRY_ACQUIRE(m);
}

void spin_mutex_unlock(struct spin_mutex *m)
{
    RELEASE(m);
}

void spin_mutex_destroy(struct spin_mutex *m)
{
    __cilkrts_free(m);
}

/* End spin_mutex.c */
