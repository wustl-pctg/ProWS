/* piper_fake_macros.h                  -*-C++-*-
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
 **************************************************************************/

/**
 * @file piper_fake_macros.h
 *
 * @brief Macros for hand-compilation of a cilk_pipe_while loop.
 */

#ifndef INCLUDED_PIPER_FAKE_MACROS_H_
#define INCLUDED_PIPER_FAKE_MACROS_H_

// alloca is defined in malloc.h on Windows, alloca.h on Linux
#ifndef _MSC_VER
#include <alloca.h>
#else
#include <malloc.h>
// Define offsetof
#include <stddef.h>
#endif

//#include <cilk/abi.h>
#include <internal/abi.h>

/**************************************************************************************/
// Macros and functions that are copied directly over from cilk_fake.h
//
// I am redefining them here, because including cilk_fake.h directly
// has some dependencies on C++'isms / other quirks that the runtime
// compilation seems to be unhappy with.


#define CILK_PIPE_FAKE_VERSION_FLAG (__CILKRTS_ABI_VERSION << 24)


// CILK_PIPE_SAVE_FP is equivalent to CILK_FAKE_SAVE_FP.
// 
/* Save the floating-point control registers.
 * The definition of CILK_FAKE_PIPE_SAVE_FP is compiler specific (and
 * architecture specific on Windows)
 */
#ifdef _MSC_VER
#    define MXCSR_OFFSET offsetof(struct __cilkrts_stack_frame, mxcsr)
#    define FPCSR_OFFSET offsetof(struct __cilkrts_stack_frame, fpcsr)
#    if defined(_M_IX86)
     /* Windows x86 */
     // TBD: The Win32 compiler for some reason still don't like this
     // macro.   But at least it doesn't crash the compiler now...
#       define CILK_PIPE_FAKE_SAVE_FP(sf) do {                          \
            unsigned long mxcsr;                                        \
            unsigned short fpcsr;                                       \
            __asm                                                       \
            {                                                           \
                stmxcsr [mxcsr]                                         \
                fnstcw  [fpcsr]                                         \
            }                                                           \
            (sf).mxcsr = mxcsr;                                         \
            (sf).fpcsr = fpcsr;                                         \
        } while (0)
#    elif defined(_M_X64)
     /* Windows Intel64 - Not needed - saved by setjmp call */
#       define CILK_PIPE_FAKE_SAVE_FP(sf) ((void) sf)
#    else
#       error "Unknown architecture"
#    endif /* Microsoft architecture specifics */
#else
/* Non-Windows */
#if __CILKRTS_ABI_VERSION >= 1
#   define CILK_PIPE_FAKE_SAVE_FP(sf) do {                              \
        __asm__ ( "stmxcsr %0\n\t"                                      \
                  "fnstcw %1" : : "m" ((sf).mxcsr), "m" ((sf).fpcsr));  \
    } while (0)
#else
// ABI 0: no floating point sate to save.
#   define CILK_PIPE_FAKE_SAVE_FP(sf) ((void) sf)
#endif
#endif


/* Initialize frame. To be called when worker is known */
__CILKRTS_INLINE void __cilk_pipe_fake_enter_frame_fast(__cilkrts_stack_frame *sf,
                                                        __cilkrts_worker      *w)
{
    sf->call_parent = w->current_stack_frame;
    sf->worker      = w;
    sf->flags       = CILK_PIPE_FAKE_VERSION_FLAG;
    w->current_stack_frame = sf;
}

/* Initialize frame. To be called when worker is not known */
__CILKRTS_INLINE void __cilk_pipe_fake_enter_frame(__cilkrts_stack_frame *sf)
{
    __cilkrts_worker* w = __cilkrts_get_tls_worker();
    uint32_t          last_flag = 0;
    if (! w) {
        w = __cilkrts_bind_thread_1();
        last_flag = CILK_FRAME_LAST;
    }
    __cilk_pipe_fake_enter_frame_fast(sf, w);
    sf->flags |= last_flag;
}

