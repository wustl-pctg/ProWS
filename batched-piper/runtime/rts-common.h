/* rts-common.h                  -*-C++-*-
 *
 *************************************************************************
 *
 *  @copyright
 *  Copyright (C) 2009-2013
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

#ifndef INCLUDED_RTS_COMMON_DOT_H
#define INCLUDED_RTS_COMMON_DOT_H

/* Abbreviations API functions returning different types.  By using these
 * abbreviations instead of using CILK_API(ret) directly, etags and other
 * tools can more easily recognize function signatures.
 */
#define CILK_API_VOID          CILK_API(void)
#define CILK_API_VOID_PTR      CILK_API(void*)
#define CILK_API_INT           CILK_API(int)
#define CILK_API_SIZET         CILK_API(size_t)
#define CILK_API_TBB_RETCODE   CILK_API(__cilk_tbb_retcode)
#define CILK_API_PEDIGREE      CILK_API(__cilkrts_pedigree) 

/* Abbreviations ABI functions returning different types.  By using these
 * abbreviations instead of using CILK_ABI(ret) directly, etags and other
 * tools can more easily recognize function signatures.
 */
#define CILK_ABI_VOID        CILK_ABI(void)
#define CILK_ABI_WORKER_PTR  CILK_ABI(__cilkrts_worker_ptr)
#define CILK_ABI_THROWS_VOID CILK_ABI_THROWS(void)
#define CILK_ABI_PIPE_DATA_PTR CILK_ABI(__cilkrts_pipe_iter_data_ptr)
/* documentation aid to identify portable vs. nonportable
   parts of the runtime.  See README for definitions. */
#define COMMON_PORTABLE
#define COMMON_SYSDEP
#define NON_COMMON

#if !(defined __GNUC__ || defined __ICC)
#   define __builtin_expect(a_, b_) a_
#endif

#ifdef __cplusplus
#   define cilk_nothrow throw()
#else
#   define cilk_nothrow /*empty in C*/
#endif

#ifdef __GNUC__
#   define NORETURN void __attribute__((noreturn))
#else
#   define NORETURN void __declspec(noreturn)
#endif

#ifdef __GNUC__
#   define NOINLINE __attribute__((noinline))
#else
#   define NOINLINE __declspec(noinline)
#endif

#ifndef __GNUC__
#   define __attribute__(X)
#endif

/* Microsoft CL accepts "inline" for C++, but not for C.  It accepts 
 * __inline for both.  Intel ICL accepts inline for C of /Qstd=c99
 * is set.  The Cilk runtime is assumed to be compiled with /Qstd=c99
 */
#if defined(_MSC_VER) && ! defined(__INTEL_COMPILER)
#   error define inline
#   define inline __inline
#endif

/* Compilers that build the Cilk runtime are assumed to know about zero-cost
 * intrinsics (__notify_intrinsic()).  For those that don't, #undef the
 * following definition:
 */
#define ENABLE_NOTIFY_ZC_INTRINSIC 1

#if defined(__INTEL_COMPILER)
/* The notify intrinsic was introduced in ICC 12.0. */
#   if __INTEL_COMPILER <= 1200
#       undef ENABLE_NOTIFY_ZC_INTRINSIC
#   endif
#elif defined(__VXWORKS__)
#   undef ENABLE_NOTIFY_ZC_INTRINSIC
#elif defined(__clang__)
#   if !defined(__has_extension) || !__has_extension(notify_zc_intrinsic)
#      undef ENABLE_NOTIFY_ZC_INTRINSIC
#   endif
#elif defined(__arm__)
// __notify_zc_intrinsic not yet supported by gcc for ARM
#   undef ENABLE_NOTIFY_ZC_INTRINSIC
#endif

// If ENABLE_NOTIFY_ZC_INTRINSIC is defined, use __notify_zc_intrisic
#ifdef ENABLE_NOTIFY_ZC_INTRINSIC
#   define NOTIFY_ZC_INTRINSIC(annotation, data) \
    __notify_zc_intrinsic(annotation, data)
#else
#   define NOTIFY_ZC_INTRINSIC(annotation, data)
#endif

#endif // ! defined(INCLUDED_RTS_COMMON_DOT_H)
