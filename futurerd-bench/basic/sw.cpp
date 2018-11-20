#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <chrono>

#include <cmath>

#if !SERIAL

#ifndef NO_FUTURES
  #include <cilk/future.hpp>
#endif

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#endif

#include "../util/getoptions.hpp"
#include "../util/ktiming.h"
#include "../util/util.hpp"

#ifndef TIMES_TO_RUN
#define TIMES_TO_RUN 10
#endif

#define SIZE_OF_ALPHABETS 4

static int base_case_log;
#define MIN_BASE_CASE 32
#define NUM_BLOCKS(n) (n >> base_case_log)
#define BLOCK_ALIGN(n) (((n + (1 << base_case_log)-1) >> base_case_log) << base_case_log)  
#define BLOCK_IND_TO_IND(i) (i << base_case_log)

static inline int max(int a, int b) { return (a < b) ? b : a; }
static inline int nearpow2(int x) { return 1 << (32 - __builtin_clz (x - 1)); }
static inline int ilog2(int x) { return 32 - __builtin_clz(x) - 1; }

/**
 * stor : the storage for solutions to subproblems, where stor[i,j] stores the 
 *        longest common subsequence of a[1,i], and b[1,j] (assume string index is
 *        1-based), so stor[0,*] = 0 because we are not considering a at all and
 *        stor[*,0] = 0 because we are not considering b at all.
 * a, b : input strings of size n-1
 * n    : length of input strings + 1
 **/
static int simple_seq_sw(int* stor, char *a, char *b, int n) {

  // solutions to LCS when not considering input b
  for(int i = 0; i < n; i++) { // vertical strip when j == 0
    stor[i*n] = 0;
  }
  // solutions to LCS when not considering input a
  for(int j = 1; j < n; j++) { // horizontal strip when i == 0
    stor[j] = 0;
  }
  for(int i = 1; i < n; i++) {
    for(int j = 1; j < n; j++) {
      int max_val = 0;
      for(int k = 1; k < i; k++) {
        max_val = max(stor[k*n + j]-(i-k), max_val);
      }
      for(int k = 1; k < j; k++) {
        max_val = max(stor[i*n + k]-(j-k), max_val);
      }
      stor[i*n + j] = max(stor[(i-1)*n + (j-1)] + (a[i-1]==b[j-1]), max_val);
    }
  }

  return stor[(n-1)*n + (n-1)];
}

static int 
process_sw_tile(int *stor, char *a, char *b, int n, int iB, int jB) {

  int bSize = 1 << base_case_log;

  for(int i = 0; i < bSize; i++) {
    for(int j = 0; j < bSize; j++) {

      int i_ind = BLOCK_IND_TO_IND(iB) + i;
      int j_ind = BLOCK_IND_TO_IND(jB) + j;

      if(i_ind == 0 || j_ind == 0) {
        stor[i_ind*n + j_ind] = 0;
      } else {
        int max_val = 0;
        for(int k = 1; k < i_ind; k++) {
          max_val = max(stor[k*n + j_ind]-(i_ind-k), max_val);
        }
        for(int k = 1; k < j_ind; k++) {
          max_val = max(stor[i_ind*n + k]-(j_ind-k), max_val);
        }
        stor[i_ind*n + j_ind] = 
          max( stor[(i_ind-1)*n + (j_ind-1)] + (a[i_ind-1]==b[j_ind-1]), 
               max_val );
      }
    }
  }
    
  return 0;
}

#ifdef NO_FUTURES

void recursive_process_sw_tiles(int *stor, char *a, char *b, int n, int ibase, int start, int end) {
  //if (start <= end) {
  //  if (start == end) {
  //    int iB = ibase - start;
  //    process_sw_tile(stor, a, b, n, iB, start);
  //  }
  //} else {

  //  cilk_spawn recursive_process_sw_tiles(stor, a, b, n, ibase, start, ((start+end)/2));
  //  recursive_process_sw_tiles(stor, a, b, n, ibase, (start+end)/2, end);

  //}
  int count = end - start;
  int mid;
  while (count > 1) {
    mid = start + count/2;
    cilk_spawn recursive_process_sw_tiles(stor, a, b, n, ibase, start, mid);
    start = mid;
    count = end - start;
  }
  int iB = ibase - start;
  process_sw_tile(stor, a, b, n, iB, start);
}

