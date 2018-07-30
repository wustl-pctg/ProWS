/**
 * Matrix multiplication using futures
 * C[i,j] = A[i,k] * B[k, j]
 * The matrix layout is morton Z at the block level, and row-major in
 * the base case.
 **/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <future.hpp>
#include <chrono>
#include <algorithm>

#include <cmath>

#include "../util/getoptions.hpp"
#include "../util/util.hpp"
#include "rd.h"

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

// Don't make base case too large --- tmp matrices allocated on stack
static int ITER_BASE_CASE, REC_BASE_CASE; // 2^POWER
static int ITER_POWER, REC_POWER;
//#define MIN_BASE_CASE 16

static inline int nearpow2(int x) { return 1 << (32 - __builtin_clz (x - 1)); }
static inline int ilog2(int x) { return 32 - __builtin_clz(x) - 1; }

// The following three should match
#define DATA float
#if DATA == float
#define do_check_iter(C,I,n) do_check_iter_float(C,I,n) // use what DATA defined to be
#define init(A,n) init_float(A,n)                       // use what DATA defined to be
#else
#define do_check_iter(C,I,n) do_check_iter_int(C,I,n) // use what DATA defined to be
#define init(A,n) init_int(A,n)                       // use what DATA defined to be
#endif

#define ERR_THRESH 0.0001

static const unsigned int Q[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF};
static const unsigned int S[] = {1, 2, 4, 8};

unsigned long rand_nxt = 0;

static int cilk_rand(void) {
  int result;
  rand_nxt = rand_nxt * 1103515245 + 12345;
  result = (rand_nxt >> 16) % ((unsigned int) RAND_MAX + 1);
  return result;
}


// provides a look up for the Morton Number of the z-order curve given the x and y coordinate
// every instance of an (x,y) lookup must use this function
static unsigned int z_convert(int row, int col) {

  unsigned int z; // z gets the resulting 32-bit Morton Number.
  // x and y must initially be less than 65536.
  // The top and the left boundary

  col = (col | (col << S[3])) & Q[3];
  col = (col | (col << S[2])) & Q[2];
  col = (col | (col << S[1])) & Q[1];
  col = (col | (col << S[0])) & Q[0];

  row = (row | (row << S[3])) & Q[3];
  row = (row | (row << S[2])) & Q[2];
  row = (row | (row << S[1])) & Q[1];
  row = (row | (row << S[0])) & Q[0];

  z = col | (row << 1);

  return z;
}

// converts (x,y) position in the array to the mixed z-order row major layout
static int block_convert(int row, int col) {
  int block_index = z_convert(row >> REC_POWER, col >> REC_POWER);
  // BASE_CASE << REC_POWER == BASE_CASE * BASE_CASE
  return (block_index * REC_BASE_CASE << REC_POWER)
    + ((row - ((row >> REC_POWER) << REC_POWER)) << REC_POWER)
    + (col - ((col >> REC_POWER) << REC_POWER));
}

// init the matrix to random numbers
// static void init_int(DATA *M, int n) {
//   for(int i = 0; i < n; i++) {
//     for(int j = 0; j < n; j++) {
//       M[block_convert(i,j)] = (DATA) cilk_rand();
//     }
//   }
// }

static void init_float(DATA *M, int n) {
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      M[block_convert(i,j)] = (DATA) ((float)cilk_rand() / (float)RAND_MAX) - 0.5f;
    }
  }
}

// zero the matrix
template <typename T>
static void zero(T *M, int n) {
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      M[block_convert(i,j)] = 0;
    }
  }
}

// iterative solution for matrix multiplication
static void seq_iter_matmul(DATA *A, DATA *B, DATA *C, int n) {
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      DATA c = 0.0;
      for(int k = 0; k < n; k++) {
        c += A[block_convert(i,k)] * B[block_convert(k,j)];
      }
      C[block_convert(i, j)] = c;
    }
  }
}

static void do_check_iter_float(DATA *C, DATA *I, int n) {
  double max_err = 0.0;
  for(int i=0; i < n; i++) {
    for(int j=0; j < n; j++) {
      double diff = (C[i*n+j] - I[i*n+j]);
      if(diff < 0) { diff = -diff; }
      if(diff > max_err) { max_err = diff; }
      assert(diff < ERR_THRESH);
    }
  }
  fprintf(stderr, "Check passed; max error: %g.\n", max_err);
}

// static void do_check_iter_int(DATA *C, DATA *I, int n) {

