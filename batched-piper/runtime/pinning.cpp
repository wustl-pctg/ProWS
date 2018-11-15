/* pinning.c                  -*-C-*-
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
 *
 *  Patents Pending, Intel Corporation.
 **************************************************************************/

/**
 * Support for pinning of workers to threads.
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#    include <unistd.h>
#endif

#include "bug.h"
#include "cilk_malloc.h"
#include "os.h"
#include "pinning.h"

/// The string to prepend in front of all pinning-related messages.
static const char* CILK_PIN_MSG_STRING = "CILK_PINNING";

// Determines whether a < b according to a lexicographic order.
// a[0] is the most-significant term, a[3] is the least-significant.
inline bool compare_int4(int a[4], int b[4])
{
    return std::lexicographical_compare(a, a+4, b, b+4);
}


/**
 * Compare lexicographically, with ordering being:
 *    (id[PACKAGE], id[CORE], id[HW_THREAD])
 *
 * This ordering tries to put consecutive workers on consecutive
 * hardware threads.
 */ 
inline bool compare_proc_id_compact_hw_thread(const proc_id_t& left,
                                              const proc_id_t& right)
{
    int L[PROC_ID_NUM_LEVELS+1] = { left.id[PACKAGE],
                                    left.id[CORE],
                                    left.id[HW_THREAD],
                                    left.os_id };
    int R[PROC_ID_NUM_LEVELS+1] = { right.id[PACKAGE],
                                    right.id[CORE],
                                    right.id[HW_THREAD],
                                    right.os_id };
    return compare_int4(L, R);
}


/**
 * Compare lexicographically, with ordering being:
 *   (id[HW_THREAD], id[PACKAGE], id[CORE])
 *
 * This ordering tries to put consecutive workers onto adjacent cores,
 * but will try to spread hardware threads as far away from each other
 * as possible.
 */
inline bool compare_proc_id_compact(const proc_id_t& left,
                                    const proc_id_t& right)
{
    int L[PROC_ID_NUM_LEVELS+1] = { left.id[HW_THREAD],
                                    left.id[PACKAGE],
                                    left.id[CORE],
                                    left.os_id };
    int R[PROC_ID_NUM_LEVELS+1] = { right.id[HW_THREAD],
                                    right.id[PACKAGE],
                                    right.id[CORE],
                                    right.os_id };
    return compare_int4(L, R);
}


/**
 * Compare lexicographically, with ordering being:
 *   (id[HW_THREAD], id[CORE], id[PACKAGE])
 *
 * This ordering tries to put consecutive workers further away from
 * each other.
 */
inline bool compare_proc_id_scatter(const proc_id_t& left,
                                    const proc_id_t& right)
{
    int L[PROC_ID_NUM_LEVELS+1] = { left.id[HW_THREAD],
                                    left.id[CORE],
                                    left.id[PACKAGE],
                                    left.os_id };
    int R[PROC_ID_NUM_LEVELS+1] = { right.id[HW_THREAD],
                                    right.id[CORE],
                                    right.id[PACKAGE],
                                    right.os_id };
    return compare_int4(L, R);
}


/**
 *@brief Functor for keeping CPUs whose core_id is less than a
 *specified limit.
 */
class PinningCoresInLimitFunctor {
public:
    /// Constructor: saves the core limit.
    PinningCoresInLimitFunctor(int core_limit)
        : m_core_limit(core_limit)
    { }

    /// Unary operator returning true if the processor nubmer is less
    /// than the limit.
    bool operator()(const proc_id_t& proc) {
        return (proc.id[CORE] < m_core_limit);
    }
private:
    int m_core_limit;
};


/**
 * @brief Filters out CPUs from the array that have core id >= @c
 * core_limit.
 *
 * All processors x which are filtered out are moved to the end. 
 *
 * The elements we keep should remain in the same relative order.
 *
 * @param proc_array   The array of proc_id_t objects to filter.
 * @param length       Length of proc_array.
 * @param core_limit   Ignore all procs with id >= this value.
 *
 * @return Number of valid proc_id_t elements that remain.
 */
int pinning_filter_extra_cores(proc_id_t* proc_array,
                               int length,
                               int core_limit)
{
    // Partition the array, with the predicate being
    // all cores that have core id less than the specified limit.
    //
    // This call puts the cores we want to keep on the left,
    // and returns the pointer to the first core we want to get rid
    // of.
    //
    // The only downside to calling std::stable_partition instead of
    // writing our own loop is that we technically don't need
    // stability for the cores we are throwing away.  Thus, we could
    // really do this partition in place, since we only need stability
    // for the left half.  Oh well. :)

    proc_id_t* new_last =
        std::stable_partition(proc_array,
                              proc_array + length,
                              PinningCoresInLimitFunctor(core_limit));

    // Return the number of elements we keep.
    // Cast this value.  If the number of processors overflows an int,
    // we have problems...
    return (int)(new_last - proc_array);    
}


