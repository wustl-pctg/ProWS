/* cilk_fiber-unix.cpp                  -*-C++-*-
 *
 *************************************************************************
 *
 *  Copyright (C) 2012-2015, Intel Corporation
 *  All rights reserved.
 *  
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
 *  *********************************************************************
 *  
 *  PLEASE NOTE: This file is a downstream copy of a file mainitained in
 *  a repository at cilkplus.org. Changes made to this file that are not
 *  submitted through the contribution process detailed at
 *  http://www.cilkplus.org/submit-cilk-contribution will be lost the next
 *  time that a new version is released. Changes only submitted to the
 *  GNU compiler collection or posted to the git repository at
 *  https://bitbucket.org/intelcilkplusruntime/itnel-cilk-runtime.git are
 *  not tracked.
 *  
 *  We welcome your contributions to this open source project. Thank you
 *  for your assistance in helping us improve Cilk Plus.
 **************************************************************************/

#include "cilk_fiber-unix.h"
#include "cilk_malloc.h"
#include "bug.h"
#include "os.h"

/// @todo{Remove this include; only for debugging suspended fibers}
#include "global_state.h" 
extern global_state_t *__cilkrts_global_state;

#include <cstdio>
#include <cstdlib>

#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h> // strerror
#include "declare-alloca.h"

// MAP_ANON is deprecated on Linux, but seems to be required on Mac...
#ifndef MAP_ANONYMOUS
#   define MAP_ANONYMOUS MAP_ANON
#endif

// MAP_STACK and MAP_GROWSDOWN have no affect in Linux as of 2014-04-04, but
// could be very useful in future versions.  If they are not defined, then set
// them to zero (no bits set).
#ifndef MAP_STACK
#   define MAP_STACK 0
#endif
#ifndef MAP_GROWSDOWN
#   define MAP_GROWSDOWN 0
#endif

// Magic number for sanity checking fiber structure
const unsigned magic_number = 0x5afef00d;

// Page size for stacks
#ifdef _WRS_KERNEL
long cilk_fiber_sysdep::s_page_size = 4096;
#else
long cilk_fiber_sysdep::s_page_size = sysconf(_SC_PAGESIZE);
#endif

cilk_fiber_sysdep::cilk_fiber_sysdep(std::size_t stack_size)
	: cilk_fiber(stack_size)
	, m_magic(magic_number)
{
	// Set m_stack and m_stack_base.
	make_stack(stack_size);

	// Get high-address of stack, with 32-bytes of spare space, and rounded
	// down to the nearest 32-byte boundary.
	const uintptr_t align_mask = 32 - 1;
	m_stack_base -= ((std::size_t) m_stack_base) & align_mask;
}

cilk_fiber_sysdep::cilk_fiber_sysdep(from_thread_t)
	: cilk_fiber()
	, m_magic(magic_number)
{
	this->set_allocated_from_thread(true);

	// Dummy stack data for thread-main fiber
	m_stack      = NULL;
	m_stack_base = NULL;
}

void cilk_fiber_sysdep::convert_fiber_back_to_thread()
{
	// Does nothing on Linux.
}

cilk_fiber_sysdep::~cilk_fiber_sysdep()
{
	CILK_ASSERT(magic_number == m_magic);
	if (!this->is_allocated_from_thread())
		free_stack();
}

#if SUPPORT_GET_CURRENT_FIBER
cilk_fiber_sysdep* cilk_fiber_sysdep::get_current_fiber_sysdep()
{
	return cilkos_get_tls_cilk_fiber();
}
#endif

// Jump to resume other fiber.  We may or may not come back.
inline void cilk_fiber_sysdep::resume_other_sysdep(cilk_fiber_sysdep* other)
{
	if (other->is_resumable()) {
		other->set_not_resumable();
		// Resume by longjmp'ing to the place where we suspended.
		CILK_LONGJMP(other->m_resume_jmpbuf);
	}
	else {
		// Otherwise, we've never run this fiber before.  Start the
		// proc method.
		other->run();
	}
}

