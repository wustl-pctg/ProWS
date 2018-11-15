/* os.h                  -*-C++-*-
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
 * @file os.h
 *
 * @brief Low-level operating-system dependent facilities, not dependent on
 * any Cilk facilities.
 */

#ifndef INCLUDED_OS_DOT_H
#define INCLUDED_OS_DOT_H

#include "rts-common.h"
#include "cilk/common.h"
#include "cilk-tbb-interop.h"

#ifdef __cplusplus
#   include <cstddef>
#else
#   include <stddef.h>
#endif

__CILKRTS_BEGIN_EXTERN_C


// /* Thread-local storage */
// #ifdef _WIN32
// typedef unsigned cilkos_tls_key_t;
// #else
// typedef pthread_key_t cilkos_tls_key_t;
// #endif
// cilkos_tls_key_t cilkos_allocate_tls_key();
// void cilkos_set_tls_pointer(cilkos_tls_key_t key, void* ptr);
// void* cilkos_get_tls_pointer(cilkos_tls_key_t key);

/* The RTS assumes that some thread-local state exists that stores the
   worker and reducer map currently associated with a thread.  These routines
   manipulate this state. */

/** @brief Thread-local state for cilk fibers. */
typedef struct cilk_fiber_sysdep cilk_fiber_sysdep;

/** @brief Initialize  all TLS variables for Cilk. */
COMMON_SYSDEP void __cilkrts_init_tls_variables(void);

/** @brief Set worker struct in TLS. */
COMMON_SYSDEP
void __cilkrts_set_tls_worker(__cilkrts_worker *w) cilk_nothrow;

/** @brief Get stack_op for TBB-interop structures from TLS. */
COMMON_SYSDEP
__cilk_tbb_stack_op_thunk *__cilkrts_get_tls_tbb_interop(void);

/** @brief Set stack_op for TBB-interop structures in TLS. */
COMMON_SYSDEP
void __cilkrts_set_tls_tbb_interop(__cilk_tbb_stack_op_thunk *t);

/**
 * @brief Get the pointer to the pedigree leaf node from TLS.
 *
 * Function to get a pointer to the thread's pedigree leaf node.  This
 * pointer can be NULL.
 */
COMMON_SYSDEP
__cilkrts_pedigree * __cilkrts_get_tls_pedigree_leaf(int create_new);

/**
 * @brief Sets the pointer to the pedigree leaf node in TLS.
 *
 * If the previous pointer value was not NULL, it is the caller's
 * responsibility to ensure that previous pointer value is saved and
 * freed.
 *
 * @param pedigree_leaf The leaf node to store into TLS.
 */ 
COMMON_SYSDEP
void __cilkrts_set_tls_pedigree_leaf(__cilkrts_pedigree* pedigree_leaf);


#if SUPPORT_GET_CURRENT_FIBER > 0
/**
 * @brief Get the cilk_fiber from TLS. 
 */
COMMON_SYSDEP
cilk_fiber_sysdep* cilkos_get_tls_cilk_fiber(void);

/**
 * @brief Set the cilk_fiber in TLS.
 *
 * @param fiber The fiber to store into TLS. 
 */
COMMON_SYSDEP
void cilkos_set_tls_cilk_fiber(cilk_fiber_sysdep* fiber);
#endif

/**
 * @brief Function for returning the current thread id.
 * @warning This function is useful for debugging purposes only.
 */
COMMON_SYSDEP
void* cilkos_get_current_thread_id(void);

/** @brief Return number of CPUs supported by this hardware, using whatever definition
   of CPU is considered appropriate. */
COMMON_SYSDEP int __cilkrts_hardware_cpu_count(void);

/** @brief Get current value of timer */
COMMON_SYSDEP unsigned long long __cilkrts_getticks(void);

/* Machine instructions */

/// Stall execution for a few cycles.
COMMON_SYSDEP void __cilkrts_short_pause(void);
/// Wrapper for xchg instruction
COMMON_SYSDEP int __cilkrts_xchg(volatile int *ptr, int x);

/* gcc before 4.4 does not implement __sync_synchronize properly */
#if (__ICC >= 1110 && !(__MIC__ || __MIC2__))                      \
    || (!defined __ICC && __GNUC__ * 10 + __GNUC_MINOR__ > 43)