/* Initialize frame. To be called within the spawn helper */
__CILKRTS_INLINE void __cilk_pipe_fake_helper_enter_frame(
    __cilkrts_stack_frame *sf,
    __cilkrts_stack_frame *parent_sf)
{
    sf->worker      = 0;
    sf->call_parent = parent_sf;
}


/* This variable is used in CILK_FAKE_FORCE_FRAME_PTR(), below */
static int __cilk_pipe_fake_dummy = 8;

/* The following macro is used to force the compiler into generating a frame
 * pointer.  We never change the value of __cilk_pipe_fake_dummy, so the alloca()
 * is never called, but we need the 'if' statement and the __cilk_pipe_fake_dummy
 * variable so that the compiler does not attempt to optimize it away.
 */
#define CILK_PIPE_FAKE_FORCE_FRAME_PTR(sf) do {                            \
    if (__builtin_expect(1 & __cilk_pipe_fake_dummy, 0))                   \
        (sf).worker = (__cilkrts_worker*) alloca(__cilk_pipe_fake_dummy);  \
} while (0)

/* Prologue of a spawning function, that uses an extended frame type.
 * The extended frame type should inherit from __cilkrts_stack_frame.
 * We are not doing shrink-wrapping for the pipe_while loop.
 */
#define CILK_PIPE_FAKE_FRAME_PROLOG(FrameType, fname)        \
    FrameType fname;                                         \
    CILK_PIPE_FAKE_FORCE_FRAME_PTR(fname);                   \
    __cilk_pipe_fake_enter_frame(&(fname)) 

// Equivalent of CILK_FAKE_DEFERRED_ENTER_FRAME
#   define CILK_PIPE_FAKE_DEFERRED_ENTER_FRAME(sf) do {       \
        if (! (sf).worker) __cilk_pipe_fake_enter_frame(&(sf));    \
    } while (0)


#if __CILKRTS_ABI_VERSION >= 1
/* Equivalent to CILK_FAKE_SYNC, but with an sf argument */
#   define CILK_PIPE_FAKE_SYNC_IMP(sf) do {                                   \
        if (__builtin_expect((sf).flags & CILK_FRAME_UNSYNCHED, 0))      {    \
            (sf).parent_pedigree = (sf).worker->pedigree;                     \
            CILK_PIPE_FAKE_SAVE_FP(sf);                                       \
            if (! CILK_SETJMP((sf).ctx))                                      \
                 __cilkrts_sync(&(sf));                                       \
        }                                                                     \
        ++(sf).worker->pedigree.rank;                                         \
    } while (0)
#else
#   define CILK_PIPE_FAKE_SYNC_IMP(sf) do {                                   \
        if (__builtin_expect((sf).flags & CILK_FRAME_UNSYNCHED, 0))      {    \
            CILK_PIPE_FAKE_SAVE_FP(sf);                                       \
            if (! CILK_SETJMP((sf).ctx))                                      \
                __cilkrts_sync(&(sf));                                        \
        }                                                                     \
    } while (0)
#endif

/* Pop the current frame off of the call chain */
#define CILK_PIPE_FAKE_POP_FRAME(sf) do {                  \
    (sf).worker->current_stack_frame = (sf).call_parent;   \
    (sf).call_parent = 0;                                  \
} while (0)

/* Implementation of spawning function epilog.  See CILK_FAKE_EPILOG macro and
 * __cilk_pipe_fake_stack_frame destructor body.
 */
#define CILK_PIPE_FAKE_CLEANUP_FRAME(sf) do {                \
    if (! (sf).worker) break;                                \
    CILK_PIPE_FAKE_SYNC_IMP(sf);                             \
    CILK_PIPE_FAKE_POP_FRAME(sf);                            \
    if ((sf).flags != CILK_PIPE_FAKE_VERSION_FLAG)           \
        __cilkrts_leave_frame(&(sf));                        \
} while (0)


// End of macros copied from cilk_fake.h
/**************************************************************************************/




#endif // INCLUDED_PIPER_FAKE_MACROS_H_
