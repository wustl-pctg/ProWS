/*
 * Copyright (c) 1996 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to use, copy, modify, and distribute the Software without
 * restriction, provided the Software, including any modified copies made
 * under this license, is not distributed for a fee, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE MASSACHUSETTS INSTITUTE OF TECHNOLOGY BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the name of the Massachusetts
 * Institute of Technology shall not be used in advertising or otherwise
 * to promote the sale, use or other dealings in this Software without
 * prior written authorization from the Massachusetts Institute of
 * Technology.
 *  
 */


#ifndef SERIAL
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#else
#define cilk_spawn 
#define cilk_sync
#define __cilkrts_accum_timing()
#define __cilkrts_init()
#define __cilkrts_reset_timing()
#endif

#define __cilkrts_accum_timing()
#define __cilkrts_reset_timing()

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TIMING_COUNT
#define TIMING_COUNT 10
#endif


#include "getoptions.h"
#include "ktiming.h"
#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

#include "internal/abi.h"

extern "C" {
  void __cilkrts_cilk_for_32(void (*body) (void *, uint32_t, uint32_t),
      void *context, uint32_t count, int grain);
void __cilkrts_pop_frame(__cilkrts_stack_frame*);
void __cilkrts_detach(__cilkrts_stack_frame*);
}

#define SizeAtWhichDivideAndConquerIsMoreEfficient 64
#define SizeAtWhichNaiveAlgorithmIsMoreEfficient 16
#define CacheBlockSizeInBytes 32

/* The real numbers we are using --- either double or float */
typedef double REAL;
typedef unsigned long PTR;

/* maximum tolerable relative error (for the checking routine) */
#define EPSILON (1.0E-6)

/* 
 * Matrices are stored in row-major order; A is a pointer to
 * the first element of the matrix, and an is the number of elements
 * between two rows. This macro produces the element A[i,j]
 * given A, an, i and j
 */
#define ELEM(A, an, i, j) (A[(i) * (an) + (j)])

unsigned long rand_nxt = 0;

int cilk_rand(void) {
  int result;
  rand_nxt = rand_nxt * 1103515245 + 12345;
  result = (rand_nxt >> 16) % ((unsigned int) RAND_MAX + 1);
  return result;
}

/* 
 * ANGE: 
 * recursively multiply an m x n matrix A with size n vector V, and store 
 * result in vector size m P.  The value rw is the row width of A, and 
 * add the result into P if variable add != 0
 */
void mat_vec_mul(int m, int n, int rw, REAL *A, REAL *V, REAL *P, int add) {

  if((m + n) <= 64) { // base case 
    int i, j;

    if(add) {
      for(i = 0; i < m; i++) {
        REAL c = 0;
        for(j = 0; j < n; j++) {
          c += ELEM(A, rw, i, j) * V[j];
        }
        P[i] += c;
      }
    } else {
      for(i = 0; i < m; i++) {
        REAL c = 0;
        for(j = 0; j < n; j++) {
          c += ELEM(A, rw, i, j) * V[j];
        }
        P[i] = c;
      }
    }

  } else if( m >= n ) { // cut m dimension 
    int m1 = m >> 1;
    mat_vec_mul(m1, n, rw, A, V, P, add);
    mat_vec_mul(m - m1, n, rw, &ELEM(A, rw, m1, 0), V, P + m1, add);

  } else { // cut n dimension 
    int n1 = n >> 1;
    mat_vec_mul(m, n1, rw, A, V, P, add);
    // sync here if parallelized 
    mat_vec_mul(m, n - n1, rw, &ELEM(A, rw, 0, n1), V + n1, P, 1);
  }
}

/*
 * Naive sequential algorithm, for comparison purposes
 */
void matrixmul(int n, REAL *A, int an, REAL *B, int bn, REAL *C, int cn) {

  int i, j, k;
  REAL s;

  for (i = 0; i < n; ++i)
    for (j = 0; j < n; ++j) {
      s = 0.0;
      for (k = 0; k < n; ++k)
        s += ELEM(A, an, i, k) * ELEM(B, bn, k, j);

      ELEM(C, cn, i, j) = s;
    }
}



