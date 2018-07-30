#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <chrono>

#include <cmath>

#if !SERIAL
#include <future.hpp>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "rd.h"
#endif

#include "../util/getoptions.hpp"
#include "../util/util.hpp"

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

static inline int 
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

#ifdef STRUCTURED_FUTURES 
static int wave_sw_with_futures(int *stor, char *a, char *b, int n) {

  int nBlocks = NUM_BLOCKS(n);

  // create an array of future objects
  auto farray = (cilk::future<int>*)
    malloc(sizeof(cilk::future<int>) * nBlocks * nBlocks);


  // walk the upper half of triangle, including the diagonal (we assume square NxN LCS) 
  for(int wave_front = 0; wave_front < nBlocks; wave_front++) {
    for(int jB = 0; jB <= wave_front; jB++) {
      int iB = wave_front - jB;
      if(iB > 0) { // up dependency
        farray[(iB-1)*nBlocks + jB].get();
      } 
      // since we are walking the wavefront serially, no need to get
      // left dependency --- already gotten by previous square.
      reasync_helper<int,int*,char*,char*,int,int,int>
        (&farray[iB*nBlocks+jB], process_sw_tile, stor, a, b, n, iB, jB);
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
      reasync_helper<int,int*,char*,char*,int,int,int>
        (&farray[iB*nBlocks+jB], process_sw_tile, stor, a, b, n, iB, jB);

    }
  }
  // make sure the last square finishes before we move onto returning
  farray[nBlocks * nBlocks - 1].get();
  free(farray);

  return stor[n*(n-1) + n-1];
}
#endif

#ifdef NONBLOCKING_FUTURES
static int process_sw_tile_with_get(cilk::future<int> * farray, int *stor,
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

static int wave_sw_with_futures(int *stor, char *a, char *b, int n) {

  int nBlocks = NUM_BLOCKS(n);
  int blocks = nBlocks * nBlocks;
    
  auto *farray = (cilk::future<int>*)
    malloc(sizeof(cilk::future<int>) * blocks);

  cilk_for(int i=0; i < blocks; i++) {
    int iB = i / nBlocks; // row block index 
    int jB = i % nBlocks; // col block index
    reasync_helper<int,decltype(farray),int*,char*,char*,int,int,int>
      (&farray[i], process_sw_tile_with_get, farray, stor, a, b, n, iB, jB);
  }
  // make sure the last square finishes before we move onto returning
  farray[blocks-1].get();
  free(farray);
    

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
#if REACH_MAINT && (!RACE_DETECT)
    futurerd_disable_shadowing();
#endif

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

  ensure_serial_execution();

  if (bSize == 0) {
    bSize = std::sqrt(n);

    // Minimum base case size, but only if not set explicitly
    if (bSize < MIN_BASE_CASE) bSize = MIN_BASE_CASE;
  }

  // Nearest power of 2
  bSize = nearpow2(bSize);
  base_case_log = ilog2(bSize);
     
  n = BLOCK_ALIGN(n); // round it to be 64-byte aligned
  printf("Compute SmithWaterman with %d x %d table.\n", n, n);

  // str len is n-1, but allocated n to have the last char be \0
  char *a1 = (char *)malloc(n * sizeof(char));
  char *b1 = (char *)malloc(n * sizeof(char));
  int *stor1 = (int *) malloc(sizeof(int) * n * n);
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
#else // STRUCTURED_FUTURE
  printf("Performing SW with structured futures and %d x %d base case.\n",
         bSize, bSize);
#endif

  auto start = std::chrono::steady_clock::now();
#if SERIAL
  result = simple_seq_sw(stor1, a1, b1, n);
#else
  result = wave_sw_with_futures(stor1, a1, b1, n);
#endif
  auto end = std::chrono::steady_clock::now();
    
  if(check) { do_check(stor1, a1, b1, n, result); }
    
  printf("Result: %d\n", result);
    
  auto time = std::chrono::duration <double, std::milli> (end-start).count();
  printf("Benchmark time: %f ms\n", time);

  free(a1);
  free(b1);
  free(stor1);

  return 0;
}
