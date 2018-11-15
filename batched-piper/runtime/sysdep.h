/* sysdep.h                  -*-C++-*-
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
 * @file sysdep.h
 *
 * @brief Common system-dependent functions
 */

#ifndef INCLUDED_SYSDEP_DOT_H
#define INCLUDED_SYSDEP_DOT_H

#include <cilk/common.h>
#include <internal/abi.h>

#include "global_state.h"
#include "full_frame.h"
#include "os.h"
#include "os_mutex.h"

/**
 * @brief Default page size for Cilk stacks.
 *
 * All Cilk stacks should have size that is a multiple of this value.
 */
#define PAGE 4096

/**
 * @brief Size of a scheduling stack.
 *
 * A scheduling stack is used to by system workers to execute runtime
 * code.  Since this stack is only executing runtime functions, we
 * don't need it to be a full size stack.
 *
 * The number "18" should be small since the runtime doesn't require a
 * large stack, but large enough to call "printf" for debugging.
 */ 
#define CILK_SCHEDULING_STACK_SIZE (18*PAGE)

__CILKRTS_BEGIN_EXTERN_C


/**
 * Code to initialize the system-dependent portion of the global_state_t
 *
 * @param g Pointer to the global state.
 */
COMMON_SYSDEP
void __cilkrts_init_global_sysdep(global_state_t *g);

/**
 * Code to clean up the system-dependent portion of the global_state_t
 *
 * @param g Pointer to the global state.
 */
COMMON_SYSDEP
void __cilkrts_destroy_global_sysdep(global_state_t *g);

/**
 * Passes stack range to Cilkscreen.  This functionality should be moved
 * into Cilkscreen.
 */
COMMON_SYSDEP
void __cilkrts_establish_c_stack(void);


/**
 * Save system dependent information in the full_frame and
 * __cilkrts_stack_frame.  Part of promoting a
 * __cilkrts_stack_frame to a full_frame.
 *
 * @param w The worker the frame was running on.  Not used.
 * @param ff The full frame that is being created for the
 * __cilkrts_stack_frame.
 * @param sf The __cilkrts_stack_frame that's being promoted
 * to a full frame.
 * @param state_valid ?
 * @param why A description of why make_unrunnable was called.
 * Used for debugging.
 */
COMMON_SYSDEP
void __cilkrts_make_unrunnable_sysdep(__cilkrts_worker *w,
                                      full_frame *ff,
                                      __cilkrts_stack_frame *sf,
                                      int state_valid,
                                      const char *why);


/**
 * OS-specific code to spawn worker threads.
 *
 * @param g The global state.
 * @param n Number of worker threads to start.
 */
COMMON_SYSDEP
void __cilkrts_start_workers(global_state_t *g, int n);

/**
 * @brief OS-specific code to stop worker threads.
 *
 * @param g The global state.
 */
COMMON_SYSDEP
void __cilkrts_stop_workers(global_state_t *g);

/**
 * @brief Imports a user thread the first time it returns to a stolen parent.
 *
 * The thread has been bound to a worker, but additional steps need to
 * be taken to start running a scheduling loop.
 *
 * @param w The worker bound to the thread.
 */
COMMON_SYSDEP
void __cilkrts_sysdep_import_user_thread(__cilkrts_worker *w);

/**
 * @brief Function to be run for each of the system worker threads.
 * 
 * This declaration also appears in cilk/cilk_undocumented.h -- don't
 * change one declaration without also changing the other.
 *
 * @param arg The context value passed to the thread creation routine for
 * the OS we're running on.
 *
 * @returns OS dependent.
 */
#ifdef _WIN32
/* Do not use CILK_API because __cilkrts_worker_stub must be __stdcall */
CILK_EXPORT unsigned __CILKRTS_NOTHROW __stdcall
__cilkrts_worker_stub(void *arg);
#else
/* Do not use CILK_API because __cilkrts_worker_stub have default visibility */
__attribute__((visibility("default")))
void* __CILKRTS_NOTHROW __cilkrts_worker_stub(void *arg);
#endif

