/* bug.h                  -*-C++-*-
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
 * @file bug.h
 *
 * @brief Support for reporting bugs and debugging.
 */

#ifndef INCLUDED_BUG_DOT_H
#define INCLUDED_BUG_DOT_H

#include "rts-common.h"
#include <cilk/common.h>

__CILKRTS_BEGIN_EXTERN_C

/**
 * Flush all output, write error message to stderr and abort the execution.
 * On Windows the error is also written to the debugger.
 *
 * @param fmt printf-style format string.  Any remaining parameters will be
 * be interpreted based on the format string text.
 */
COMMON_PORTABLE NORETURN __cilkrts_bug(const char *fmt,...) cilk_nothrow;

/// @todo{Why are cilkrts runtime assertions basically always on? Shouldn't they be disabled if NDEBUG is defined?}
#ifndef CILK_ASSERT

/** Standard text for failed assertion */
COMMON_PORTABLE extern const char *const __cilkrts_assertion_failed;

/**
 * Macro to assert an invariant that must be true.  If the statement evalutes
 * to false, __cilkrts_bug will be called to report the failure and terminate
 * the application.
 */
#define CILK_ASSERT(ex)                                                 \
    (__builtin_expect((ex) != 0, 1) ? (void)0 :                         \
     __cilkrts_bug(__cilkrts_assertion_failed, __FILE__, __LINE__,  #ex))

#define CILK_ASSERT_MSG(ex, msg)                                        \
    (__builtin_expect((ex) != 0, 1) ? (void)0 :                         \
     __cilkrts_bug(__cilkrts_assertion_failed, __FILE__, __LINE__,      \
                   #ex "\n    " msg))
#endif  // CILK_ASSERT

/**
 * Assert that there is no uncaught exception.
 *
 * Not valid on Windows or Android.
 *
 * On Android, calling std::uncaught_exception with the stlport library causes
 * a seg fault.  Since we're not supporting exceptions there at this point,
 * just don't do the check.  It works with the GNU STL library, but that's
 * GPL V3 licensed.
 */
COMMON_PORTABLE void cilkbug_assert_no_uncaught_exception(void);
#if defined(_WIN32) || defined(ANDROID)
#  define CILKBUG_ASSERT_NO_UNCAUGHT_EXCEPTION()
#else
#  define CILKBUG_ASSERT_NO_UNCAUGHT_EXCEPTION() \
    cilkbug_assert_no_uncaught_exception()
#endif


/**
 * Call __cilkrts_bug with a standard message that the runtime state is
 * corrupted and the application is being terminated.
 */
COMMON_SYSDEP void abort_because_rts_is_corrupted(void);

// Debugging aids
#ifndef _DEBUG
#       define DBGPRINTF(_fmt, ...)
#elif defined(_WIN32)

/**
 * Write debugging output.  On windows this is written to the debugger.
 *
 * @param fmt printf-style format string.  Any remaining parameters will be
 * be interpreted based on the format string text.
 */
COMMON_SYSDEP void __cilkrts_dbgprintf(const char *fmt,...) cilk_nothrow;

/**
 * Macro to write debugging output which will be elided if this is not a
 * debug build.  The macro is currently always elided on non-Windows builds.
 *
 * @param _fmt printf-style format string.  Any remaining parameters will be
 * be interpreted based on the format string text.
 */
#       define DBGPRINTF(_fmt, ...) __cilkrts_dbgprintf(_fmt, __VA_ARGS__)

#else /* if _DEBUG && !_WIN32 */
    /* Non-Windows debug logging.  Someday we should make GetCurrentFiber()
     * and GetWorkerFiber() do something.
     */
#   include <stdio.h>
    __CILKRTS_INLINE void* GetCurrentFiber() { return 0; }
    __CILKRTS_INLINE void* GetWorkerFiber(__cilkrts_worker* w) { return 0; }
#       define DBGPRINTF(_fmt, ...) fprintf(stderr, _fmt, __VA_ARGS__)
#endif  // _DEBUG

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_BUG_DOT_H)
