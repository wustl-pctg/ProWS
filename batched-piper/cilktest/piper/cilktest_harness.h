/*  cilktest_harness.h                  -*- C++ -*-
 *
 *  @copyright
 *  Copyright (C) 2012-2013, Intel Corporation
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
 */

/**
 * @file cilktest_harness.h
 *
 * @brief Test harness for Cilk library code.
 */

#ifndef __CILKTEST_HARNESS_H_
#define __CILKTEST_HARNESS_H_

#ifdef __cplusplus
#   include <cstdio>
#   include <cstdlib>
#   include <cstdarg>
using std::printf;
using std::fprintf;
#else
#   include <stdio.h>
#   include <stdlib.h>
#   include <stdarg.h>
#endif

/**
 * Globals used to control the test program.
 *
 * The static variables declred in these functions are effectively
 * COMDAT because they are inline.  This convoluted way of creating a
 * global variable allows this .h file to be #included in multiple
 * object files.
 */
#ifdef __cplusplus
inline int& __cilktest_verbosity_helper() {
    static int __cilktest_verbosity_val = 2;
    return __cilktest_verbosity_val;
}
inline int& __cilktest_num_errors_helper() {
    static int __cilktest_num_errors_val = 0;
    return __cilktest_num_errors_val;
}
inline int& __cilktest_num_warnings_helper() {
    static int __cilktest_num_warnings_val = 0;
    return __cilktest_num_warnings_val;
}
inline int& __cilktest_max_error_count_helper() {
    static int __cilktest_max_error_count_val = 1;
    return __cilktest_max_error_count_val;
}
inline int& __cilktest_perf_level_helper() {
    static int __cilktest_perf_level_val = 0;
    return __cilktest_perf_level_val;
}

#    define CILKTEST_SET_VERBOSITY(x)  __cilktest_verbosity_helper() = (x)
#    define CILKTEST_VERBOSITY()       __cilktest_verbosity_helper()
#    define CILKTEST_NUM_ERRORS()      __cilktest_num_errors_helper()
#    define CILKTEST_NUM_WARNINGS()    __cilktest_num_warnings_helper()
#    define CILKTEST_MAX_ERROR_COUNT() __cilktest_max_error_count_helper()
#    define CILKTEST_PERF_LEVEL()      __cilktest_perf_level_helper()
#    define CILKTEST_SET_PERF_LEVEL(x) __cilktest_perf_level_helper() = (x)
#else

// Put the TEST_COMDAT macro in front of the definition of a variable that
// should be put into common storage; i.e., duplicate definitions of the
// variable at link time are discarded, effectively folding all of the
// definitions into one.
#ifdef _WIN32
#   define TEST_COMDAT __declspec(selectany)
#else
#   define TEST_COMDAT __attribute__((common))
#endif

TEST_COMDAT int __cilktest_num_errors = 0;
TEST_COMDAT int __cilktest_num_warnings = 0;
TEST_COMDAT int __cilktest_verbosity = 2;
TEST_COMDAT int __cilktest_max_error_count = 1;
TEST_COMDAT int __cilktest_perf_level = 0;

#    define CILKTEST_SET_VERBOSITY(x)  __cilktest_verbosity = (x)
#    define CILKTEST_VERBOSITY()       __cilktest_verbosity
#    define CILKTEST_NUM_ERRORS()      __cilktest_num_errors
#    define CILKTEST_NUM_WARNINGS()    __cilktest_num_warnings
#    define CILKTEST_MAX_ERROR_COUNT() __cilktest_max_error_count
#    define CILKTEST_PERF_LEVEL()      __cilktest_perf_level
#    define CILKTEST_SET_PERF_LEVEL(x) __cilktest_perf_level = (x)
#endif // __cplusplus

#define TEST_VERBOSE (CILKTEST_VERBOSITY() > 0)