/*****************************************************************************
**
** FastNaiveMatrixMultiply
**
** For small to medium sized matrices A, B, and C of size
** MatrixSize * MatrixSize this function performs the operation
** C = A x B efficiently.
**
** Note MatrixSize must be divisible by 8.
**
** INPUT:
**    C = (*C WRITE) Address of top left element of matrix C.
**    A = (*A IS READ ONLY) Address of top left element of matrix A.
**    B = (*B IS READ ONLY) Address of top left element of matrix B.
**    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
**    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
**    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
**    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
**
** OUTPUT:
**    C = (*C WRITE) Matrix C contains A x B. (Initial value of *C undefined.)
**
*****************************************************************************/
void FastNaiveMatrixMultiply(REAL *C, const REAL *A, const REAL *B,
                 unsigned MatrixSize, unsigned RowWidthC,
                 unsigned RowWidthA, unsigned RowWidthB) { 

  /* Assumes size of real is 8 bytes */
  PTR RowWidthBInBytes = RowWidthB  << 3;
  PTR RowWidthAInBytes = RowWidthA << 3;
  PTR MatrixWidthInBytes = MatrixSize << 3;
  PTR RowIncrementC = ( RowWidthC - MatrixSize) << 3;
  unsigned Horizontal, Vertical;
#ifdef DEBUG_ON
  REAL *OLDC = C;
  REAL *TEMPMATRIX;
#endif

  const REAL *ARowStart = A;
  for (Vertical = 0; Vertical < MatrixSize; Vertical++) {
    for (Horizontal = 0; Horizontal < MatrixSize; Horizontal += 8) {
      const REAL *BColumnStart = B + Horizontal;
      REAL FirstARowValue = *ARowStart++;

      REAL Sum0 = FirstARowValue * (*BColumnStart);
      REAL Sum1 = FirstARowValue * (*(BColumnStart+1));
      REAL Sum2 = FirstARowValue * (*(BColumnStart+2));
      REAL Sum3 = FirstARowValue * (*(BColumnStart+3));
      REAL Sum4 = FirstARowValue * (*(BColumnStart+4));
      REAL Sum5 = FirstARowValue * (*(BColumnStart+5));
      REAL Sum6 = FirstARowValue * (*(BColumnStart+6));
      REAL Sum7 = FirstARowValue * (*(BColumnStart+7)); 

      unsigned Products;
      for (Products = 1; Products < MatrixSize; Products++) {
        REAL ARowValue = *ARowStart++;
        BColumnStart = (REAL*) (((PTR) BColumnStart) + RowWidthBInBytes);

        Sum0 += ARowValue * (*BColumnStart);
        Sum1 += ARowValue * (*(BColumnStart+1));
        Sum2 += ARowValue * (*(BColumnStart+2));
        Sum3 += ARowValue * (*(BColumnStart+3));
        Sum4 += ARowValue * (*(BColumnStart+4));
        Sum5 += ARowValue * (*(BColumnStart+5));
        Sum6 += ARowValue * (*(BColumnStart+6));
        Sum7 += ARowValue * (*(BColumnStart+7));    
      }
      ARowStart = (REAL*) ( ((PTR) ARowStart) - MatrixWidthInBytes);

      *(C) = Sum0;
      *(C+1) = Sum1;
      *(C+2) = Sum2;
      *(C+3) = Sum3;
      *(C+4) = Sum4;
      *(C+5) = Sum5;
      *(C+6) = Sum6;
      *(C+7) = Sum7;
      C+=8;
    }

    ARowStart = (REAL*) ( ((PTR) ARowStart) + RowWidthAInBytes );
    C = (REAL*) ( ((PTR) C) + RowIncrementC );
  }

}


/*****************************************************************************
 **
 ** FastAdditiveNaiveMatrixMultiply
 **
 ** For small to medium sized matrices A, B, and C of size
 ** MatrixSize * MatrixSize this function performs the operation
 ** C += A x B efficiently.
 **
 ** Note MatrixSize must be divisible by 8.
 **
 ** INPUT:
 **    C = (*C READ/WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **
 ** OUTPUT:
 **    C = (*C READ/WRITE) Matrix C contains C + A x B.
 **
 *****************************************************************************/
void FastAdditiveNaiveMatrixMultiply(REAL *C, const REAL *A, const REAL *B,
    unsigned MatrixSize, unsigned RowWidthC,
    unsigned RowWidthA, unsigned RowWidthB) { 

  /* Assumes size of real is 8 bytes */
  PTR RowWidthBInBytes = RowWidthB  << 3;
  PTR RowWidthAInBytes = RowWidthA << 3;
  PTR MatrixWidthInBytes = MatrixSize << 3;
  PTR RowIncrementC = ( RowWidthC - MatrixSize) << 3;
  unsigned Horizontal, Vertical;

  const REAL *ARowStart = A;
  for (Vertical = 0; Vertical < MatrixSize; Vertical++) {
    for (Horizontal = 0; Horizontal < MatrixSize; Horizontal += 8) {
      const REAL *BColumnStart = B + Horizontal;

      REAL Sum0 = *C;
      REAL Sum1 = *(C+1);
      REAL Sum2 = *(C+2);
      REAL Sum3 = *(C+3);
      REAL Sum4 = *(C+4);
      REAL Sum5 = *(C+5);
      REAL Sum6 = *(C+6);
      REAL Sum7 = *(C+7);   

      unsigned Products;
      for (Products = 0; Products < MatrixSize; Products++) {
        REAL ARowValue = *ARowStart++;

        Sum0 += ARowValue * (*BColumnStart);
        Sum1 += ARowValue * (*(BColumnStart+1));
        Sum2 += ARowValue * (*(BColumnStart+2));
        Sum3 += ARowValue * (*(BColumnStart+3));
        Sum4 += ARowValue * (*(BColumnStart+4));
        Sum5 += ARowValue * (*(BColumnStart+5));
        Sum6 += ARowValue * (*(BColumnStart+6));
        Sum7 += ARowValue * (*(BColumnStart+7));

        BColumnStart = (REAL*) (((PTR) BColumnStart) + RowWidthBInBytes);

      }
      ARowStart = (REAL*) ( ((PTR) ARowStart) - MatrixWidthInBytes);

      *(C) = Sum0;
      *(C+1) = Sum1;
      *(C+2) = Sum2;
      *(C+3) = Sum3;
      *(C+4) = Sum4;
      *(C+5) = Sum5;
      *(C+6) = Sum6;
      *(C+7) = Sum7;
      C+=8;
    }

    ARowStart = (REAL*) ( ((PTR) ARowStart) + RowWidthAInBytes );
    C = (REAL*) ( ((PTR) C) + RowIncrementC );
  }
}


