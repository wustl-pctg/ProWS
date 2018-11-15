/* cilk-abi-vla-internal.c        -*-C++-*-
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

/*
 * These functions are provided in their own compilation unit so I can debug
 * them.  cilk-abi-vla.c must always be compiled with optimization on so that
 * inlining occurs.
 */

#include "internal/abi.h"
#include "cilk-abi-vla-internal.h"
#include "bug.h"
#include "full_frame.h"
#include "local_state.h"

#include <stdlib.h>
#include <stdint.h>

#include "bug.h"

void *vla_internal_heap_alloc(__cilkrts_stack_frame *sf,
                              size_t full_size,
                              uint32_t align)
{
    return malloc(full_size);
}

void vla_internal_heap_free(void *t, size_t size)
{
    free(t);
}

void vla_free_from_original_stack(__cilkrts_stack_frame *sf,
                                  size_t full_size)
{
    // The __cilkrts_stack_frame must be initialized
    CILK_ASSERT(sf->worker);

#if 1
    // Add full_size to ff->sync_sp so that when we return, the VLA will no
    // longer be allocated on the stack
    __cilkrts_adjust_stack(*sf->worker->l->frame_ff, full_size);
#else
    // Inline __cilkrts_adjust_stack for Kevin
    full_frame *ff = *sf->worker->l->frame_ff;
    ff->sync_sp = ff->sync_sp + full_size;
#endif
}