#ifdef __cplusplus
// Overloaded functions to print built-in data types in a test driver.  Test
// driver authors should create their own overloads for user-defined types
// that may need to be printed in a test driver.
inline void tfprint(FILE* f, bool v) { fprintf(f, v ? "true" : "false"); }
inline void tfprint(FILE* f, char v) { fprintf(f, "'%c'", v); }
inline void tfprint(FILE* f, unsigned char v) { fprintf(f, "'%c'", v); }
inline void tfprint(FILE* f, signed char v) { fprintf(f, "'%c'", v); }
inline void tfprint(FILE* f, int v) { fprintf(f, "%d", v); }
inline void tfprint(FILE* f, unsigned v) { fprintf(f, "%u", v); }
inline void tfprint(FILE* f, short v) { fprintf(f, "%hd", v); }
inline void tfprint(FILE* f, unsigned short v) { fprintf(f, "%hu", v); }
inline void tfprint(FILE* f, long v) { fprintf(f, "%ld", v); }
inline void tfprint(FILE* f, unsigned long v) { fprintf(f, "%lu", v); }
inline void tfprint(FILE* f, long long v) { fprintf(f, "%lld", v); }
inline void tfprint(FILE* f, unsigned long long v) { fprintf(f, "%llu", v); }
inline void tfprint(FILE* f, const char* v) { fprintf(f, "\"%s\"", v); }
inline void tfprint(FILE* f, const unsigned char* v)
    { fprintf(f, "\"%s\"", v); }
inline void tfprint(FILE* f, const signed char* v) { fprintf(f, "\"%s\"", v); }
inline void tfprint(FILE* f, const void* v) { fprintf(f, "%p", v); }
inline void tfprint(FILE* f, float v) { fprintf(f, "%f", v); }
inline void tfprint(FILE* f, double v) { fprintf(f, "%f", v); }
inline void tfprint(FILE* f, long double v) { fprintf(f, "%Lf", v); }


template <typename T>
inline void CILKTEST_PRINT(int level,
                           const char* pre_msg,
                           T obj,
                           const char* post_msg)
{
    if (CILKTEST_VERBOSITY() < level )
        return;

    // TBD: I'd really like these 3 things to happen atomically.
    // But that requires extra work... :)
    if (pre_msg) {
        std::printf("%s", pre_msg);
    }
    tfprint(stdout, obj);
    if (post_msg) {
        std::printf("%s", post_msg);
    }
}
#endif // __cplusplus

inline static int CILKTEST_PRINTF(int level, const char* fmt, ...)
{
    if (CILKTEST_VERBOSITY() < level)
        return 0;

    va_list ap;
    va_start(ap, fmt);
    int ret = std::vprintf(fmt, ap);
    va_end(ap);
    std::fflush(stdout);
    return ret;
}

inline static void test_assert_fail(const char* file, int line, const char* msg)
{
    std::fprintf(stderr, "%s:%d: Assertion failed: %s\n", file, line, msg);
    if (++CILKTEST_NUM_ERRORS() >= CILKTEST_MAX_ERROR_COUNT())
    {
        std::fprintf(stderr, "Too many errors; quitting\n");
        exit(CILKTEST_NUM_ERRORS());
    }    
}

inline static void test_warning(const char* file, int line, const char* msg)
{
    std::fprintf(stderr, "%s:%d: WARNING: %s\n", file, line, msg);
    CILKTEST_NUM_WARNINGS()++;
}

#define TEST_ASSERT(c) do {                                           \
        if (!(c)) test_assert_fail(__FILE__, __LINE__, #c);           \
    } while (0)

#define TEST_ASSERT_MSG(c, msg) do {                                  \
        if (!(c)) test_assert_fail(__FILE__, __LINE__, msg) ;         \
    } while (0)

#define TEST_WARNING(c) do {                                 \
        if ((c)) test_warning(__FILE__, __LINE__, #c);      \
    } while (0)

#define TEST_WARNING_MSG(c, msg) do {                        \
        if ((c)) test_warning(__FILE__, __LINE__, msg);     \
    } while (0)


