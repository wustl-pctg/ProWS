/* cilk-abi-vla-internal.h        -*-C++-*-
 *
 *************************************************************************
 *
 *  @copyright
 *  Copyright (C) 2013
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
 * @file cilk-abi-vla-internal.h
 *
 * @brief Allocation/deallocation function for use with Variable Length
 * Arrays in spawning functions.
 *
 * These should be the only functions in the Cilk runtime allocating memory
 * from the standard C runtime heap.  This memory will be provided to user
 * code for use in VLAs, when the memory cannot be allocated from the stack.
 *
 * While these functions are simply passthroughs to malloc and free at the
 * moment, once we've got the basics of VLA allocations working we'll make
 * them do fancier tricks.
 */

/**
 * @brief Allocate memory from the heap for use by a Variable Length Array in
 * a spawning function.
 *
 * @param sf The __cilkrts_stack_frame for the spawning function containing
 * the VLA.
 * @param full_size The number of bytes to be allocated, including any tags
 * needed to identify this as allocated from the heap.
 * @param align Any alignment necessary for the allocation.
 */

void *vla_internal_heap_alloc(__cilkrts_stack_frame *sf,
                              size_t full_size,
                              uint32_t align);

/**
 * @brief Deallocate memory from the heap used by a Variable Length Array in
 * a spawning function.
 *
 * @param t The address of the memory block to be freed.
 * @param size The size of the memory block to be freed.
 */

void vla_internal_heap_free(void *t,
                            size_t size);

/**
 * @brief Deallocate memory from the original stack.  We'll do this by adding
 * full_size to ff->sync_sp.  So after the sync, the Variable Length Array
 * will no longer be allocated on the stack.
 *
 * @param sf The __cilkrts_stack_frame for the spawning function that is
 * deallocating a VLA.
 * @param full_size The size of the VLA, including any alignment and tags.
 */
void vla_free_from_original_stack(__cilkrts_stack_frame *sf,
                                  size_t full_size);