//   for(int i = 0; i < n; i++) {
//     for(int j = 0; j < n; j++) {
//       assert(C[i*n + j] == I[i*n + j]);
//     }
//   }

//   fprintf(stderr, "Check passed.\n");
// }

// fhandles are a 3D array with nBlocks x nBlocks x nBlocks dimensions, with
// the kB dimension being the outer most, followed by iB, followed by jB.
// i.e. fhandles[kB][iB][jB]
static int fh_index(int kB, int iB, int jB, int nBlocks) {
  int nB2 = nBlocks * nBlocks;
  return (kB * nB2 + iB * nBlocks + jB);
}

#ifdef STRUCTURED_FUTURES
// C[i,j] = A[i,k] x B[k,j]
// iB = block index in dimension i
// kB = block index in dimension k
// jB = block index in dimension j
static int
matmul_base_structured(DATA *A, DATA *B, DATA *C,
                       int iB, int kB, int jB, cilk::future<int> *f) {
  int n = REC_BASE_CASE;
  int block_index_A = z_convert(iB, kB);
  int block_index_B = z_convert(kB, jB);
  int block_index_C = z_convert(iB, jB);

  // start address of the actual blocks that we should use to compute
  DATA *myA = &A[(block_index_A) << (REC_POWER*2)];
  DATA *myB = &B[(block_index_B) << (REC_POWER*2)];
  DATA *myC = &C[(block_index_C) << (REC_POWER*2)];

  DATA tmp[n*n];
#ifdef RACE_DETECT
  race_detector::mark_stack_allocate(&tmp);
#endif

  for(int ii = 0; ii < n; ii++) {
    for(int jj = 0; jj < n; jj++) {
      DATA c = 0.0;
      for(int kk = 0; kk < n; kk++) {
        c += myA[ii*n + kk] * myB[kk*n + jj];
      }
      tmp[ii*n + jj] = c;
    }
  }
  // make sure the previous kB that wrote to the same block C[iB, jB] is done
  if(f != NULL) f->get();

  for(int ii = 0; ii < n; ii++) {
    for(int jj = 0; jj < n; jj++) {
      myC[ii*n + jj] += tmp[ii*n + jj];
    }
  }

  return 0;
}

// Structured
static void do_matmul(DATA *A, DATA *B, DATA *C, int n) {

  printf("Performing structured matmul with z-layout, %d x %d with base case %d x %d.\n",
         n, n, REC_BASE_CASE, REC_BASE_CASE);

  int nBlocks = n >> REC_POWER; // number of blocks per dimension
  int num_futures = nBlocks * nBlocks * nBlocks;
  auto fhandles = (cilk::future<int>*)
    malloc(sizeof(cilk::future<int>) * num_futures);

  auto start = std::chrono::steady_clock::now();
  cilk_for(int iB = 0; iB < nBlocks; iB++) {
    cilk_for(int jB = 0; jB < nBlocks; jB++) {
      cilk::future<int> *f = NULL;
      cilk::future<int> *prevf = NULL;
      for(int kB = 0; kB < nBlocks; kB++) {
        prevf = f; // first is NULL
        f = &(fhandles[fh_index(kB, iB, jB, nBlocks)]);
        reasync_helper<int,DATA*,DATA*,DATA*,int,int,int,decltype(prevf)>
          (f, matmul_base_structured, A, B, C, iB, kB, jB, prevf);
      }
    }
  }

  // wait for the very last kB blocks to finish
  cilk_for(int iB = 0; iB < nBlocks; iB++) {
    cilk_for(int jB = 0; jB < nBlocks; jB++) {
      fhandles[fh_index(nBlocks-1, iB, jB, nBlocks)].get();
    }
  }
  auto end = std::chrono::steady_clock::now();
  auto time = std::chrono::duration <double, std::milli> (end-start).count();
  printf("Benchmark time: %f ms\n", time);

  free(fhandles);
}
#endif // STRUCTURED_FUTURES

#ifdef NONBLOCKING_FUTURES
// use static global to avoid parameter proliferation
static int g_nBlocks; // number of blocks in the original matrices
static cilk::future<int> *g_fhandles; // future handles