#ifdef __cplusplus
#   define TEST_ASSERT_EQ(a,b) do {                                          \
        if (!((a)==(b))) { test_assert_fail(__FILE__, __LINE__, #a" == "#b); \
            fprintf(stderr, "  " #a " == "); tfprint(stderr, (a));           \
            fprintf(stderr, ", " #b " == "); tfprint(stderr, (b));           \
            fprintf(stderr, "\n");                                           \
        }} while (0)

#   define TEST_ASSERT_NE(a,b) do {                                          \
        if (!((a)!=(b))) { test_assert_fail(__FILE__, __LINE__, #a" != "#b); \
            fprintf(stderr, "  " #a " == "); tfprint(stderr, (a));           \
            fprintf(stderr, ", " #b " == "); tfprint(stderr, (b));           \
            fprintf(stderr, "\n");                                           \
        }} while (0)
#endif

// Defining OS-independent sleep function.
#ifdef _WIN32
#   include <windows.h>


    /**
     * @brief OS sleep function [Windows]
     * @param time_in_ms  Time to sleep in ms.
     */
    inline static void cilk_ms_sleep(int time_in_ms)
    {
        Sleep(time_in_ms);
    }

#else
    // Linux
#   include <unistd.h>
    /**
     * @brief OS sleep function [Linux]
     * @param time_in_ms  Time to sleep in ms.
     */
    inline static void cilk_ms_sleep(int time_in_ms)
    {
	usleep(time_in_ms * 1000);
    }
#endif // _WIN32

#define REPORT(...)  CILKTEST_PRINTF(0, __VA_ARGS__)
#define CILKTEST_REMARK(level, ...)  CILKTEST_PRINTF(level, __VA_ARGS__)


// Return nonzero if we are measuring performance.
#define CILKTEST_PERF_RUN() CILKTEST_PERF_LEVEL()

inline void CILKTEST_PARSE_TEST_ARGS(int argc, char* argv[])
{
    if (argc > 1) {
        int verbosity = atoi(argv[1]);
        if ((verbosity >= 0)  && (verbosity <= 5)) {
            CILKTEST_SET_VERBOSITY(atoi(argv[1]));
        }
    }

    // If we see two arguments, we are running a performance test.
    // Yeah, we should really have nicer argument parsing...
    if (argc > 2) {
        int perf_level = atoi(argv[2]);
        CILKTEST_SET_PERF_LEVEL(perf_level);
    }
    CILKTEST_REMARK(3, "CILKTEST: setting verbosity to = %d\n", CILKTEST_VERBOSITY());
}

inline void CILKTEST_BEGIN(const char* testname)
{
    CILKTEST_PRINTF(2, "CILKTEST: Running %s:  ... \n", testname);
}

inline int CILKTEST_END(const char* testname)
{
    CILKTEST_PRINTF(1,
                    "CILKTEST %10s... warnings = %d, errors = %d. %5s.\n",
                    testname,
                    CILKTEST_NUM_WARNINGS(),
                    CILKTEST_NUM_ERRORS(),
                    (CILKTEST_NUM_ERRORS() > 0) ?  "FAILED"  : "PASSED");
    return CILKTEST_NUM_ERRORS();
}



static const char* CILKPUB_PERF_STRING = "CILKPUB_DATA_POINT";

/**
 * @brief Print out the time for a benchmark in a standard format that
 * Cilkpub scripts expect.
 *
 * @param f            File to output results to.
 * @param bench_desc   Name of benchmark being run
 * @param P            Number of processors used to run benchmark
 * @param time         Running time
 * @param input_params List of input parameters, as a string
 * @param output_data  List of output data, as a string
 *
 * TBD: Strip all ',' characters out of the @c benchmark_desc, @c
 * input_params and @c output_data strings.  They will confuse the
 * parsing scripts...
 */
inline static void CILKPUB_PERF_REPORT_TIME(FILE* f,
                                            const char* benchmark_desc,
                                            int P,
                                            double time,
                                            const char* input_params,
                                            const char* output_data) {
    std::fprintf(f, "%s, %f, %d, %s, [%s], [%s]\n",
                 CILKPUB_PERF_STRING,
                 time,
                 P,
                 benchmark_desc ? benchmark_desc : "unknown_bench",
                 input_params ? input_params : "",
                 output_data ? output_data : "");
}

#endif  // !defined(__CILKTEST_HARNESS_H_)
