/*  cilk.h                  -*-C++-*-
 *
 *  @copyright
 *  Copyright (C) 2010-2013
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
 */
 
/** @file cilk.h
 *
 *  @brief Provides convenient aliases for the Cilk language keywords.
 *
 *  @details
 *  Since Cilk is a nonstandard extension to both C and C++, the Cilk
 *  language keywords all begin with “`_Cilk_`”, which guarantees that they
 *  will not conflict with user-defined identifiers in properly written 
 *  programs, so that “standard” C and C++ programs can safely be
 *  compiled a Cilk-enabled C or C++ compiler.
 *
 *  However, this means that the keywords _look_ like something grafted on to
 *  the base language. Therefore, you can include this header:
 *
 *      #include "cilk/cilk.h"
 *
 *  and then write the Cilk keywords with a “`cilk_`” prefix instead of
 *  “`_Cilk_`”.
 *
 *  @ingroup language
 */
 
 
/** @defgroup language Language Keywords
 *  Definitions having to do with the Cilk language.
 *  @{
 */
 
#ifndef cilk_spawn
# define cilk_spawn _Cilk_spawn ///< Spawn a task that can execute in parallel.
# define cilk_sync  _Cilk_sync  ///< Wait for spawned tasks to complete.
# define cilk_for   _Cilk_for   ///< Execute iterations of a for loop in parallel.
#endif

/// @}