/*****************************************************************************
 **
 ** MultiplyByDivideAndConquer
 **
 ** For medium to medium-large (would you like fries with that) sized
 ** matrices A, B, and C of size MatrixSize * MatrixSize this function
 ** efficiently performs the operation
 **    C  = A x B (if AdditiveMode == 0)
 **    C += A x B (if AdditiveMode != 0)
 **
 ** Note MatrixSize must be divisible by 16.
 **
 ** INPUT:
 **    C = (*C READ/WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **    AdditiveMode = 0 if we want C = A x B, otherwise we'll do C += A x B
 **
 ** OUTPUT:
 **    C (+)= A x B. (+ if AdditiveMode != 0)
 **
 *****************************************************************************/
void MultiplyByDivideAndConquer(REAL *C, const REAL *A, const REAL *B,
    unsigned MatrixSize, unsigned RowWidthC,
    unsigned RowWidthA, unsigned RowWidthB,
    int AdditiveMode) {

#define A00 A
#define B00 B
#define C00 C

  const REAL  *A01, *A10, *A11, *B01, *B10, *B11;
  REAL *C01, *C10, *C11;
  unsigned QuadrantSize = MatrixSize >> 1;

  /* partition the matrix */
  A01 = A00 + QuadrantSize;
  A10 = A00 + RowWidthA * QuadrantSize;
  A11 = A10 + QuadrantSize;

  B01 = B00 + QuadrantSize;
  B10 = B00 + RowWidthB * QuadrantSize;
  B11 = B10 + QuadrantSize;

  C01 = C00 + QuadrantSize;
  C10 = C00 + RowWidthC * QuadrantSize;
  C11 = C10 + QuadrantSize;

  if (QuadrantSize > SizeAtWhichNaiveAlgorithmIsMoreEfficient) {

    MultiplyByDivideAndConquer(C00, A00, B00, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C01, A00, B01, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C11, A10, B01, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C10, A10, B00, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C00, A01, B10, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, 1);

    MultiplyByDivideAndConquer(C01, A01, B11, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, 1);

    MultiplyByDivideAndConquer(C11, A11, B11, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, 1);

    MultiplyByDivideAndConquer(C10, A11, B10, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB, 1);

  } else {

    if (AdditiveMode) {
      FastAdditiveNaiveMatrixMultiply(C00, A00, B00, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);

      FastAdditiveNaiveMatrixMultiply(C01, A00, B01, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);

      FastAdditiveNaiveMatrixMultiply(C11, A10, B01, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);

      FastAdditiveNaiveMatrixMultiply(C10, A10, B00, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);

    } else {

      FastNaiveMatrixMultiply(C00, A00, B00, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);

      FastNaiveMatrixMultiply(C01, A00, B01, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);

      FastNaiveMatrixMultiply(C11, A10, B01, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);

      FastNaiveMatrixMultiply(C10, A10, B00, QuadrantSize,
          RowWidthC, RowWidthA, RowWidthB);
    }

    FastAdditiveNaiveMatrixMultiply(C00, A01, B10, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB);

    FastAdditiveNaiveMatrixMultiply(C01, A01, B11, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB);

    FastAdditiveNaiveMatrixMultiply(C11, A11, B11, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB);

    FastAdditiveNaiveMatrixMultiply(C10, A11, B10, QuadrantSize,
        RowWidthC, RowWidthA, RowWidthB);
  }

  return;
}


/*****************************************************************************
 **
 ** OptimizedStrassenMultiply
 **
 ** For large matrices A, B, and C of size MatrixSize * MatrixSize this
 ** function performs the operation C = A x B efficiently.
 **
 ** INPUT:
 **    C = (*C WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **
 ** OUTPUT:
 **    C = (*C WRITE) Matrix C contains A x B. (Initial value of *C undefined.)
 **
 *****************************************************************************/

typedef struct s1loopctx {
  REAL *S1;
  int QuadrantSize;
  int RowWidthA;
  const REAL *A21;
  const REAL *A22; 
} s1loopctx;

void __attribute__((noinline)) S1LoopBody(void *context, uint32_t start, uint32_t end) {
  s1loopctx *ctx = (s1loopctx*) context;
  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S1[Row * ctx->QuadrantSize + Column] = ctx->A21[ctx->RowWidthA * Row + Column] + ctx->A22[ctx->RowWidthA * Row + Column];
    }
  }
}

void S1Loop(REAL *const S1, const int QuadrantSize, const int RowWidthA, const REAL *const A21, const REAL *const A22) {
  s1loopctx ctx = {
    .S1 = S1,
    .QuadrantSize = QuadrantSize,
    .RowWidthA = RowWidthA,
    .A21 = A21,
    .A22 = A22
  };
  __cilkrts_cilk_for_32(S1LoopBody, &ctx, QuadrantSize, 0);
}

