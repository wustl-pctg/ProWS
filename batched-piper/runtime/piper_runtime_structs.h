/* piper_runtime_structs.h            -*-C++-*-
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
 * @file piper_runtime_structs.h
 *
 * @brief  Definition of runtime structures of pipeline functions.
 *
 * This file defines C++ objects.
 */

#ifndef INCLUDED_PIPER_RUNTIME_STRUCTS_DOT_H
#define INCLUDED_PIPER_RUNTIME_STRUCTS_DOT_H


#include "rts-common.h"
#include "worker_mutex.h"

#include <cilk/common.h>
#include "full_frame.h"


/// Possible status values for an iteration during its execution.
typedef enum pipe_iter_status_t {
    /// Starting status of an iteration.
    ITER_STANDARD_EXEC = 0,
    
    /// The continuation of the spawn of this iteration has been stolen,
    /// and the iteration is not suspended.
    ITER_PROMOTED_ACTIVE = 1,

    /// The continuation of the spawn of this iteration has been stolen, 
    /// and the iteration has been suspended (because of a stage wait).
    ITER_PROMOTED_SUSPENDED = 2,

    /// The iteration for this block is finished executing.
    ITER_FINISHED = 3
} pipe_iter_status_t;

/// Indicates whether a block is available or in use.
typedef enum pipe_data_block_status_t {
    /// This block is available to be reused.  This status means that
    /// the iteration after this one has finished.
    BLOCK_AVAILABLE = 0,

    /// Block is being used for executing an iteration.
    BLOCK_IN_USE = 2
} pipe_data_block_status_t;

/**
 *@brief A pipe_iter_full_frame is a special kind of full frame for
 * pipe_while iterations.
 */
struct pipe_iter_full_frame : full_frame {

    /// The data block for this full frame.
    __cilkrts_pipe_iter_data* data;

    /// Slot to save reducer map when we suspend frame.
    struct cilkred_map *saved_reducer_map;

    /**
     * @brief The actual full frame that is being suspended.
     *
     * This frame is either this object, or a full frame that is a
     * direct call descendant of this pipeline iteration that is being
     * suspended.
     */
    full_frame *suspended_ff;

    /// Field which is 1 if this full frame is in use, 0 otherwise.
    bool in_use;

    /// Padding between local fields and shared synchronization fields
    /// for the data block.
    char data_block_padding[64];

    /// Mutex protecting this data block.
    spin_mutex pipe_data_lock;

    /// Counter to track when we can reuse this data block.
    int cleanup_counter;

    /// Status field for the iteration, tracking whether it has been
    /// stolen from / promoted or not.
    pipe_iter_status_t iter_status;

    /// Status field for the data block itself, tracking whether it is
    /// in use or free to be reused.
    pipe_data_block_status_t block_status;
};

/**
 * @brief Declaration of struct defining runtime data stored in a
 *  control frame.
 */
struct pipe_control_data_t {
    size_t control_data_size;            ///< Size of control data
                                         ///(including iteration
                                         ///frames).
    /// The pointer to the buffer of iteration data blocks.
    __cilkrts_pipe_iter_data* iter_buffer;

    /// Buffer of full frames, for use in pipeline iterations.
    pipe_iter_full_frame* ff_buffer;

    /// Slot to save reducer map when we suspend the control frame.
    struct cilkred_map *saved_reducer_map;
};


#endif // ! defined(INCLUDED_PIPER_RUNTIME_STRUCTS_DOT_H)