extern "C" {
    
// Returns true if 'a' and 'b' are equal null-terminated strings
inline int strmatch(const char* a, const char* b)
{
    return 0 == strcmp(a, b);
}

// Print the options for pinning to output.
static void pinning_print_options(pin_options_t* pin_options)
{
    const char* policy_string;
    cilkos_message(CILK_PIN_MSG_STRING,
                   "verbose = %d\n",
                   pin_options->verbose);
    switch (pin_options->policy) {
    case PIN_SCATTER:
        policy_string = "scatter";
        break;
    case PIN_COMPACT:
        policy_string = "compact";
        break;
    case PIN_NONE:
    default:
        policy_string = "none";
    }

    cilkos_message(CILK_PIN_MSG_STRING,
                   "policy = %s\n",
                   policy_string);
}

void pinning_parse_options(pin_options_t* pin_options)
{
    char envstr[48];
    // Check for undocumented environment variables for thread
    // pinning.
    size_t len = cilkos_getenv(envstr, sizeof(envstr), CILK_PIN_MSG_STRING);

    pin_options->policy = PIN_NONE;
    pin_options->verbose = 0;

    // TBD: Right now, we just read in a fixed file for Linux, 
    // and Windows does not do anything.
#ifdef _WIN32    
    pin_options->sysinfo = "";
#else
    pin_options->sysinfo = "/proc/cpuinfo";
#endif
    
    // Get the total number of CPUs on the system.
    pin_options->expected_num_procs = __cilkrts_hardware_cpu_count();
    
    if (len > 0) {
        // These are the strings we recognize.
        static const char* const s_compact = "compact";
        static const char* const s_none = "none";
        static const char* const s_scatter = "scatter";
        static const char* const s_verbose = "verbose";

        char* save_ptr;
        char* token;
        int num_tokens = 0;

        // Set a reasonable limit for the maxmimum number of tokens we accept.
        //
        // TBD: Currently, we don't read in environment
        // variables longer than 48 characters, so 10 tokens is
        // unlikely.  Also, at the moment, anything more
        // than two tokens is technically redundant, but why not?
        const int MAX_TOKENS = 10;

#ifdef _WIN32
        token = strtok_s(envstr, ",", &save_ptr);
#else
        token = strtok_r(envstr, ",", &save_ptr);
#endif
        while ((token != NULL) && (num_tokens < MAX_TOKENS))
        {
            num_tokens++;

            // Reads in tokens one at a time.
            // Technically, only the last one of "scatter",
            // "compact", and "none" is useful, since the
            // effect of earlier tokens are discarded.
            if (strmatch(token, s_scatter)) {
                pin_options->policy = PIN_SCATTER;
            } else if (strmatch(token, s_compact)) {
                pin_options->policy = PIN_COMPACT;
            } else if (strmatch(token, s_none)) {
                pin_options->policy = PIN_NONE;
            } else if (strmatch(token, s_verbose)) {
                pin_options->verbose = 1;
            }
#ifdef _WIN32
            token = strtok_s(NULL, ",", &save_ptr);
#else
            token = strtok_r(NULL, ",", &save_ptr);
#endif
        }

        if (pin_options->verbose) {
            pinning_print_options(pin_options);
        }
    }
}


/**
 * @brief Returns true if all the ids are in the interval [0, max_id)
 */
static int pinning_proc_id_in_range(proc_id_t* x,
                                    int max_id) 
{
    return ((x->id[PACKAGE] >= 0) &&
            (x->id[PACKAGE] < max_id) &&
            (x->id[CORE] >= 0) &&
            (x->id[CORE] < max_id) &&
            (x->id[HW_THREAD] >= 0) &&
            (x->id[HW_THREAD] < max_id));
}


/**
 * Compute minimum and maximum values of each field of elements in @c
 * cpu_array.
 *
 * This method fills in @c *min_cpu_id and @c *max_cpu_id as output.
 */
inline
static void compute_cpu_id_range(proc_id_t* cpu_array,
                                 int length,
                                 proc_id_t* min_cpu_id,
                                 proc_id_t* max_cpu_id)
{
    proc_id_t empty_id = {-1, {-1, -1, -1}};
    // Degenerate length case.
    if (length <= 0) {
        *max_cpu_id = empty_id;
        *min_cpu_id = empty_id;
        return;
    }
    

    // Start the min and max element as the first one.
    *max_cpu_id = cpu_array[0];
    *min_cpu_id = cpu_array[0];

    // Scan the remaining elements for minimum and maximum values for
    // each id.
    for (int i = 1; i < length; ++i) {
        (*max_cpu_id).os_id = std::max(cpu_array[i].os_id,
                                       (*max_cpu_id).os_id);
        (*min_cpu_id).os_id = std::min(cpu_array[i].os_id,
                                       (*min_cpu_id).os_id);
        
        for (int j = 0; j < PROC_ID_NUM_LEVELS; ++j) {
            (*max_cpu_id).id[j] = std::max(cpu_array[i].id[j],
                                           (*max_cpu_id).id[j]);
            (*min_cpu_id).id[j] = std::min(cpu_array[i].id[j],
                                           (*min_cpu_id).id[j]);
        }
    }
}

/**
 * Helper method for reading in a /proc/cpuinfo file.
 * Normally, this file does not assign hw_thread_ids relative to a
 * core.  Thus, id[HW_THREAD] is assumed to be a global id for all the
 * processors in sysmap.
 *
 * This method relabels all the hw_thread ids of processors, so that
 * they are relative to a given core, instead of being global ids.
 *
 * This method also sets sysmap->core_count to the number of cores in
 * the map.
 *
 * This function also sorts the processors in the sysmap array.
 */
static void pinning_relabel_hw_threads_and_count_cores(system_cpu_map *sysmap)
{

    // cilkos_message(CILK_PIN_MSG_STRING, "Initial system map: ");
    // pinning_print_system_map(sysmap);

    // First sort the workers, so that all hardware threads
    // corresponding to the same core and package are contiguous.
    std::sort(sysmap->worker_to_proc,
              sysmap->worker_to_proc + sysmap->hardware_thread_count,
              compare_proc_id_compact_hw_thread);

    // cilkos_message(CILK_PIN_MSG_STRING, "After sorting: ");
    // pinning_print_system_map(sysmap);

    sysmap->core_count = 0;
    int idx = 0;
    
    // Each iteration of this loop relabels the HW_THREAD ids of
    // all consecutive elements that have the same CORE and PACKAGE
    // id to be 0, 1, 2, ..., starting with the element at
    // sysmap->worker_to_proc[idx].
    while (idx < sysmap->hardware_thread_count) {
        // Look at the first element.
        int start_idx = idx;
        int start_core_id = sysmap->worker_to_proc[idx].id[CORE];
        int start_package_id = sysmap->worker_to_proc[idx].id[PACKAGE];

        // Set the first element's hw_thread_id to 0.
        sysmap->worker_to_proc[idx].id[HW_THREAD] = idx - start_idx;
        idx++;
        sysmap->core_count++;

        // Loop through any consecutive elements to my right
        // that have the same CORE and PACKAGE id.  Relabel these
        // thread ids in increasing order.
        while ((idx < sysmap->hardware_thread_count) &&
               (sysmap->worker_to_proc[idx].id[CORE] == start_core_id) &&
               (sysmap->worker_to_proc[idx].id[PACKAGE] == start_package_id)) {
            sysmap->worker_to_proc[idx].id[HW_THREAD] = idx - start_idx;
            idx++;
        }
    }

    // cilkos_message(CILK_PIN_MSG_STRING, "After relabeling: found %d cores ", sysmap->core_count);
    // pinning_print_system_map(sysmap);
}

/**
 * @brief Parses an input /proc/cpuinfo file, creating an array of
 * proc_id_t objects for each processor.
 *
 * This parsing method ignores any CPUs with core id >=
 * expected_num_procs
 *
 * @param file_path           Path to input file (which should be a /proc/cpuinfo)
 * @param expected_num_procs  The number of processors we expect to find.
 * @param verbosity           Level of output messages to generate.
 *
 * @return An array of proc_id_t objects, of length expected_num_pros,
 *         or NULL if there was any error in parsing the file.
 */
static proc_id_t* pinning_parse_proc_cpuinfo_file(const char* file_path,
                                                  int expected_num_procs,
                                                  int verbosity)
{
    FILE* f = fopen(file_path, "r");
    if (NULL != f)
    {
        char buf[1024];
        int procs_found = 0;
        proc_id_t* cpu_array =
            (proc_id_t*)__cilkrts_malloc(sizeof(proc_id_t) * expected_num_procs);
        
        // Keep parsing the file until we run out of entries.
        while (fgets(buf, sizeof(buf), f)) {

            proc_id_t current = {-1, {-1, -1, -1} };
            // First look for a processor string.
            do {
                int tmp;
                int items = sscanf(buf, "processor\t\t: %d\n", &tmp);
                if (items >= 1) {
                    current.os_id = tmp;
                }
            } while ((current.os_id < 0) && (fgets(buf, sizeof(buf), f)));
            
            // Once we find a processor, look for its subfields
            // next.
            int fields_found = 0;
            while ((fgets(buf, sizeof(buf), f)) && (fields_found < 3)) {
                int tmp;
                int items;
                // Look for physical id.
                items = sscanf(buf, "physical id\t: %d\n", &tmp);
                if (items >= 1) {
                    current.id[PACKAGE] = tmp;
                    fields_found++;
                    continue;
                }

                // Look for core id.
                items = sscanf(buf, "core id\t\t: %d\n", &tmp);
                if (items >= 1) {
                    current.id[CORE] = tmp;
                    fields_found++;
                    continue;
                }
                
                // Look for apicd id. 
                // Note that unlike the other two ids, we will have to rename
                // this field later, because apicid by default is a
                // global id, and we want to store an id relative to
                // the same package and core.
                items = sscanf(buf, "apicid\t\t: %d\n", &tmp);
                if (items >= 1) {
                    current.id[HW_THREAD] = tmp;
                    fields_found++;
                    continue;
                }
            }

            if (fields_found == 3) {
                if (pinning_proc_id_in_range(&current, CILK_MAX_PROC_ID)) {
                    if (verbosity >= 2) {
                        cilkos_message(CILK_PIN_MSG_STRING,
                                       "Found processor %d: os_id=%d, package=%d, core=%d, hw_thread=%d, fields_found=%d\n",
                                       procs_found,
                                       current.os_id,
                                       current.id[PACKAGE],
                                       current.id[CORE],
                                       current.id[HW_THREAD],
                                       fields_found);
                    }

                    if (procs_found < expected_num_procs) {
                        // We found a processor.  Save it away.
                        cpu_array[procs_found] = current;
                        procs_found++;
                    }
                    else {
                        cilkos_message(CILK_PIN_MSG_STRING,
                                       "WARNING: finding an extra processor with id %d. ignoring...\n",
                                       current.os_id);
                    }
                }
            }
        }
        fclose(f);

        if (verbosity >= 2) {
            cilkos_message(CILK_PIN_MSG_STRING,
                           "Found %d total processors...\n", procs_found);
        }

        if (procs_found != expected_num_procs) {
            cilkos_message(CILK_PIN_MSG_STRING,
                           "WARNING: could not parse /proc/cpuinfo file.. found %d processors, expected total of %d\n",
                           procs_found, expected_num_procs);
            __cilkrts_free(cpu_array);
            return NULL;
        }

        // Found the right number of processors. Return the
        // array.
        return cpu_array;
    }
    return NULL;
}


/**
 * @brief Sorts the CPU map based on the desired policy for pinning.
 */    
static void pinning_sort_map_for_pin_policy(system_cpu_map *sysmap,
                                            pin_options_t* pin_options)
{
    switch(pin_options->policy) {
    case PIN_SCATTER:
    {
        std::sort(sysmap->worker_to_proc,
                  sysmap->worker_to_proc + sysmap->hardware_thread_count,
                  compare_proc_id_scatter);
        break;
    }

    case PIN_COMPACT:
    {
        std::sort(sysmap->worker_to_proc,
                  sysmap->worker_to_proc + sysmap->hardware_thread_count,
                  compare_proc_id_compact);
        break;
    }
    case PIN_NONE:
        // Do nothing by default.
        break;
    case PIN_MAX_TYPE:
    default:
        cilkos_message(CILK_PIN_MSG_STRING,
                       "ERROR: found invalid pin policy\n");
    }
}

    
/**
 * @brief Build a system map for this machine.
 *
 * More specifically, this method constructs an object @c sysmap,
 * which maps a worker id to a @c proc_id_t struct describing each
 * processor.
 *
 * @param pin_options  Describe the kind of pinning we want to do.
 * @param verbosity    Controls how much print output we want for debugging.
 *
 * @return The system map for this machine, or NULL if we failed to build one.
 */
system_cpu_map* pinning_create_system_map(pin_options_t* pin_options,
                                          int verbosity)
{
    system_cpu_map* gss = NULL;

    if ((pin_options->policy > PIN_NONE) &&
        (pin_options->policy < PIN_MAX_TYPE)) {
        gss = (system_cpu_map*) __cilkrts_malloc(sizeof(system_cpu_map));

        // Get the number of CPUs
        gss->hardware_thread_count = pin_options->expected_num_procs;

        if (verbosity >= 2) {
            cilkos_message(CILK_PIN_MSG_STRING,
                           "Found hardware_thread_count = %d\n",
                           gss->hardware_thread_count);
        }

        if (gss->hardware_thread_count > 0) {
            // Grab the array of processors.
            gss->worker_to_proc = pinning_parse_proc_cpuinfo_file(pin_options->sysinfo,
                                                                  gss->hardware_thread_count,
                                                                  verbosity);
            // If we successfully read in a map, then process for a
            // given pinning
            if (gss->worker_to_proc) {

                // Relabel the hw threads to have ids relative to a
                // core.
                pinning_relabel_hw_threads_and_count_cores(gss);

#ifdef __MIC__
                // If we don't have at least two cores on a KNC,
                // something is wrong...
                CILK_ASSERT(gss->core_count >= 2);

                // KNC, throw out the last core, because it may be
                // used for offload and the OS.
                //
                // WARNING: This call below will only throw out a core
                // if all cores are in the same package (which is
                // currently true on KNC).  If there are multiple
                // packages, the core id will generally be much
                // smaller than gss->core_count (since core_id is
                // relative to package), and this filtering won't do
                // anything.
                gss->hardware_thread_count =
                    pinning_filter_extra_cores(gss->worker_to_proc,
                                               gss->hardware_thread_count,
                                               gss->core_count - 1);
                if (verbosity >= 2) {
                    cilkos_message(CILK_PIN_MSG_STRING,
                                   "After KNC filtering, gss->core_count was %d. we have %d procs left\n",
                                   gss->core_count,
                                   gss->hardware_thread_count);
                }
#endif
                
                // Calclulate min and maximum values for each id.
                compute_cpu_id_range(gss->worker_to_proc,
                                     gss->hardware_thread_count,
                                     &gss->min_cpu, &gss->max_cpu);

                // TBD: Set offset to 0 for now.  This option might be
                // user-specified later.
                gss->wkr0_offset = 0;
                
                // Sort the map for the policy we specify.
                pinning_sort_map_for_pin_policy(gss, pin_options);
            }
        }
    }
    else {
        if (verbosity >= 1) {
            cilkos_message(CILK_PIN_MSG_STRING,
                           "No pinning of Cilk threads.\n");
        }
    }

    if (verbosity >= 1) {
        pinning_print_system_map(gss);
    }

    return gss;
}


void pinning_destroy_system_map(system_cpu_map* sysmap)
{
    if (sysmap) {
        CILK_ASSERT(sysmap->worker_to_proc);
        __cilkrts_free(sysmap->worker_to_proc);
        __cilkrts_free(sysmap);
    }
}

}; // End extern "C"