typedef struct s2loopctx {
  REAL *S2;
  int QuadrantSize;
  int RowWidthA;
  const REAL *S1;
  const REAL *A;
} s2loopctx;

void __attribute__((noinline)) S2LoopBody(void *context, uint32_t start, uint32_t end) {
  s2loopctx *ctx = (s2loopctx*) context;
  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S2[Row * ctx->QuadrantSize + Column] = ctx->S1[Row * ctx->QuadrantSize + Column] - ctx->A[ctx->RowWidthA * Row + Column];
    }
  }
}

void S2Loop(REAL *const S2, const int QuadrantSize, const int RowWidthA, const REAL *const S1, const REAL *const A) {
  s2loopctx ctx = {
    .S2 = S2,
    .QuadrantSize = QuadrantSize,
    .RowWidthA = RowWidthA,
    .S1 = S1,
    .A = A
  };
  __cilkrts_cilk_for_32(S2LoopBody, &ctx, QuadrantSize, 0);
}

typedef struct s3loopctx {
  REAL *S3;
  int QuadrantSize;
  int RowWidthA;
  const REAL *A;
  const REAL *A21;
} s3loopctx;

void __attribute__((noinline)) S3LoopBody(void *context, uint32_t start, uint32_t end) {
  s3loopctx* ctx = (s3loopctx*) context;
  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S3[Row * ctx->QuadrantSize + Column] = ctx->A[ctx->RowWidthA * Row + Column] - ctx->A21[ctx->RowWidthA * Row + Column];
    }
  }
}

void __attribute__((noinline)) S3Loop(REAL *const S3, const int QuadrantSize, const int RowWidthA, const REAL *const A, const REAL *const A21) {
  s3loopctx ctx = {
    .S3 = S3,
    .QuadrantSize = QuadrantSize,
    .RowWidthA = RowWidthA,
    .A = A,
    .A21 = A21
  };
  __cilkrts_cilk_for_32(S3LoopBody, &ctx, QuadrantSize, 0);
}

void __attribute__((noinline)) S3LoopHelper(REAL *const S3, const int QuadrantSize, const int RowWidthA, const REAL *const A, const REAL *const A21) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_fast_1(&sf);
  __cilkrts_detach(&sf);

  S3Loop(S3, QuadrantSize, RowWidthA, A, A21);

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

typedef struct s4loopctx {
  REAL *S4;
  int QuadrantSize;
  int RowWidthA;
  const REAL *A12;
  const REAL *S2;
} s4loopctx;

void __attribute__((noinline)) S4LoopBody(void *context, uint32_t start, uint32_t end) {
  s4loopctx *ctx = (s4loopctx*) context;

  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S4[Row * ctx->QuadrantSize + Column] = ctx->A12[Row * ctx->RowWidthA + Column] - ctx->S2[ctx->QuadrantSize * Row + Column];
    }
  }
}

void __attribute__((noinline)) S4Loop(REAL *const S4, const int QuadrantSize, const int RowWidthA, const REAL *const A12, const REAL *const S2) {
  s4loopctx ctx = {
    .S4 = S4,
    .QuadrantSize = QuadrantSize,
    .RowWidthA = RowWidthA,
    .A12 = A12,
    .S2 = S2
  };

  __cilkrts_cilk_for_32(S4LoopBody, &ctx, QuadrantSize, 0);
}

void __attribute__((noinline)) S4LoopHelper(REAL *const S4, const int QuadrantSize, const int RowWidthA, const REAL *const A12, const REAL *const S2) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_fast_1(&sf);
  __cilkrts_detach(&sf);

  S4Loop(S4, QuadrantSize, RowWidthA, A12, S2);

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

typedef struct s5loopctx {
  REAL *S5;
  int QuadrantSize;
  int RowWidthB;
  const REAL *B12;
  const REAL *B;
} s5loopctx;

void __attribute__((noinline)) S5LoopBody(void *context, uint32_t start, uint32_t end) {
  s5loopctx *ctx = (s5loopctx*)context;

  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S5[Row * ctx->QuadrantSize + Column] = ctx->B12[Row * ctx->RowWidthB + Column] - ctx->B[Row * ctx->RowWidthB + Column];
    }
  }

}

void __attribute__((noinline)) S5Loop(REAL *const S5, const int QuadrantSize, const int RowWidthB, const REAL *const B12, const REAL *const B) {
  s5loopctx ctx = {
    .S5 = S5,
    .QuadrantSize = QuadrantSize,
    .RowWidthB = RowWidthB,
    .B12 = B12,
    .B = B
  };

  __cilkrts_cilk_for_32(S5LoopBody, &ctx, QuadrantSize, 0);
}

typedef struct s6loopctx {
  REAL *S6;
  int QuadrantSize;
  int RowWidthB;
  const REAL *B22;
  const REAL *S5;
} s6loopctx;

void __attribute__((noinline)) S6LoopBody(void *context, uint32_t start, uint32_t end) {
  s6loopctx *ctx = (s6loopctx*) context;
  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S6[Row * ctx->QuadrantSize + Column] = ctx->B22[Row * ctx->RowWidthB + Column] - ctx->S5[Row * ctx->QuadrantSize + Column];
    }
  }
}

