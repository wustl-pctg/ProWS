#ifndef SERIAL
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#else
#define cilk_spawn 
#define cilk_sync
#define __cilkrts_accum_timing()
#endif

#define __cilkrts_accum_timing()

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "getoptions.h"

#ifndef TIMING_COUNT
#define TIMING_COUNT 10
#endif

#if TIMING_COUNT
#include "ktiming.h"
#endif

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

#define ERR_THRESHOLD   (0.1)

#define REAL int
static int BASE_CASE; //the base case of the computation (2*POWER)
static int POWER; //the power of two the base case is based on
#define timing


static const unsigned int Q[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF};
static const unsigned int S[] = {1, 2, 4, 8};

unsigned long rand_nxt = 0;

int cilk_rand(void) {
    int result;
    rand_nxt = rand_nxt * 1103515245 + 12345;
    result = (rand_nxt >> 16) % ((unsigned int) RAND_MAX + 1);
    return result;
}


//provides a look up for the Morton Number of the z-order curve given the x and y coordinate
//every instance of an (x,y) lookup must use this function
unsigned int z_convert(int row, int col){

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

//converts (x,y) position in the array to the mixed z-order row major layout
int block_convert(int row, int col){
    int block_index = z_convert(row >> POWER, col >> POWER);
    return (block_index * BASE_CASE << POWER) 
        + ((row - ((row >> POWER) << POWER)) << POWER) 
        + (col - ((col >> POWER) << POWER));
}

//init the matric in order
void order(REAL *M, int n){
    int i,j;
    for(i = 0; i < n; i++) {
        for(j = 0; j < n; j++){
            M[block_convert(i,j)] = i * n + j;   
        }
    }
}

//init the matric in order
void order_rm(REAL *M, int n){
    int i;
    for(i = 0; i < n * n; i++) {
        M[i] = i;
    }
}

//init the matrix to all ones 
void one(REAL *M, int n){
    int i;
    for(i = 0; i < n * n; i++) {
        M[i] = 1.0;
    }
}


//init the matrix to all zeros 
void zero(REAL *M, int n){
    int i;
    for(i = 0; i < n * n; i++) {
        M[i] = 0.0;
    }
}

//init the matrix to random numbers
void init(REAL *M, int n){
    int i,j;
    for(i = 0; i < n; i++) {
        for(j = 0; j < n; j++){
            M[block_convert(i,j)] = (REAL) cilk_rand();
        }
    }
}

//init the matrix to random numbers
void init_rm(REAL *M, int n){
    int i;
    for(i = 0; i < n * n; i++) {
        M[i] = (REAL) cilk_rand();
    }
}

//prints the matrix
void print_matrix(REAL *M, int n){
    int i,j;
    for(i = 0; i < n; i++){
        for(j = 0; j < n; j++){
            printf("%6d ", M[block_convert(i,j)]);
        }
        printf("\n");
    }
}


//prints the matrix
void print_matrix_rm(REAL *M, int n, int orig_n){
    int i,j;
    for(i = 0; i < n; i++){
        for(j = 0; j < n; j++){
            printf("%6d ", M[i * orig_n + j]);
        }
        printf("\n");
    }
}

//itterative solution for matrix multiplication
void iter_matmul(REAL *A, REAL *B, REAL *C, int n){
    int i, j, k;

    for(i = 0; i < n; i++){
        for(k = 0; k < n; k++){
            REAL c = 0.0;
            for(j = 0; j < n; j++){
                c += A[block_convert(i,j)] * B[block_convert(j,k)];
            }
            C[block_convert(i, k)] = c;
        }
    }
}

//itterative solution for matrix multiplication
void iter_matmul_rm(REAL *A, REAL *B, REAL *C, int n){
    int i, j, k;

    for(i = 0; i < n; i++){
        for(k = 0; k < n; k++){
            REAL c = 0.0;
            for(j = 0; j < n; j++){
                c += A[i * n + j] * B[j * n + k];
            }
            C[i * n + k] = c;
        }
    }
}

//calculates the max error between the itterative solution and other solution
double maxerror(REAL *M1, REAL *M2, int n) {
    int i,j;
    double err = 0.0;

    for(i = 0; i < n; i++){
        for(j = 0; j < n; j++){
            double diff = (M1[block_convert(i,j)] - M2[block_convert(i,j)]) / M1[block_convert(i,j)];
            if(diff < 0) {
                diff = -diff;
            }
            if(diff > err) {
                err = diff;
            }
        }
    }

    return err;
}

//calculates the max error between the itterative solution and other solution
double maxerror_rm(REAL *M1, REAL *M2, int n) {
    int i,j;
    double err = 0.0;

    for(i = 0; i < n; i++){
        for(j = 0; j < n; j++){
            double diff = (M1[i * n + j] - M2[i * n + j]) / M1[i * n + j];
            if(diff < 0) {
                diff = -diff;
            }
            if(diff > err) {
                err = diff;
            }
        }
    }

    return err;
}

//recursive parallel solution to matrix multiplication
void mat_mul_par(const REAL *const A, const REAL *const B, REAL *C, int n){
    //BASE CASE: here computation is switched to itterative matrix multiplication
    //At the base case A, B, and C point to row order matrices of n x n
    if(n == BASE_CASE) {
        int i, j, k;
        for(i = 0; i < n; i++){
            for(k = 0; k < n; k++){
                REAL c = 0.0;
                for(j = 0; j < n; j++){
                    c += A[i * n + j] * B[j* n + k];
                }
                C[i * n + k] += c;
            }
        }
        return;
    }

    //partition each matrix into 4 sub matrices
    //each sub-matrix points to the start of the z pattern
    const REAL *const A1 = &A[block_convert(0,0)];
    const REAL *const A2 = &A[block_convert(0, n >> 1)]; //bit shift to divide by 2
    const REAL *const A3 = &A[block_convert(n >> 1,0)];
    const REAL *const A4 = &A[block_convert(n >> 1, n >> 1)];

    const REAL *const B1 = &B[block_convert(0,0)];
    const REAL *const B2 = &B[block_convert(0, n >> 1)];
    const REAL *const B3 = &B[block_convert(n >> 1, 0)];
    const REAL *const B4 = &B[block_convert(n >> 1, n >> 1)];
    
    REAL *C1 = &C[block_convert(0,0)];
    REAL *C2 = &C[block_convert(0, n >> 1)];
    REAL *C3 = &C[block_convert(n >> 1,0)];
    REAL *C4 = &C[block_convert(n >> 1, n >> 1)];

    //recrusively call the sub-matrices for evaluation in parallel
    cilk_spawn mat_mul_par(A1, B1, C1, n >> 1);
    cilk_spawn mat_mul_par(A1, B2, C2, n >> 1);
    cilk_spawn mat_mul_par(A3, B1, C3, n >> 1);
    mat_mul_par(A3, B2, C4, n >> 1);
    cilk_sync; //wait here for first round to finish

    cilk_spawn mat_mul_par(A2, B3, C1, n >> 1);
    cilk_spawn mat_mul_par(A2, B4, C2, n >> 1);
    cilk_spawn mat_mul_par(A4, B3, C3, n >> 1);
    mat_mul_par(A4, B4, C4, n >> 1);
    cilk_sync; //wait here for all second round to finish
}

//recursive parallel solution to matrix multiplication - row major order
void mat_mul_par_rm(REAL *A, REAL *B, REAL *C, int n, int orig_n){
    //BASE CASE: here computation is switched to itterative matrix multiplication
    //At the base case A, B, and C point to row order matrices of n x n
    if(n == BASE_CASE) {
        int i, j, k;
        for(i = 0; i < n; i++){
            for(k = 0; k < n; k++){
                REAL c = 0.0;
                for(j = 0; j < n; j++){
                    c += A[i * orig_n + j] * B[j * orig_n + k];
                }
                C[i * orig_n + k] += c;
            }
        }
        return;
    }


    //partition each matrix into 4 sub matrices
    //each sub-matrix points to the start of the z pattern
    REAL *A1 = &A[0];
    REAL *A2 = &A[n >> 1]; //bit shift to divide by 2
    REAL *A3 = &A[(n * orig_n) >> 1];
    REAL *A4 = &A[((n * orig_n) + n) >> 1];

    REAL *B1 = &B[0];
    REAL *B2 = &B[n >> 1];
    REAL *B3 = &B[(n * orig_n) >> 1];
    REAL *B4 = &B[((n * orig_n) + n) >> 1];
    
    REAL *C1 = &C[0];
    REAL *C2 = &C[n >> 1];
    REAL *C3 = &C[(n * orig_n) >> 1];
    REAL *C4 = &C[((n * orig_n) + n) >> 1];

    //recursively call the sub-matrices for evaluation in parallel
    cilk_spawn mat_mul_par_rm(A1, B1, C1, n >> 1, orig_n);
    cilk_spawn mat_mul_par_rm(A1, B2, C2, n >> 1, orig_n);
    cilk_spawn mat_mul_par_rm(A3, B1, C3, n >> 1, orig_n);
    cilk_spawn mat_mul_par_rm(A3, B2, C4, n >> 1, orig_n);
    cilk_sync; //wait here for first round to finish

    cilk_spawn mat_mul_par_rm(A2, B3, C1, n >> 1, orig_n);
    cilk_spawn mat_mul_par_rm(A2, B4, C2, n >> 1, orig_n);
    cilk_spawn mat_mul_par_rm(A4, B3, C3, n >> 1, orig_n);
    cilk_spawn mat_mul_par_rm(A4, B4, C4, n >> 1, orig_n);
    cilk_sync; //wait here for all second round to finish

}

const char *specifiers[] = {"-n", "-c", "-rc", "-h", 0};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, BOOLARG, 0};