// GCC doesn't allow us to call __builtin_longjmp in the same function that
// calls __builtin_setjmp, so create a new function to house the call to
// __builtin_longjmp
static void __attribute__((noinline))
do_cilk_longjmp(__CILK_JUMP_BUFFER jmpbuf)
{
	CILK_LONGJMP(jmpbuf);
    __builtin_unreachable();
}

#define ALIGN_MASK  (~((uintptr_t)0xFF))
static char* __attribute__((always_inline)) get_sp_for_executing_sf(char* stack_base) {
    // Make the stack pointer 256-byte aligned
    char* new_stack_base = stack_base - 256;
    new_stack_base = (char*)((size_t)new_stack_base & ALIGN_MASK);
    return new_stack_base;
}

void cilk_fiber_sysdep::suspend_self_and_resume_other_sysdep(cilk_fiber_sysdep* other)
{
#if SUPPORT_GET_CURRENT_FIBER
	cilkos_set_tls_cilk_fiber(other);
#endif

	// This is now set in the other fiber
	//    CILK_ASSERT(this->is_resumable());
	CILK_ASSERT(!this->is_resumable());
	// Should assert that m_from_fiber of other is this...

	// Jump to the other fiber.  We expect to come back.
	if (! CILK_SETJMP(m_resume_jmpbuf)) {
        // This unfortunate code duplication saves
        // a fair amount of time with futures.
	    if (other->is_resumable()) {
	    	other->set_not_resumable();
	    	// Resume by longjmp'ing to the place where we suspended.
	    	do_cilk_longjmp(other->m_resume_jmpbuf);
	    }
	    else {
	    	// Otherwise, we've never run this fiber before.  Start the
	    	// proc method.
	    	other->run();
	    }
	}

	// Return here when another fiber resumes me.
	// If the fiber that switched to me wants to be deallocated, do it now.
	do_post_switch_actions();
}

NORETURN cilk_fiber_sysdep::jump_to_resume_other_sysdep(cilk_fiber_sysdep* other)
{
#if SUPPORT_GET_CURRENT_FIBER
	cilkos_set_tls_cilk_fiber(other);
#endif
	CILK_ASSERT(!this->is_resumable());

	// Jump to the other fiber.  But we are never coming back because
	// this fiber is being reset.
	resume_other_sysdep(other);

	// We should never come back here...
	__cilkrts_bug("Should not get here");
}

#ifdef TRACK_FIBER_COUNT
extern "C" {
void increment_fiber_count(global_state_t* g);
}
#endif
NORETURN __attribute__((noinline)) cilk_fiber_sysdep::run()
{
	// Only fibers created from a pool have a proc method to run and execute. 
	CILK_ASSERT(m_start_proc);
	CILK_ASSERT(!this->is_allocated_from_thread());
	CILK_ASSERT(!this->is_resumable());

#ifdef TRACK_FIBER_COUNT
increment_fiber_count(__cilkrts_get_tls_worker()->g);
#endif

    uintptr_t frame_size = NULL;
    char *stack_pointer = NULL;
    __asm__ volatile ("movq %%rsp, %0" : "=r" (stack_pointer));

    __asm__ volatile ("movq %%rbp,%0" : "=r" (frame_size));
    // equivalent to: frame_size = rbp - rsp;
    frame_size = frame_size - (uintptr_t)stack_pointer;

    // Make sure the frame size 16-byte aligned
    frame_size += ((16 - (frame_size & (0xF))) & 0xF);

    CILK_ASSERT(frame_size < 4096);

    char *new_sp = m_stack_base - frame_size;

    // equivalent to: rsp = new_sp;
    __asm__ volatile ("movq %0,%%rsp" :: "r" (new_sp));

	// Note: our resetting of the stack pointer is valid only if the
	// compiler has not saved any temporaries onto the stack for this
	// function before the longjmp that we still care about at this
	// point.
    
	// Verify that 1) 'this' is still valid and 2) '*this' has not been
	// corrupted.
	CILK_ASSERT(magic_number == m_magic);

	// If the fiber that switched to me wants to be deallocated, do it now.
	do_post_switch_actions();

	// Now call the user proc on the new stack
	m_start_proc(this);

	// alloca() to force generation of frame pointer.  The argument to alloca
	// is contrived to prevent the compiler from optimizing it away.  This
	// code should never actually be executed.
	int* dummy = (int*) alloca((sizeof(int) + (std::size_t) m_start_proc) & 0x1);
	*dummy = 0xface;

    // Evidence points toward the compiler optimizing the frame pointer away
    // if there isn't a setjmp on m_resume_jmpbuf as well.
    if (!CILK_SETJMP(m_resume_jmpbuf)) {
        __cilkrts_bug("Should not exec this setjmp");
    }

	// User proc should never return.
	__cilkrts_bug("Should not get here");
}