void __attribute__((noinline)) S6Loop(REAL *const S6, const int QuadrantSize, const int RowWidthB, const REAL *const B22, const REAL *const S5) {
  s6loopctx ctx = {
    .S6 = S6,
    .QuadrantSize = QuadrantSize,
    .RowWidthB = RowWidthB,
    .B22 = B22,
    .S5 = S5
  };
  __cilkrts_cilk_for_32(S6LoopBody, &ctx, QuadrantSize, 0);
}

typedef struct s7loopctx {
  REAL *S7;
  int QuadrantSize;
  int RowWidthB;
  const REAL *B22;
  const REAL *B12;
} s7loopctx;

void __attribute__((noinline)) S7LoopBody(void *context, uint32_t start, uint32_t end) {
  s7loopctx *ctx = (s7loopctx*)context;

  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S7[Row * ctx->QuadrantSize + Column] = ctx->B22[Row * ctx->RowWidthB + Column] - ctx->B12[Row * ctx->RowWidthB + Column];
    }
  }
  
}

void __attribute__((noinline)) S7Loop(REAL *const S7, const int QuadrantSize, const int RowWidthB, const REAL *const B22, const REAL *const B12) {
  s7loopctx ctx = {
    .S7 = S7,
    .QuadrantSize = QuadrantSize,
    .RowWidthB = RowWidthB,
    .B22 = B22,
    .B12 = B12
  };

  __cilkrts_cilk_for_32(S7LoopBody, &ctx, QuadrantSize, 0);
}

typedef struct s8loopctx {
  REAL *S8;
  int QuadrantSize;
  int RowWidthB;
  const REAL *S6;
  const REAL *B21;
} s8loopctx;

void __attribute__((noinline)) S8LoopBody(void *context, uint32_t start, uint32_t end) {
  s8loopctx *ctx = (s8loopctx*)context;

  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->S8[Row * ctx->QuadrantSize + Column] = ctx->S6[Row * ctx->QuadrantSize + Column] - ctx->B21[Row * ctx->RowWidthB + Column];
    }
  }
}

void __attribute__((noinline)) S8Loop(REAL *const S8, const int QuadrantSize, const int RowWidthB, const REAL *const S6, const REAL *const B21) {
  s8loopctx ctx = {
    .S8 = S8,
    .QuadrantSize = QuadrantSize,
    .RowWidthB = RowWidthB,
    .S6 = S6,
    .B21 = B21
  };

  __cilkrts_cilk_for_32(S8LoopBody, &ctx, QuadrantSize, 0);
}

void __attribute__((noinline)) S8LoopHelper(REAL *const S8, const int QuadrantSize, const int RowWidthB, const REAL *const S6, const REAL *const B21) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_fast_1(&sf);
  __cilkrts_detach(&sf);
  
  S8Loop(S8, QuadrantSize, RowWidthB, S6, B21);

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

typedef struct cloopctx {
  REAL *C;
  int QuadrantSize;
  int RowWidthC;
  const REAL *M2;
} cloopctx;

void __attribute__((noinline)) CLoopBody(void *context, uint32_t start, uint32_t end) {
  cloopctx *ctx = (cloopctx*)context;

  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->C[ctx->RowWidthC * Row + Column] += ctx->M2[Row * ctx->QuadrantSize + Column];
    }
  }
}

void __attribute__((noinline)) CLoop(REAL *const C, const int QuadrantSize, const int RowWidthC, const REAL *const M2) {
  cloopctx ctx = {
    .C = C,
    .QuadrantSize = QuadrantSize,
    .RowWidthC = RowWidthC,
    .M2 = M2
  };
  __cilkrts_cilk_for_32(CLoopBody, &ctx, QuadrantSize, 0);
}

void __attribute__((noinline)) CLoopHelper(REAL *const C, const int QuadrantSize, const int RowWidthC, const REAL *const M2) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_fast_1(&sf);
  __cilkrts_detach(&sf);

  CLoop(C, QuadrantSize, RowWidthC, M2);

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

typedef struct c12loopctx {
  REAL *C12;
  int QuadrantSize;
  int RowWidthC;
  const REAL *M5;
  const REAL *T1sMULT;
  const REAL *M2;
} c12loopctx;

void __attribute__((noinline)) C12LoopBody(void *context, uint32_t start, uint32_t end) {
  c12loopctx *ctx = (c12loopctx*) context;

  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->C12[ctx->RowWidthC * Row + Column] += ctx->M5[Row * ctx->QuadrantSize + Column] + ctx->T1sMULT[Row * ctx->QuadrantSize + Column] + ctx->M2[Row * ctx->QuadrantSize + Column];
    }
  }
}

void __attribute__((noinline)) C12Loop(REAL *const C12, const int QuadrantSize, const int RowWidthC, const REAL *const M5, const REAL *const T1sMULT, const REAL *const M2) {
  c12loopctx ctx = {
    .C12 = C12,
    .QuadrantSize = QuadrantSize,
    .RowWidthC = RowWidthC,
    .M5 = M5,
    .T1sMULT = T1sMULT,
    .M2 = M2
  };

  __cilkrts_cilk_for_32(C12LoopBody, &ctx, QuadrantSize, 0);
}