static int __attribute__ ((noinline))
matmul_iter(DATA *A, DATA *B, DATA *C, int n, int iB, int kB, int jB) {
  DATA tmp[n*n];
#ifdef RACE_DETECT
  race_detector::mark_stack_allocate(&tmp);
#endif

  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      DATA c = 0.0;
      for(int k = 0; k < n; k++) {
        c += A[i*n + k] * B[k*n + j];
      }
      tmp[i*n + j] = c;
    }
  }

  // make sure the previous kB that wrote to the same block C[iB, jB] is done
  if(kB > 0) g_fhandles[fh_index(kB-1, iB, jB, g_nBlocks)].get();

  for(int i = 0; i < n; i++) {
    for(int j = 0; j < n; j++) {
      C[i*n + j] += tmp[i*n + j];
    }
  }

  return 0;
}

int matmul_rec(DATA *A, DATA *B, DATA *C, int n, int iB, int kB, int jB) {
  // Base case uses row-major order; switch to iterative traversal
  if(n == REC_BASE_CASE) return matmul_iter(A, B, C, n, iB, kB, jB);

  int block_inc = (n >> REC_POWER) >> 1;

  // partition each matrix into 4 sub matrices
  // each sub-matrix points to the start of the z pattern
  DATA *A1 = &A[block_convert(0,0)];
  DATA *A2 = &A[block_convert(0, n >> 1)]; //bit shift to divide by 2
  DATA *A3 = &A[block_convert(n >> 1,0)];
  DATA *A4 = &A[block_convert(n >> 1, n >> 1)];

  DATA *B1 = &B[block_convert(0,0)];
  DATA *B2 = &B[block_convert(0, n >> 1)];
  DATA *B3 = &B[block_convert(n >> 1, 0)];
  DATA *B4 = &B[block_convert(n >> 1, n >> 1)];

  DATA *C1 = &C[block_convert(0,0)];
  DATA *C2 = &C[block_convert(0, n >> 1)];
  DATA *C3 = &C[block_convert(n >> 1,0)];
  DATA *C4 = &C[block_convert(n >> 1, n >> 1)];

  // recursively call the sub-matrices for evaluation in parallel
  matmul_rec(A1, B1, C1, n >> 1, iB, kB, jB);
  matmul_rec(A1, B2, C2, n >> 1, iB, kB, jB + block_inc);
  matmul_rec(A3, B1, C3, n >> 1, iB + block_inc, kB, jB);
  matmul_rec(A3, B2, C4, n >> 1, iB + block_inc, kB, jB + block_inc);

  matmul_rec(A2, B3, C1, n >> 1, iB, kB + block_inc, jB);
  matmul_rec(A2, B4, C2, n >> 1, iB, kB + block_inc, jB + block_inc);
  matmul_rec(A4, B3, C3, n >> 1, iB + block_inc, kB + block_inc, jB);
  matmul_rec(A4, B4, C4, n >> 1, iB + block_inc, kB + block_inc, jB + block_inc);

  return 0;
}

// matmul using unstructured future; divide and conquer untill we reach base case,
// C[i,j] = A[i,k] x B[k,j]
// n = current block size
// iB: block index for the i dimension
// kB: block index for the k dimension
// jB: block index for the j dimension
int matmul(DATA *A, DATA *B, DATA *C, int n, int iB, int kB, int jB) {
  // the last two could have been global var

  // Base case uses row-major order; switch to iterative traversalw
  // if(n == BASE_CASE) {
  //   auto f = &(g_fhandles[fh_index(kB, iB, jB, g_nBlocks)]);
  //   reasync_helper<int,DATA*,DATA*,DATA*,int,int,int,int>
  //     (f, matmul_base, A, B, C, n, iB, kB, jB);

  //   return 0;
  // }
  if (n == REC_BASE_CASE) {
    auto f = &(g_fhandles[fh_index(kB, iB, jB, g_nBlocks)]);
    reasync_helper<int,DATA*,DATA*,DATA*,int,int,int,int>
      (f, matmul_rec, A, B, C, n, iB, kB, jB);
    return 0;
  }

  int block_inc = (n >> REC_POWER) >> 1;

  // partition each matrix into 4 sub matrices
  // each sub-matrix points to the start of the z pattern
  DATA *A1 = &A[block_convert(0,0)];
  DATA *A2 = &A[block_convert(0, n >> 1)]; //bit shift to divide by 2
  DATA *A3 = &A[block_convert(n >> 1,0)];
  DATA *A4 = &A[block_convert(n >> 1, n >> 1)];

  DATA *B1 = &B[block_convert(0,0)];
  DATA *B2 = &B[block_convert(0, n >> 1)];
  DATA *B3 = &B[block_convert(n >> 1, 0)];
  DATA *B4 = &B[block_convert(n >> 1, n >> 1)];

  DATA *C1 = &C[block_convert(0,0)];
  DATA *C2 = &C[block_convert(0, n >> 1)];
  DATA *C3 = &C[block_convert(n >> 1,0)];
  DATA *C4 = &C[block_convert(n >> 1, n >> 1)];

  // recrusively call the sub-matrices for evaluation in parallel
  cilk_spawn matmul(A1, B1, C1, n >> 1, iB, kB, jB);
  cilk_spawn matmul(A1, B2, C2, n >> 1, iB, kB, jB + block_inc);
  cilk_spawn matmul(A3, B1, C3, n >> 1, iB + block_inc, kB, jB);
  cilk_spawn matmul(A3, B2, C4, n >> 1, iB + block_inc, kB, jB + block_inc);

  cilk_spawn matmul(A2, B3, C1, n >> 1, iB, kB + block_inc, jB);
  cilk_spawn matmul(A2, B4, C2, n >> 1, iB, kB + block_inc, jB + block_inc);
  cilk_spawn matmul(A4, B3, C3, n >> 1, iB + block_inc, kB + block_inc, jB);
  cilk_spawn matmul(A4, B4, C4, n >> 1, iB + block_inc, kB + block_inc, jB + block_inc);
  cilk_sync; // this just sync for the spawning of futures but don't wait for them to finish

  return 0;
}