/**
 * Initialize any OS-depenendent portions of a newly created
 * __cilkrts_worker.
 *
 * Exported for Piersol.  Without the export, Piersol doesn't display
 * useful information in the stack trace.  This declaration also appears in
 * cilk/cilk_undocumented.h -- do not modify one without modifying the other.
 *
 * @param w The worker being initialized.
 */
COMMON_SYSDEP
CILK_EXPORT
void __cilkrts_init_worker_sysdep(__cilkrts_worker *w);

/**
 * Deallocate any OS-depenendent portions of a __cilkrts_worker.
 *
 * @param w The worker being deallocaed.
 */
COMMON_SYSDEP
void __cilkrts_destroy_worker_sysdep(__cilkrts_worker *w);

/**
 * Called to do any OS-dependent setup before starting execution on a
 * frame. Mostly deals with exception handling data.
 *
 * @param w The worker the frame will run on.
 * @param ff The full_frame that is about to be resumed.
 */
COMMON_SYSDEP
void __cilkrts_setup_for_execution_sysdep(__cilkrts_worker *w,
                                          full_frame *ff);

/**
 * @brief OS-specific implementaton of resetting fiber and frame state
 * to resume exeuction.
 *
 * This method:
 *  1. Calculates the value of stack pointer where we should resume
 *     execution of "sf".  This calculation uses info stored in the
 *     fiber, and takes into account alignment and frame size.
 *  2. Updates sf and ff to match the calculated stack pointer.
 *
 *  On Unix, the stack pointer calculation looks up the base of the
 *  stack from the fiber.
 *
 *  On Windows, this calculation is calls "alloca" to find a stack
 *  pointer on the currently executing stack.  Thus, the Windows code
 *  assumes @c fiber is the currently executing fiber.
 *
 * @param fiber   fiber to resume execution on.
 * @param ff      full_frame for the frame we're resuming.
 * @param sf      __cilkrts_stack_frame that we should resume
 * @return    The calculated stack pointer.
 */
COMMON_SYSDEP
char* sysdep_reset_jump_buffers_for_resume(cilk_fiber* fiber,
                                           full_frame *ff,
                                           __cilkrts_stack_frame *sf);

/**
 * @brief System-dependent longjmp to user code for resuming execution
 *   of a @c __cilkrts_stack_frame.
 *
 * This method:
 *  - Changes the stack pointer in @c sf to @c new_sp.
 *  - If @c ff_for_exceptions is not NULL, changes fields in @c sf and
 *    @c ff_for_exceptions for exception processing.
 *  - Restores any floating point state
 *  - Finishes with a longjmp to user code, never to return. 
 *
 * @param new_sp             stack pointer where we should resume execution
 * @param sf                 @c __cilkrts_stack_frame for the frame we're resuming.
 * @param ff_for_exceptions  full_frame to safe exception info into, if necessary
 */
COMMON_SYSDEP
NORETURN
sysdep_longjmp_to_sf(char* new_sp,
                     __cilkrts_stack_frame *sf,
                     full_frame *ff_for_exceptions);

/**
 * @brief Pin the currently executing worker to a thread.
 *
 * This method is not currently implemented on Windows.
 */
COMMON_SYSDEP
void set_current_worker_affinity_sysdep(__cilkrts_worker *w);


/**
 * @brief Pin the currently executing worker to a thread.
 *
 * This method is not currently implemented on Windows.
 */
COMMON_SYSDEP
void set_current_worker_affinity_sysdep(__cilkrts_worker *w);

/**
 * @brief System-dependent code to save floating point control information
 * to a @c __cilkrts_stack_frame.  This function will be called by compilers
 * that cannot inline the code.
 *
 * Note that this function does *not* save the current floating point
 * registers.  It saves the floating point control words that control
 * precision and rounding and stuff like that.
 *
 * This function will be a noop for architectures that don't have warts
 * like the floating point control words, or where the information is
 * already being saved by the setjmp.
 *
 * @param sf                 @c __cilkrts_stack_frame for the frame we're
 * saving the floating point control information in.
 */
COMMON_SYSDEP
void
sysdep_save_fp_ctrl_state(__cilkrts_stack_frame *sf);

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_SYSDEP_DOT_H)