void __attribute__((noinline)) C12LoopHelper(REAL *const C12, const int QuadrantSize, const int RowWidthC, const REAL *const M5, const REAL *const T1sMULT, const REAL *const M2) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_fast_1(&sf);
  __cilkrts_detach(&sf);

  C12Loop(C12, QuadrantSize, RowWidthC, M5, T1sMULT, M2);

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

typedef struct c21loopctx {
  REAL *C21;
  int QuadrantSize;
  int RowWidthC;
  const REAL *C22;
  const REAL *T1sMULT;
  const REAL *M2;
} c21loopctx;

void __attribute__((noinline)) C21LoopBody(void *context, uint32_t start, uint32_t end) {
  c21loopctx *ctx = (c21loopctx*)context;
  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
      ctx->C21[ctx->RowWidthC * Row + Column] = -ctx->C21[ctx->RowWidthC * Row + Column] + ctx->C22[ctx->RowWidthC * Row + Column] + ctx->T1sMULT[Row * ctx->QuadrantSize + Column] + ctx->M2[Row * ctx->QuadrantSize + Column];
    }
  }
}

void __attribute__((noinline)) C21Loop(REAL *const C21, const int QuadrantSize, const int RowWidthC, const REAL *const C22, const REAL *const T1sMULT, const REAL *const M2) {
  c21loopctx ctx = {
    .C21 = C21,
    .QuadrantSize = QuadrantSize,
    .RowWidthC = RowWidthC,
    .C22 = C22,
    .T1sMULT = T1sMULT,
    .M2 = M2
  };

  __cilkrts_cilk_for_32(C21LoopBody, &ctx, QuadrantSize, 0);
}

typedef struct c22loopctx {
  REAL *C22;
  int QuadrantSize;
  int RowWidthC;
  const REAL *M5;
  const REAL *T1sMULT;
  const REAL *M2;
} c22loopctx;

void __attribute__((noinline)) C22LoopBody(void *context, uint32_t start, uint32_t end) {
  c22loopctx *ctx = (c22loopctx*)context;
  for (int Row = start; Row < end; Row++) {
    for (int Column = 0; Column < ctx->QuadrantSize; Column++) {
        ctx->C22[ctx->RowWidthC * Row + Column] += ctx->M5[Row * ctx->QuadrantSize + Column] + ctx->T1sMULT[Row * ctx->QuadrantSize + Column] + ctx->M2[Row * ctx->QuadrantSize + Column];
    }
  }
}

void __attribute__((noinline)) C22Loop(REAL *const C22, const int QuadrantSize, const int RowWidthC, const REAL *const M5, const REAL *const T1sMULT, const REAL *const M2) {
  c22loopctx ctx = {
    .C22 = C22,
    .QuadrantSize = QuadrantSize,
    .RowWidthC = RowWidthC,
    .M5 = M5,
    .T1sMULT = T1sMULT,
    .M2 = M2
  };

  __cilkrts_cilk_for_32(C22LoopBody, &ctx, QuadrantSize, 0);
}
void OptimizedStrassenMultiply(REAL *C, const REAL *A, const REAL *B,
    unsigned MatrixSize, unsigned RowWidthC,
    unsigned RowWidthA, unsigned RowWidthB);