void cilk_fiber_sysdep::make_stack(size_t stack_size)
{
	char* p;

    // We've already validated that the stack size is page-aligned and
    // is a reasonable value.  No need to do any extra rounding here.
    size_t rounded_stack_size = stack_size;

    // Normally, we have already validated that the stack size is
    // aligned to 4K.  In the rare case that pages are huge though, we
    // need to do some extra checks.
    if (rounded_stack_size < 3 * (size_t)s_page_size) {
        // If the specified stack size is too small, round up to 3
        // pages.  We need at least 2 extra for the guard pages.
        rounded_stack_size = 3 * (size_t)s_page_size;
    }   
    else {
        // Otherwise, the stack size is large enough, but might not be
        // a multiple of page size.  Round up to nearest multiple of
        // s_page_size, just to be safe.
        size_t remainder = rounded_stack_size % s_page_size;
        if (remainder) {
            rounded_stack_size += s_page_size - remainder;
        }
    } 

	p = (char*)mmap(0, rounded_stack_size,
									PROT_READ|PROT_WRITE,
									MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK|MAP_GROWSDOWN,
									-1, 0);
	if (__builtin_expect(MAP_FAILED == p, 0)) {
		// For whatever reason (probably ran out of memory), mmap() failed.
		// There is no stack to return, so the program loses parallelism.
		m_stack = NULL;
		m_stack_base = NULL;

		// They return here as if they later notice that m_stack is null,
		// but I don't see anywhere that they do, so the program segfaults
		// when jumping to this fiber...so I've added this call to
		// __cilkrts_bug.
		__cilkrts_bug("Cilk: out of memory for stacks\n");
		fprintf(stderr, "Cilk: out of memory for stacks!\n");
		raise(SIGSTOP);
		
		return;
	} 

    #if FIBER_DEBUG >= 1
	//fprintf(stderr, "Stack mmap: %p\n", p);
	size_t newval = __sync_fetch_and_add(&__cilkrts_global_state->active_stacks, 1) + 1;
	size_t oldval = __cilkrts_global_state->stacks_high_watermark;
	while (newval > oldval
				 &&  !__sync_bool_compare_and_swap(&__cilkrts_global_state->stacks_high_watermark, oldval, newval)) {
		oldval = __cilkrts_global_state->stacks_high_watermark;
	}
    #endif

	// mprotect guard pages.
	mprotect(p + rounded_stack_size - s_page_size, s_page_size, PROT_NONE);
	mprotect(p, s_page_size, PROT_NONE);

	m_stack = p;
	m_stack_base = p + rounded_stack_size - s_page_size;
}


void cilk_fiber_sysdep::free_stack()
{
	if (m_stack) {
		size_t rounded_stack_size = m_stack_base - m_stack + s_page_size;
		if (__builtin_expect(munmap(m_stack, rounded_stack_size) < 0, 0)) {
			__cilkrts_bug("Cilk: stack munmap failed error %s\n", strerror(errno));
			fprintf(stderr, "Cilk: stack munmap failed error %s\n", strerror(errno));
			raise(SIGSTOP);
		}
        #if FIBER_DEBUG >= 1
		__sync_fetch_and_sub(&__cilkrts_global_state->active_stacks, 1);
        #endif
		
		//fprintf(stderr, "Stack munmap: %p\n", m_stack);
	}
}

/* End cilk_fiber-unix.cpp */
