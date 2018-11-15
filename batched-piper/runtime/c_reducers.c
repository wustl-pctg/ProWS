/* c_reducers.c                  -*-C-*-
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

/* Implementation of C reducers */

// Disable warning about integer conversions losing significant bits.
// The code is correct as is.
#ifdef __INTEL_COMPILER
#pragma warning(disable:2259)
#endif

#define CILK_C_DEFINE_REDUCERS

#include <cilk/reducer_opadd.h>
#include <cilk/reducer_opand.h>
#include <cilk/reducer_opmul.h>
#include <cilk/reducer_opor.h>
#include <cilk/reducer_opxor.h>
#include <cilk/reducer_min_max.h>

/* End reducer_opadd.c */