void __attribute__((noinline)) OptimizedStrassenMultiplyHelper(REAL *C, const REAL *A, const REAL *B,
    unsigned MatrixSize, unsigned RowWidthC,
    unsigned RowWidthA, unsigned RowWidthB) {

  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_fast_1(&sf);
  __cilkrts_detach(&sf);
  
  OptimizedStrassenMultiply(C, A, B, MatrixSize, RowWidthC, RowWidthA, RowWidthB);

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}
#define strassen(n,A,an,B,bn,C,cn) OptimizedStrassenMultiply(C,A,B,n,cn,bn,an)
void OptimizedStrassenMultiply(REAL *C, const REAL *A, const REAL *B,
    unsigned MatrixSize, unsigned RowWidthC,
    unsigned RowWidthA, unsigned RowWidthB) {

  unsigned QuadrantSize = MatrixSize >> 1; /* MatixSize / 2 */
  unsigned QuadrantSizeInBytes = 
    sizeof(REAL) * QuadrantSize * QuadrantSize;

  /************************************************************************
   ** For each matrix A, B, and C, we'll want pointers to each quandrant
   ** in the matrix. These quandrants will be addressed as follows:
   **  --        --
   **  | A11  A12 |
   **  |          |
   **  | A21  A22 |
   **  --        --
   ************************************************************************/
  const REAL /* *A11, *B11, *C11, */ *A12, *B12,
       *A21, *B21, *A22, *B22;
  REAL *C12, *C21, *C22;

  REAL *S1,*S2,*S3,*S4,*S5,*S6,*S7,*S8,*M2,*M5,*T1sMULT;
#define T2sMULT C22
#define NumberOfVariables 11

  char *Heap;
  void *StartHeap;

  if (MatrixSize <= SizeAtWhichDivideAndConquerIsMoreEfficient) {
    MultiplyByDivideAndConquer(C, A, B,
        MatrixSize, RowWidthC, RowWidthA, RowWidthB, 0);

    return;
  }

  __asm__ volatile ("" ::: "memory");
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_1(&sf);

  /* Initialize quandrant matrices */
  A12 = A + QuadrantSize;
  B12 = B + QuadrantSize;
  C12 = C + QuadrantSize;
  A21 = A + (RowWidthA * QuadrantSize);
  B21 = B + (RowWidthB * QuadrantSize);
  C21 = C + (RowWidthC * QuadrantSize);
  A22 = A21 + QuadrantSize;
  B22 = B21 + QuadrantSize;
  C22 = C21 + QuadrantSize;

  /* Allocate Heap Space Here */
  StartHeap = malloc(QuadrantSizeInBytes * NumberOfVariables + 32);
  Heap = (char*)StartHeap;
  /* ensure that heap is on cache boundary */
  if ( ((PTR) Heap) & 31 )
    Heap = (char*) ( ((PTR) Heap) + 32 - ( ((PTR) Heap) & 31) );

  /* Distribute the heap space over the variables */
  S1 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  S2 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  S3 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  S4 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  S5 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  S6 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  S7 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  S8 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  M2 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  M5 = (REAL*) Heap; Heap += QuadrantSizeInBytes;
  T1sMULT = (REAL*) Heap; Heap += QuadrantSizeInBytes;


  S1Loop(S1, QuadrantSize, RowWidthA, A21, A22);
  //cilk_sync;

  S2Loop(S2, QuadrantSize, RowWidthA, S1, A);

  //cilk_sync;

  if (!CILK_SETJMP(sf.ctx)) {
      S4LoopHelper(S4, QuadrantSize, RowWidthA, A12, S2);
  }

  S5Loop(S5, QuadrantSize, RowWidthB, B12, B);

  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_sync(&sf);
    }
  }

  S6Loop(S6, QuadrantSize, RowWidthB, B22, S5);

  if (!CILK_SETJMP(sf.ctx)) {
    S8LoopHelper(S8, QuadrantSize, RowWidthB, S6, B21);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    S3LoopHelper(S3, QuadrantSize, RowWidthA, A, A21);
  }

  S7Loop(S7, QuadrantSize, RowWidthB, B22, B12);

  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_sync(&sf);
    }
  }

  if (!CILK_SETJMP(sf.ctx)) {
    /* M2 = A11 x B11 */
    OptimizedStrassenMultiplyHelper(M2, A, B, QuadrantSize,
                                    QuadrantSize, RowWidthA, RowWidthB);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    /* M5 = S1 * S5 */
    OptimizedStrassenMultiplyHelper(M5, S1, S5, QuadrantSize,
                                    QuadrantSize, QuadrantSize, 
                                    QuadrantSize);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    /* Step 1 of T1 = S2 x S6 + M2 */
    OptimizedStrassenMultiplyHelper(T1sMULT, S2, S6,  QuadrantSize,
                                    QuadrantSize, QuadrantSize, 
                                    QuadrantSize);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    /* Step 1 of T2 = T1 + S3 x S7 */
    OptimizedStrassenMultiplyHelper(C22, S3, S7, QuadrantSize,
                              RowWidthC /*FIXME*/, QuadrantSize, 
                              QuadrantSize);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    /* Step 1 of C11 = M2 + A12 * B21 */
    OptimizedStrassenMultiplyHelper(C, A12, B21, QuadrantSize,
                                    RowWidthC, RowWidthA, RowWidthB);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    /* Step 1 of C12 = S4 x B22 + T1 + M5 */
    OptimizedStrassenMultiplyHelper(C12, S4, B22, QuadrantSize,
                                    RowWidthC, QuadrantSize, RowWidthB);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    /* Step 1 of C21 = T2 - A22 * S8 */
    OptimizedStrassenMultiplyHelper(C21, A22, S8, QuadrantSize,
                                    RowWidthC, RowWidthA, QuadrantSize);
  }

  /**********************************************
   ** Synchronization Point
   **********************************************/
  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_sync(&sf);
    }
  }

  if (!CILK_SETJMP(sf.ctx)) {
    CLoopHelper(C, QuadrantSize, RowWidthC, M2);
  }

  if (!CILK_SETJMP(sf.ctx)) {
    C12LoopHelper(C12, QuadrantSize, RowWidthC, M5, T1sMULT, M2);
  }

  C21Loop(C21, QuadrantSize, RowWidthC, C22, T1sMULT, M2);

  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_sync(&sf);
    }
  }

  C22Loop(C22, QuadrantSize, RowWidthC, M5, T1sMULT, M2);

  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_sync(&sf);
    }
  }

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);

  free(StartHeap);
  return;
}

/*
 * Set an size n vector V to random values. 
 */
void init_vec(int n, REAL *V) {
  int i;

  for(i=0; i < n; i++) {
    V[i] = ((double) cilk_rand()) / (double) RAND_MAX; 
  }
}

/*
 * Compare two matrices.  Return -1 if they differ more EPSILON.
 */
int compare_vec(int n, REAL *V1, REAL *V2) {
  int i;
  REAL c, sum = 0.0;

  for(i = 0; i < n; ++i) {
    c = V1[i] - V2[i];
    if( c < 0.0 ) {
      c = -c;
    }
    sum += c;
    // ANGE: this is used in compare_matrix
    // c = c / V1[i];
    if( c > EPSILON ) {
      return -1;
    }
  }

  printf("Sum of errors: %g\n", sum);
  return 0;
}

