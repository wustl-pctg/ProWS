/* local_state.c                  -*-C++-*-
 *
 *************************************************************************
 *
 *  @copyright
 *  Copyright (C) 2010-2011
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
 *
 **************************************************************************/

#include "local_state.h"
#include "bug.h"
#include "full_frame.h"

void run_scheduling_stack_fcn(__cilkrts_worker *w)
{
    scheduling_stack_fcn_t fcn = w->l->post_suspend;
    full_frame *ff2 = *w->l->frame_ff;
    __cilkrts_stack_frame *sf2 = w->l->suspended_stack;

    w->l->post_suspend = 0;
    w->l->suspended_stack = 0;

    // Conceptually, after clearing w->l->frame_ff,
    // w no longer owns the full frame ff.
    // The next time another (possibly different) worker takes
    // ownership of ff will be at a provably_good_steal on ff. 
    *w->l->frame_ff = NULL;

    CILK_ASSERT(fcn);
    CILK_ASSERT(ff2);
    fcn(w, ff2, sf2);

    // After we run the scheduling stack function, we shouldn't
    // (still) not have a full frame.
    CILK_ASSERT(NULL == *w->l->frame_ff);
}

/* End local_state.c */
