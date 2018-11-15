/* pinning.h                 -*-C++-*-
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
 * @file pinning.h
 *
 * @brief Functions for implementing pinning of worker threads to
 *        hardware threads.
 */
#ifndef INCLUDED_PINNING_DOT_H
#define INCLUDED_PINNING_DOT_H

#include <stdlib.h>
#include "cilk/common.h"

// The structs for pinning are going to be in C, because they get
// called from scheduler.c and other C code.

__CILKRTS_BEGIN_EXTERN_C

/**
 * @brief Ways of pinning worker threads in the runtime.
 *
 * PIN_SCATTER and PIN_COMPACT are analogous to OpenMP's notion of
 * "scatter" or "compact" granularity. 
 */
typedef enum {
    PIN_NONE,      //< No pinning of workers.  Default
    PIN_SCATTER,   //< Pin by spread workers out across the machine.
    PIN_COMPACT,   //< Pin by filling up cores first.
    PIN_MAX_TYPE   //< Max type for pinning.  This value should always be last.
} pin_policy_t;

/**
 * @brief Struct describing how the runtime might pin threads.
 */
typedef struct {
    pin_policy_t policy;    ///< Policy for pinning threads.
    int verbose;            ///< True if we should output pinning messages.

    const char* sysinfo;    ///< File describing the system.  (/proc/cpuinfo file on Linux)
    int expected_num_procs; ///< Expected number of processors in the file.
} pin_options_t;

/**
 * @brief Maximum id for a processor.
 */
#define CILK_MAX_PROC_ID 65535

/**
 * @brief Enum storing the index into the id array for a particular
 * level.
 */
typedef enum {
    HW_THREAD  = 0,          //< Hardware thread level
    CORE       = 1,          //< Core level
    PACKAGE    = 2,          //< Package (socket) level
    PROC_ID_NUM_LEVELS = 3   //< Number of levels in the hierarchy.
                             // Must be last.
} proc_id_levels_t;

/**
 * @brief Information identifying a single processor in the system.
 *
 * This information typically comes from parsing /proc/cpuinfo on
 * Linux, (or an equivalent file on other systems).
 *
 * The id array stores an id for each level in the hierarchy (i.e.,
 * each of the PROC_ID_NUM_LEVELS).
 *
 * For example, id[HW_THREAD] = 0, id[CORE] = 2, id[PACKAGE] = 1 means
 * thread 0 of core 2 on package (socket) 1.
 */
typedef struct proc_id_t {
    /// Processor number (The number used by the OS for the processor).
    int os_id;   

    /// The id of a processor, stored as an id for each level.
    int id[PROC_ID_NUM_LEVELS]; 
} proc_id_t;



/**
 * @brief Stores an ordered set of @c proc_id_t objects.
 *
 * @c worker_to_proc is either NULL if no pinning scheme is in place,
 * or it is an array of @c hardware_thread_count structs, each of type
 * @c proc_id_t.
 *
 * When @c worker_to_proc is not NULL, @c worker_to_proc[i] stores the
 * proc_id_t object associated with os thread @c i in the current
 * pinning scheme.
 *
 * The runtime uses the @c pinning_map_worker_id_to_os_processor
 * method to translate from a worker id to os thread id.
 */
typedef struct system_cpu_map {
    /**
     * @brief Maps a worker id to a cpu (that can be used for pinning).
     *
     * If this array is not NULL, it has @c hardware_thread_count elements.
     */
    proc_id_t* worker_to_proc; 

    /**
     * @brief Counts number of hardware threads.
     *
     *  May be more than core count, when we have hyperthreading.
     */
    int hardware_thread_count; 

    int core_count;       ///< Counts the number of cores we have.
    proc_id_t min_cpu;    ///< Stores the minimum values of each of the ids.
    proc_id_t max_cpu;    ///< Stores the maximum values of each of the ids.

    /**
     * @brief Worker id 0 should map to this hardware thread id.
     */
    int wkr0_offset;           
} system_cpu_map;


/**
 * @brief Parse the CILK_PINNING environment variable for pinning
 * options.
 *
 * TBD: Eventually, we may want a way to specify a custom system info
 * file instead of just trying to find the /proc/cpuinfo file
 * automatically.
 *
 * @param pin_options Pointer to struct to save options into.
 */
void pinning_parse_options(pin_options_t* pin_options);

/**
 * @brief Build a system map for this machine.
 *
 * More specifically, this method constructs an object @c sysmap,
 * which maps a worker id to a @c proc_id_t struct describing each
 * processor.
 *
 * @param pin_options Describe the pinning options
 * @param verbosity   Controls how much print output we want for debugging.
 *
 * @return The system map for this machine, or NULL if we failed to build one.
 */
system_cpu_map* pinning_create_system_map(pin_options_t* pin_options,
                                          int verbosity);

/**
 * @brief Destroy a system cpu map.
 */
void pinning_destroy_system_map(system_cpu_map* sysmap);


/**
 * @brief Debugging method.  Print out the current system map.
 */
void pinning_print_system_map(system_cpu_map* sysmap);


/**
 * @brief Maps a worker number to an OS processor id, for the purposes
 * of pinning threads to processors.
 *
 * Worker ids are mapped to a hardware thread id, which falls into the
 * range [0, g->sysdep->hardware_thread_count).
 *
 * @param  sysmap          System cpu map
 * @param  worker_self_id  w->self for a worker.
 * @param  P               the expected maximum number of processors.
 * 
 * @return  -1 if pinning is not enabled
 * @return  processor id to pin the worker to.
 */
__CILKRTS_INLINE
int pinning_map_worker_id_to_os_processor(system_cpu_map *sysmap,
                                          int32_t worker_self_id,
                                          int P)
{
    if (NULL == sysmap)
        return -1;

    // Add 1 to worker_self_id, so that the user thread maps to an
    // modified worker id of 0.  Modified worker ids should be between
    // 0 and P-1.
    int32_t modified_wkr_id = (worker_self_id + 1) % P;
    
    // Correct in case (worker_self_id + 1 ) % P was somehow negative.
    // This should never happen for reasonable values of
    // worker_self_id...
    if (modified_wkr_id < 0) {
        modified_wkr_id += P;
    }

    // Add the offset the processor with modified worker id of 0 to
    // the hardware thread at @c sysmap->offset.
    int idx = (modified_wkr_id + sysmap->wkr0_offset) % sysmap->hardware_thread_count;
    return sysmap->worker_to_proc[idx].os_id;
}

/**
 * @brief Print out message about where we pin a thread.
 */
void pinning_report_thread_pin(const char *desc,
                               int32_t wkr_id,
                               int os_id);

__CILKRTS_END_EXTERN_C


#endif // ! defined(INCLUDED_PINNING_DOT_H)