#   define HAVE_SYNC_INTRINSICS 1
#endif

/*
 * void __cilkrts_fence(void)
 *
 * Executes an MFENCE instruction to serialize all load and store instructions
 * that were issued prior the MFENCE instruction. This serializing operation
 * guarantees that every load and store instruction that precedes the MFENCE
 * instruction is globally visible before any load or store instruction that
 * follows the MFENCE instruction. The MFENCE instruction is ordered with
 * respect to all load and store instructions, other MFENCE instructions, any
 * SFENCE and LFENCE instructions, and any serializing instructions (such as
 * the CPUID instruction).
 */
#ifdef HAVE_SYNC_INTRINSICS
#   define __cilkrts_fence() __sync_synchronize()
#elif defined __ICC || defined __GNUC__
    /* mfence is a strict subset of lock add but takes longer on many
     * processors. */
// #   define __cilkrts_fence() __asm__ volatile ("mfence")
    /* On MIC, fence seems to be completely unnecessary.
     * Just for simplicity of 1st implementation, it defaults to x86 */ 
#   define __cilkrts_fence() __asm__ volatile ("lock addl $0,(%rsp)")
// #elif defined _WIN32
// #   pragma intrinsic(_ReadWriteBarrier)
// #   define __cilkrts_fence() _ReadWriteBarrier()
#else
COMMON_SYSDEP void __cilkrts_fence(void); ///< MFENCE instruction
#endif

COMMON_SYSDEP void __cilkrts_sleep(void); ///< Sleep briefly 
COMMON_SYSDEP void __cilkrts_yield(void); ///< Yield quantum 

/**
 * @brief Gets environment variable 'varname' and copy its value into 'value'.
 *
 * If the entire value, including the null terminator fits into 'vallen'
 * bytes, then returns the length of the value excluding the null.  Otherwise,
 * leaves the contents of 'value' undefined and returns the number of
 * characters needed to store the environment variable's value, *including*
 * the null terminator.
 *
 * @param value    Buffer to store value.
 * @param vallen   Length of value buffer
 * @param varname  Name of the environment variable.
 * @return         Length of value buffer (excluding the null).
 */
COMMON_SYSDEP __STDNS size_t cilkos_getenv(char* value, __STDNS size_t vallen,
                                           const char* varname);

/**
 * @brief Unrecoverable error: Print an error message and abort execution.
 */
COMMON_SYSDEP void cilkos_error(const char *fmt, ...);

/**
 * @brief Print a warning message and return.
 */
COMMON_SYSDEP void cilkos_warning(const char *fmt, ...);

/**
 * @brief Print a labeled message and return.
 */
COMMON_SYSDEP void cilkos_message(const char* label, const char *fmt, ...);


/**
 * @brief Convert the user's specified stack size into a "reasonable"
 * value for the current OS.
 *
 * @param specified_stack_size   User-specified stack size.
 * @return New stack size value, modified for the OS.
 */
COMMON_SYSDEP size_t cilkos_validate_stack_size(size_t specified_stack_size);

/**
 * @brief Atomic addition: computes *p += x.
 * 
 * @param p  Pointer to value to update
 * @param x  Value of x.
 */
COMMON_SYSDEP long cilkos_atomic_add(volatile long* p, long x);

#ifdef _WIN32

/**
 * @brief Windows-only low-level functions for processor groups.
 */
typedef struct _GROUP_AFFINITY GROUP_AFFINITY;

/**
 * @brief Probe the executing OS to see if it supports processor
 * groups.  These functions are expected to be available in Windows 7
 * or later.
 */
void win_init_processor_groups(void);

unsigned long win_get_active_processor_count(unsigned short GroupNumber);
unsigned short win_get_active_processor_group_count(void);
int win_set_thread_group_affinity(/*HANDLE*/ void* hThread,
                                  const GROUP_AFFINITY *GroupAffinity,
                                  GROUP_AFFINITY* PreviousGroupAffinity);

/**
 * @brief Cleans up any state allocated in TLS.
 *
 * Only defined for Windows because Linux calls destructors for each
 * thread-local variable.
 */
void __cilkrts_per_thread_tls_cleanup(void);


#endif // _WIN32

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_OS_DOT_H)