int __attribute__((noinline)) wave_sw_with_futures(int *stor, char *a, char *b, int n) {
  int nBlocks = NUM_BLOCKS(n);

  // walk the upper half of triangle, including the diagonal (we assume square NxN SW) 
  for(int wave_front = 0; wave_front < nBlocks; wave_front++) {
    //#pragma cilk grainsize = 1
    //for(int jB = 0; jB <= wave_front; jB++) {
    //  int iB = wave_front - jB;
    //  cilk_spawn process_sw_tile(stor, a, b, n, iB, jB);
    //}
    recursive_process_sw_tiles(stor, a, b, n, wave_front, 0, wave_front+1);
    //cilk_sync;
  }

  // walk the lower half of triangle 
  for(int wave_front = 1; wave_front < nBlocks; wave_front++) {
    int iBase = nBlocks + wave_front - 1;
    //#pragma cilk grainsize = 1
    //for(int jB = wave_front; jB < nBlocks; jB++) {
    //  int iB = iBase - jB;
    //  cilk_spawn process_sw_tile(stor, a, b, n, iB, jB);
    //}
    //cilk_sync;
    recursive_process_sw_tiles(stor, a, b, n, iBase, wave_front, nBlocks);
  }

  return stor[n*(n-1) + n-1];
}

#endif


#ifdef STRUCTURED_FUTURES 


void __attribute__((noinline)) process_sw_tile_helper(cilk::future<void> *fut, int *stor, char *a, char *b, int n, int iB, int jB) {
    FUTURE_HELPER_PREAMBLE;

    process_sw_tile(stor, a, b, n, iB, jB);
    __asm__ volatile ("" ::: "memory");
    void *__cilkrts_deque = fut->put();
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);

    FUTURE_HELPER_EPILOGUE;
}

static int __attribute__((noinline)) wave_sw_with_futures(int *stor, char *a, char *b, int n) {
  CILK_FUNC_PREAMBLE;

  int nBlocks = NUM_BLOCKS(n);

  // create an array of future objects
  //auto farray = (cilk::future<void>*)
  //  malloc(sizeof(cilk::future<void>) * nBlocks * nBlocks);
  cilk::future<void> *farray = new cilk::future<void>[nBlocks*nBlocks];

  // walk the upper half of triangle, including the diagonal (we assume square NxN LCS) 
  for(int wave_front = 0; wave_front < nBlocks; wave_front++) {
    for(int jB = 0; jB <= wave_front; jB++) {
      int iB = wave_front - jB;
      if(iB > 0) { // up dependency
        farray[(iB-1)*nBlocks + jB].get();
      }
      // since we are walking the wavefront serially, no need to get
      // left dependency --- already gotten by previous square.

      START_FIRST_FUTURE_SPAWN;
         process_sw_tile_helper(&farray[iB*nBlocks+jB], stor, a, b, n, iB, jB);
      END_FUTURE_SPAWN;
    }
  }

  // walk the lower half of triangle 
  for(int wave_front = 1; wave_front < nBlocks; wave_front++) {
    int iBase = nBlocks + wave_front - 1;
    for(int jB = wave_front; jB < nBlocks; jB++) {
      int iB = iBase - jB;
      // need to get both up and left dependencies for the last row, 
      // but otherwise just the up dependency. 
      if(iB == (nBlocks - 1) && jB > 0) { // left dependency
        farray[iB*nBlocks + jB - 1].get();
      } 
      if(iB > 0) { // up dependency
        farray[(iB-1)*nBlocks + jB].get();
      }

      START_FIRST_FUTURE_SPAWN;
         process_sw_tile_helper(&farray[iB*nBlocks+jB], stor, a, b, n, iB, jB);
      END_FUTURE_SPAWN;
    }
  }
  // make sure the last square finishes before we move onto returning
  farray[nBlocks * nBlocks - 1].get();

  CILK_FUNC_EPILOGUE;

  delete [] farray;

  return stor[n*(n-1) + n-1];
}
#endif

#ifdef NONBLOCKING_FUTURES
static int process_sw_tile_with_get(cilk::future<void> * farray, int *stor,
                                    char *a, char *b, int n, int iB, int jB) {
    
  int nBlocks = NUM_BLOCKS(n);
    
  if(jB > 0) { // left dependency
    farray[iB*nBlocks + jB - 1].get();
  } 
  if(iB > 0) { // up dependency
    farray[(iB-1)*nBlocks + jB].get();
  } 
    
  process_sw_tile(stor, a, b, n, iB, jB);

  return 0;
}