// "unstructured"
static void do_matmul(DATA *A, DATA *B, DATA *C, int n) {

  printf("Performing unstructured matmul with z-layout, %d x %d with base case %d x %d.\n",
         n, n, REC_BASE_CASE, REC_BASE_CASE);

  // initialize the static global vars
  g_nBlocks = n >> REC_POWER; // number of blocks per dimension
  int num_futures = g_nBlocks * g_nBlocks * g_nBlocks;
  g_fhandles = (cilk::future<int>*) malloc(sizeof(cilk::future<int>) * num_futures);

  auto start = std::chrono::steady_clock::now();
  matmul(A, B, C, n, 0, 0, 0);


  // make sure we get the last kB layer of futures before we return
  cilk_for(int i = 0; i < (g_nBlocks * g_nBlocks); i++) {
    g_fhandles[(g_nBlocks-1)*(g_nBlocks * g_nBlocks) + i].get();
  }
  auto end = std::chrono::steady_clock::now();
  auto time = std::chrono::duration <double, std::milli> (end-start).count();
  printf("Benchmark time: %f ms\n", time);

  free(g_fhandles);
  g_fhandles = NULL;
}
#endif // NONBLOCKING_FUTURES

const char * specifiers[] = {"-n", "-c", "-h", "-b", "-r", 0};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, INTARG, INTARG, 0};

int main(int argc, char *argv[]) {
#if REACH_MAINT && (!RACE_DETECT)
    futurerd_disable_shadowing();
#endif

  int n = 1024; // default n value
  int bSize = 32; // iterative base case;
  int rSize = 0; // recursive base case (lose parallelism)
  int help = 0;
  int check = 0;

  get_options(argc, argv, specifiers, opt_types, &n, &check, &help, &bSize, &rSize);
  ensure_serial_execution();

  if(help) {
    fprintf(stderr, "%s [-n <n>|-b <b>|-c|-h]\n", argv[0]);
    exit(0);
  }

  if (rSize == 0) rSize = std::sqrt(n);

  // Nearest power of 2
  ITER_BASE_CASE = nearpow2(bSize);
  REC_BASE_CASE = std::max(nearpow2(rSize), ITER_BASE_CASE);
  ITER_POWER = ilog2(ITER_BASE_CASE);
  REC_POWER = ilog2(REC_BASE_CASE);
  assert(REC_BASE_CASE <= n);

  DATA *A, *B, *C, *I = NULL;

  A = (DATA *) malloc(n * n * sizeof(DATA)); // source matrix
  B = (DATA *) malloc(n * n * sizeof(DATA)); // source matrix
  C = (DATA *) malloc(n * n * sizeof(DATA)); // result matrix

  init(A, n);
  init(B, n);

  if(check) {
    I = (DATA *) malloc(n * n * sizeof(DATA)); //iter result matrix
    zero<DATA>(I, n);
    seq_iter_matmul(A, B, I, n);
  }

  zero<DATA>(C, n);
  do_matmul(A, B, C, n);

  if(check) { do_check_iter(C, I, n); }

  // clean up memory
  free(A);
  free(B);
  free(C);
  if(check) { free(I); }

  return 0;
}