void check_result(REAL* A, REAL* B, REAL* C, int n) {
  REAL *ans = (REAL *) malloc(n*n*sizeof(REAL));

  iter_matmul(A, B, ans, n);
  double err = maxerror(C, ans, n);
  if (err >= ERR_THRESHOLD) printf("INCORRECT RESULT\n");
}

int main(int argc, char *argv[]) {
    int n = 2048; //default n value
    POWER = 5; //default k value
    BASE_CASE = (int) pow(2.0, (double) POWER);

    int check = 0, rand_check = 0, help = 0; // default options
    get_options(argc, argv, specifiers, opt_types, 
                      &n, &check, &rand_check, &help);

  if(help) {
      fprintf(stderr, 
                  "Usage: matmul [-n size] [-c] [-rc] [-h] [<cilk options>]\n");
      fprintf(stderr, "if -c is set, "
                  "check result against iterative matrix multiply O(n^3).\n");
      fprintf(stderr, "if -rc is set, check "
                  "result against randomlized algo. due to Freivalds O(n^2).\n");
      exit(1);
    }


    REAL *A, *B, *C;

    A = (REAL *) malloc(n * n * sizeof(REAL)); //source matrix 
    B = (REAL *) malloc(n * n * sizeof(REAL)); //source matrix
    C = (REAL *) malloc(n * n * sizeof(REAL)); //result matrix
   
#if TIMING_COUNT
  clockmark_t begin, end;
  uint64_t elapsed[TIMING_COUNT];

  for(int i=0; i < TIMING_COUNT; i++) {
    __cilkrts_set_param("local stacks", "256");
    init_rm(A, n);
    init_rm(B, n);
    zero(C, n);
    begin= ktiming_getmark();
    mat_mul_par(A, B, C, n); 
    end = ktiming_getmark();
    elapsed[i] = ktiming_diff_usec(&begin, &end);
  }
  print_runtime(elapsed, TIMING_COUNT);
#else
  init_rm(A, n);
  init_rm(B, n);
  zero(C, n);
  mat_mul_par(A, B, C, n); 
#endif
__cilkrts_accum_timing();

  if (check) check_result(A, B, C, n);

    //clean up memory
  delete [] A;
    delete [] B;
    delete [] C;
    //delete [] I;
  
    return 0;
}
