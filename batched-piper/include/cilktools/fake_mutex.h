/* fake_mutex.h                  -*-C++-*-
 *
 *************************************************************************
 *
 * @copyright
 * Copyright (C) 2013
 * Intel Corporation
 * 
 * @copyright
 * This file is part of the Intel Cilk Plus Library.  This library is free
 * software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * @copyright
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * @copyright
 * Under Section 7 of GPL version 3, you are granted additional
 * permissions described in the GCC Runtime Library Exception, version
 * 3.1, as published by the Free Software Foundation.
 * 
 * @copyright
 * You should have received a copy of the GNU General Public License and
 * a copy of the GCC Runtime Library Exception along with this program;
 * see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 **************************************************************************
 *
 * Cilkscreen fake mutexes are provided to indicate to the Cilkscreen race
 * detector that a race should be ignored.
 *
 * NOTE: This class does not provide mutual exclusion.  You should use the
 * mutual exclusion constructs provided by TBB or your operating system to
 * protect against real data races.
 */

#ifndef FAKE_MUTEX_H_INCLUDED
#define FAKE_MUTEX_H_INCLUDED

#include <cilktools/cilkscreen.h>

namespace cilkscreen
{
    class fake_mutex
    {
    public:
	fake_mutex() : locked(false)
	{
	}

	~fake_mutex()
	{
	    __CILKRTS_ASSERT(! locked);
	}

        // Wait until mutex is available, then enter
        void lock()
        {
            __cilkscreen_acquire_lock(&locked);
	    __CILKRTS_ASSERT(! locked);
	    locked = true;
        }

        // A fake mutex is always available
        bool try_lock() { lock(); return true; }

        // Releases the mutex
        void unlock()
        {
	    __CILKRTS_ASSERT(locked);
	    locked = false;
            __cilkscreen_release_lock(&locked);
        }

    private:
        bool locked;
    };

} // namespace cilk

#endif  // FAKE_MUTEX_H_INCLUDED