/*
 * Allocate a vector of size n 
 */
REAL *alloc_vec(int n) {

  return (REAL *) malloc(n * sizeof(REAL));
}

/*
 * free a vector 
 */
void free_vec(REAL *V) {

  free(V);
}

/*
 * Set an n by n matrix A to random values.  The distance between
 * rows is an
 */
void init_matrix(int n, REAL *A, int an) {

  int i, j;

  for (i = 0; i < n; ++i)
    for (j = 0; j < n; ++j) 
      ELEM(A, an, i, j) = ((double) cilk_rand()) / (double) RAND_MAX; 
}

/*
 * Compare two matrices.  Print an error message if they differ by
 * more than EPSILON.
 */
int compare_matrix(int n, REAL *A, int an, REAL *B, int bn) {

  int i, j;
  REAL c;

  for (i = 0; i < n; ++i) {
    for (j = 0; j < n; ++j) {
      /* compute the relative error c */
      c = ELEM(A, an, i, j) - ELEM(B, bn, i, j);
      if (c < 0.0) 
        c = -c;

      c = c / ELEM(A, an, i, j);
      if (c > EPSILON) {
        return -1;
      }
    }
  }

  return 0;
}



/*
 * Allocate a matrix of side n (therefore n^2 elements)
 */
REAL *alloc_matrix(int n) {
  return (REAL *) malloc(n * n * sizeof(REAL));
}

/*
 * free a matrix (Never used because Matteo expects
 *                the OS to clean up his garbage. Tsk. Tsk.)
 */
void free_matrix(REAL *A) {
  free(A);
}


/*
 * simple test program
 */
int usage(void) {
  fprintf(stderr, 
      "\nUsage: strassen [<cilk-options>] [-n #] [-c] [-rc]\n\n"
      "Multiplies two randomly generated n x n matrices. To check for\n"
      "correctness use -c using iterative matrix multiply or use -rc \n"
      "using randomized algorithm due to Freivalds.\n\n");

  return 1;
}

const char *specifiers[] = {"-n", "-c", "-rc", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, BENCHMARK, BOOLARG, 0};

int main(int argc, char *argv[]) {

  REAL *A, *B, *C;
  int verify, rand_check, benchmark, help, n;

  /* standard benchmark options*/
  n = 512;  
  verify = 0;  
  rand_check = 0;

  get_options(argc, argv, specifiers, opt_types, 
              &n, &verify, &rand_check, &benchmark, &help);

  if (help) return usage();

  if (benchmark) {
    switch (benchmark) {
      case 1:      /* short benchmark options -- a little work*/
        n = 512;
        break;
      case 2:      /* standard benchmark options*/
        n = 2048;
        break;
      case 3:      /* long benchmark options -- a lot of work*/
        n = 4096;
        break;
    }
  }

  if((n & (n - 1)) != 0 || (n % 16) != 0) {
    printf("%d: matrix size must be a power of 2"
           " and a multiple of %d\n", n, 16);
    return 1;
  }
  __cilkrts_init();

  A = alloc_matrix(n);
  B = alloc_matrix(n);
  C = alloc_matrix(n);

  init_matrix(n, A, n);
  init_matrix(n, B, n); 

#if TIMING_COUNT
  clockmark_t begin, end;
  uint64_t elapsed[TIMING_COUNT];

  __cilkrts_reset_timing();
  for(int i=0; i < TIMING_COUNT; i++) {
    begin = ktiming_getmark();
    strassen(n, A, n, B, n, C, n);
    end = ktiming_getmark();
    elapsed[i] = ktiming_diff_usec(&begin, &end);
  }
  //for (int i = 0; i < 10000; i++)
  //for (int i = 0; i < 10000; i++) {
  //  printf("%d\n", i);
  //}
  print_runtime(elapsed, TIMING_COUNT);
#else
  init_matrix(n, A, n);
  init_matrix(n, B, n);
  strassen(n, A, n, B, n, C, n);
#endif 
  __cilkrts_accum_timing();
  
  if(rand_check) {
    REAL *R, *V1, *V2;
    R = alloc_vec(n);
    V1 = alloc_vec(n);
    V2 = alloc_vec(n);

    mat_vec_mul(n, n, n, B, R, V1, 0);
    mat_vec_mul(n, n, n, A, V1, V2, 0);
    mat_vec_mul(n, n, n, C, R, V1, 0);
    rand_check = compare_vec(n, V1, V2);

    free_vec(R);
    free_vec(V1);
    free_vec(V2);

  } else if (verify) {
    printf("Checking results ... \n");
    REAL *C2 = alloc_matrix(n);
    matrixmul(n, A, n, B, n, C2, n);
    verify = compare_matrix(n, C, n, C2, n);
    free_matrix(C2);
  } 

  if(rand_check || verify) {
    printf("WRONG RESULT!\n");

  } else {
    printf("\nCilk Example: strassen\n");
    printf("Options: n = %d\n\n", n);
  }

  free_matrix(A);
  free_matrix(B);
  free_matrix(C);

  return 0;
}