void pinning_print_proc_id(const char* header, const proc_id_t* cpu)
{
    cilkos_message(CILK_PIN_MSG_STRING, "%s: os_id=%d, HW_THREAD=%d, CORE=%d, PACKAGE=%d\n",
                   header,
                   cpu->os_id,
                   cpu->id[HW_THREAD],
                   cpu->id[CORE],
                   cpu->id[PACKAGE]);
}


void pinning_print_system_map(system_cpu_map* sysmap) {

    if (sysmap) {
        cilkos_message(CILK_PIN_MSG_STRING, "-------------------------------\n");
        cilkos_message(CILK_PIN_MSG_STRING,
                       "System map %p: ",
                       sysmap);
        cilkos_message(CILK_PIN_MSG_STRING,
                       "Hardware thread count = %d\n",
                       sysmap->hardware_thread_count);
        cilkos_message(CILK_PIN_MSG_STRING,
                       "Core count = %d\n",
                       sysmap->core_count);

        for (int i = 0; i < sysmap->hardware_thread_count; ++i) {
            char hstring[100];
#ifdef _WIN32            
            _snprintf_s(hstring, 100, "%d", i);
#else
            snprintf(hstring, 100, "%d", i);
#endif
            pinning_print_proc_id(hstring, &sysmap->worker_to_proc[i]);
        }
        cilkos_message(CILK_PIN_MSG_STRING, "\n");
        pinning_print_proc_id("MinProc", &sysmap->min_cpu);
        pinning_print_proc_id("MaxProc", &sysmap->max_cpu);
        cilkos_message(CILK_PIN_MSG_STRING, "-------------------------------\n");
    }
    else {
        cilkos_message(CILK_PIN_MSG_STRING, "Empty system map.\n");
    }
}

void pinning_report_thread_pin(const char *desc, int32_t wkr_id, int os_id)
{
    cilkos_message(CILK_PIN_MSG_STRING,
                   "Pin worker number %d to %d (%s)\n",
                   wkr_id,
                   os_id,
                   desc);
}