void __attribute__((noinline)) process_sw_tile_with_get_helper(cilk::future<void> *fut, cilk::future<void> *farray, int *stor,
                                                               char *a, char *b, int n, int iB, int jB) {

  FUTURE_HELPER_PREAMBLE;

  process_sw_tile_with_get(farray, stor, a, b, n, iB, jB);
  void* __cilkrts_deque = fut->put();
  if (__cilkrts_deque) __cilkrts_make_resumable(__cilkrts_deque);

  FUTURE_HELPER_EPILOGUE;
}

//typedef struct wave_sw_context_t {
//  int nBlocks;
//  cilk::future<void> *farray;
//  int *stor;
//  char *a;
//  char *b;
//  int n;
//} wave_sw_context_t;

//void wave_sw_with_futures_loop_body(void *context, uint32_t start, uint32_t end) {
//  CILK_FUNC_PREAMBLE;
//
//  wave_sw_context_t *ctx = (wave_sw_context_t*)context;
//
//  for (int i = start; i < end; i++) {
//    int iB = i / ctx->nBlocks; // row block index 
//    int jB = i % ctx->nBlocks; // col block index
//    START_FIRST_FUTURE_SPAWN;
//        process_sw_tile_with_get_helper(&ctx->farray[i], ctx->farray, ctx->stor, ctx->a, ctx->b, ctx->n, iB, jB);
//    END_FUTURE_SPAWN;
//  }
//
//  CILK_FUNC_EPILOGUE;
//}

static void __attribute__((noinline)) launch_sw_gf(int *stor, char *a, char *b, int n, int nBlocks, cilk::future<void>* farray, int loc) {
  CILK_FUNC_PREAMBLE;
  
  int iB = loc / nBlocks;
  int jB = loc % nBlocks;

  START_FIRST_FUTURE_SPAWN;
      process_sw_tile_with_get_helper(&farray[loc], farray, stor, a, b, n, iB, jB);
  END_FUTURE_SPAWN;

  CILK_FUNC_EPILOGUE;
}

static void recursive_wave_sw_with_futures(int *stor, char *a, char *b, int n, int nBlocks, cilk::future<void>* farray, int start, int end);

static void __attribute__((noinline)) recursive_wave_sw_with_futures_helper(int *stor, char *a, char *b, int n, int nBlocks, cilk::future<void>* farray, int start, int end) {
    SPAWN_HELPER_PREAMBLE;

    recursive_wave_sw_with_futures(stor, a, b, n, nBlocks, farray, start, end);

    SPAWN_HELPER_EPILOGUE;
}

static void __attribute__((noinline)) recursive_wave_sw_with_futures(int *stor, char *a, char *b, int n, int nBlocks, cilk::future<void>* farray, int start, int end) {
  CILK_FUNC_PREAMBLE;

  int count = end - start;
  int mid;

  while (count > 1) {
    mid = start + count/2;
    //cilk_spawn recursive_wave_sw_with_futures(stor, a, b, n, nBlocks, farray, start, mid);
    if (!CILK_SETJMP(sf.ctx)) {
      recursive_wave_sw_with_futures_helper(stor, a, b, n, nBlocks, farray, start, mid);
    }
    start = mid;
    count = end - start;
  }

  launch_sw_gf(stor, a, b, n, nBlocks, farray, start);
  CILK_FUNC_EPILOGUE;
}

static int wave_sw_with_futures(int *stor, char *a, char *b, int n) {

  int nBlocks = NUM_BLOCKS(n);
  int blocks = nBlocks * nBlocks;
    
  //auto *farray = (cilk::future<void>*)
  //  malloc(sizeof(cilk::future<void>) * blocks);
  cilk::future<void>* farray = new cilk::future<void>[blocks];

  //wave_sw_context_t ctx = {
  //  .nBlocks = nBlocks,
  //  .farray = farray,
  //  .stor = stor,
  //  .a = a,
  //  .b = b,
  //  .n = n
  //};
  //__cilkrts_cilk_for_32(wave_sw_with_futures_loop_body, &ctx, blocks, 0);
  recursive_wave_sw_with_futures(stor, a, b, n, nBlocks, farray, 0, blocks);
  
  // make sure the last square finishes before we move onto returning
  farray[blocks-1].get();

  //free(farray);
  delete [] farray;
    
  return stor[n*(n-1) + n-1];
}
#endif

