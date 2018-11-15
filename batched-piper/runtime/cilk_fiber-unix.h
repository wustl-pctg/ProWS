/* cilk_fiber-unix.h                  -*-C++-*-
 *
 *************************************************************************
 *
 *  @copyright
 *  Copyright (C) 2012
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

#ifndef INCLUDED_CILK_FIBER_UNIX_DOT_H
#define INCLUDED_CILK_FIBER_UNIX_DOT_H

#ifndef __cplusplus
#   error cilk_fiber-unix.h is a C++-only header
#endif

#include "cilk_fiber.h"
#include "jmpbuf.h"

/**
 * @file cilk_fiber-unix.h
 *
 * @brief Unix-specific implementation for cilk_fiber.
 */

/**
 * @brief Unix-specific fiber class derived from portable fiber class
 */
struct cilk_fiber_sysdep : public cilk_fiber
{
  public:

#if SUPPORT_GET_CURRENT_FIBER
    /**
     * @brief Gets the current fiber from TLS.
     */
    static cilk_fiber_sysdep* get_current_fiber_sysdep();
#endif

    /**
     * @brief Construct the system-dependent portion of a fiber.
     *
     * @param stack_size  The size of the stack for this fiber.
     */ 
    cilk_fiber_sysdep(std::size_t stack_size);

    /**
     * @brief Construct the system-dependent of a fiber created from a
     * thread.
     */ 
    cilk_fiber_sysdep(from_thread_t);

    /**
     * @brief Destructor
     */ 
    ~cilk_fiber_sysdep();

    /**
     * @brief OS-specific calls to convert this fiber back to thread.
     *
     * Nothing to do for Linux.
     */
    void convert_fiber_back_to_thread();

    /**
     * @brief System-dependent function to suspend self and resume execution of "other".
     *
     * This fiber is suspended.
     *          
     * @pre @c is_resumable() should be true. 
     *
     * @param other              Fiber to resume.
     */
    void suspend_self_and_resume_other_sysdep(cilk_fiber_sysdep* other);

    /**
     * @brief System-dependent function called to jump to @p other
     * fiber.
     *
     * @pre @c is_resumable() should be false.
     *
     * @param other  Fiber to resume.
     */
    NORETURN jump_to_resume_other_sysdep(cilk_fiber_sysdep* other);
    
    /**
     * @brief Runs the start_proc.
     * @pre is_resumable() should be false.
     * @pre is_allocated_from_thread() should be false.
     * @pre m_start_proc must be valid.
     */
    NORETURN run();

    /**
     * @brief Returns the base of this fiber's stack.
     */
    inline char* get_stack_base_sysdep() { return m_stack_base; }

  private:
    char*                       m_stack_base;     ///< The base of this fiber's stack.
    char*                       m_stack;          // Stack memory (low address)
    __CILK_JUMP_BUFFER          m_resume_jmpbuf;  // Place to resume fiber
    unsigned                    m_magic;          // Magic number for checking

    static int                  s_page_size;      // Page size for
                                                  // stacks.

    // Allocate memory for a stack.  This method
    // initializes m_stack and m_stack_base.
    void make_stack(size_t stack_size);

    // Deallocates memory for the stack.
    void free_stack();

    // Common helper method for implementation of resume_other_sysdep
    // variants.
    inline void resume_other_sysdep(cilk_fiber_sysdep* other);
};

#endif // ! defined(INCLUDED_CILK_FIBER_UNIX_DOT_H)