static void do_check(int *stor1, char *a1, char *b1, int n, int result) {
  char *a2 = (char *)malloc(n * sizeof(char));
  char *b2 = (char *)malloc(n * sizeof(char));
  int *stor2 = (int *) malloc(sizeof(int) * n * n);

  memcpy(a2, a1, n * sizeof(char));
  memcpy(b2, b1, n * sizeof(char));

  int result2 = simple_seq_sw(stor2, a2, b2, n);
  assert(result2 == result);

  for(int i=0; i < n; i++) {
    for(int j=0; j < n; j++) {
      assert(stor1[i*n + j] == stor2[i*n + j]);
    }
  }

  fprintf(stderr, "Check passed.\n");

  free(a2);
  free(b2);
  free(stor2);
}

const char* specifiers[] = {"-n", "-c", "-h", "-b"};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, INTARG};

int main(int argc, char *argv[]) {
//#if REACH_MAINT && (!RACE_DETECT)
//    futurerd_disable_shadowing();
//#endif

  int n = 1024;
  int bSize = 0;
  int check = 0, help = 0;

  get_options(argc, argv, specifiers, opt_types, &n, &check, &help, &bSize);

  if(help) {
    fprintf(stderr, "Usage: sw [-n size] [-c] [-h]\n");
    fprintf(stderr, "\twhere -n specifies string length (default: 1024)\n");
    fprintf(stderr, "\t -b specifies base cast size (default: sqrt(n))\n");
    fprintf(stderr, "\tcheck results if -c is set\n");
    fprintf(stderr, "\toutput this message if -h is set\n");
    exit(1);
  }

  //ensure_serial_execution();

  if (bSize == 0) {
    bSize = std::sqrt(n);

    // Minimum base case size, but only if not set explicitly
    if (bSize < MIN_BASE_CASE) bSize = MIN_BASE_CASE;
  }

  // Nearest power of 2
  bSize = nearpow2(bSize);
  base_case_log = ilog2(bSize);
     
  n = BLOCK_ALIGN(n); // round it to be 64-byte aligned

  // Must be less than sqrt(2**31 - 1)
  if (n > 46340) {
      fprintf(stderr, "n must be < 46340! (n was rounded to %d)\n", n);
      exit(1);
  }

  printf("Compute SmithWaterman with %d x %d table.\n", n, n);

  // str len is n-1, but allocated n to have the last char be \0
  char *a1 = (char *)malloc(n * sizeof(char));
  char *b1 = (char *)malloc(n * sizeof(char));
  int result = 0;

  /* Generate random inputs; a/b[n-1] not used */
  gen_rand_string(a1, n-1, SIZE_OF_ALPHABETS);
  gen_rand_string(b1, n-1, SIZE_OF_ALPHABETS);
  a1[n-1] = '\0';
  b1[n-1] = '\0';

#if SERIAL
  printf("Performing SW serially.\n");
#elif NONBLOCKING_FUTURES
  printf("Performing SW with non-structured futures and %d x %d base case.\n",
         bSize, bSize);
#elif defined(STRUCTURED_FUTURES) // STRUCTURED_FUTURE
  printf("Performing SW with structured futures and %d x %d base case.\n",
         bSize, bSize);
#else
  printf("Performing SW with cilk-for and %d x %d base case.\n",
         bSize, bSize);
#endif

  uint64_t running_time[TIMES_TO_RUN];

  __cilkrts_init();
  int *stor1 = NULL;
  for (int i = 0; i < TIMES_TO_RUN; i++) {
    stor1 = (int *) malloc(sizeof(int) * n * n);
  //auto start = std::chrono::steady_clock::now();
    auto start = ktiming_getmark();
#if SERIAL
    result = simple_seq_sw(stor1, a1, b1, n);
#else
    result = wave_sw_with_futures(stor1, a1, b1, n);
#endif
    //auto end = std::chrono::steady_clock::now();
    auto end = ktiming_getmark();
    running_time[i] = ktiming_diff_usec(&start, &end);
    if (i < TIMES_TO_RUN-1) free(stor1);
  } 
  if(check && stor1) { do_check(stor1, a1, b1, n, result); }
    
  printf("Result: %d\n", result);
    
  print_runtime(running_time, TIMES_TO_RUN);
  //auto time = std::chrono::duration <double, std::milli> (end-start).count();
  //printf("Benchmark time: %f ms\n", time);

  free(a1);
  free(b1);
  free(stor1);

  return 0;
}
