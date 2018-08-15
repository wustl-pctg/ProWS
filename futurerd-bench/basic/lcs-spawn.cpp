
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <cilk/cilk_api.h>

#ifndef TIMING_COUNT
#define TIMING_COUNT 5
#endif

#if TIMING_COUNT
#include "../util/ktiming.h"
#endif

#include "../util/getoptions.hpp"

/* Global variables  */

char* a;
char* b;
int n = 65536;

inline int max( int a, int b )
{
  if ( a < b )
    return b;
  else
    return a;
}

void LCSsectsolve( int** stor, int startX, int startY, int width ) {
  int i, j;
  for ( i = startX; i < startX + width; ++i) {
    for ( j = startY; j < startY + width; ++j) {
      if ( a[i-1] == b[j-1] ) {
        stor[i][j] = stor[i-1][j-1] + 1;
      } else {
        stor[i][j] = max( stor[i-1][j], stor[i][j-1] );
      }
    }
  }
}

long long parallelNEWdivLCS( int** stor, int startX, int startY, int width, int base ) {

  if(width <= base) {
    LCSsectsolve(stor, startX, startY, width);
  } else {
    parallelNEWdivLCS(stor, startX, startY, width/2, base);
    _Cilk_spawn parallelNEWdivLCS(stor, startX + width/2, startY, width/2, base);
    parallelNEWdivLCS(stor, startX, startY + width/2, width/2, base);
    _Cilk_sync;
    parallelNEWdivLCS(stor, startX + width/2, startY + width/2, width/2, base);
  }
  return stor[startX + width - 1][startY + width - 1 ];
}

int serialquadLCS( int n, char* a, char* b ) {
  int i,j;

  int** stor = (int**)malloc( (n+1) * sizeof(int*) );
  for ( i = 0; i < n + 1; ++i ) {
    stor[i] = (int*)malloc( (n+1) * sizeof(int) );
  }
  for ( i = 0; i < n + 1; ++i ) {
    stor[i][0] = 0;
  }
  for ( i = 0; i < n + 1; ++i ) {
    stor[0][i] = 0;
  }

  for ( i = 1; i < n + 1; ++i ) {
    for ( j = 1; j < n + 1; ++j ) {
      if(a[i-1] == b[j-1]) {
        stor[i][j] = stor[i-1][j-1] + 1;
      } else {
        stor[i][j] = max( stor[i-1][j], stor[i][j-1] );
      }
    }
  }

  int result = stor[n][n];

  for ( i = 0; i < n + 1; ++i) {
    free( stor[i] );
  }

  free( stor );

  return result;
}

void genRandString(char * s, int n, int alphabet) {
  int i;
  for ( i = 0; i < n; ++i ) {
    s[i] = (char)(rand() % alphabet + 97);
  }
}

const char* specifiers[] = {"-n", "-a", "-b", "-c", "-h", "-m", 0};
int   opt_types[]  = {INTARG, INTARG, INTARG, BOOLARG, BOOLARG, BOOLARG, 0};

void init_stor(int **stor, int n) {
  for (int i = 0; i < n + 1; ++i ) {
    stor[i] = (int*)malloc( (n+1) * sizeof(int) );
  }

  for ( int i = 0; i < n + 1; ++i ) {
    stor[i][0] = 0;
  }
  for ( int i = 0; i < n + 1; ++i ) {
    stor[0][i] = 0;
  }
}

int main(int argc, char *argv[]) {

  int alphabet = 4;
  int base = 128;
  int check = 0, help = 0, mute = 0;
  int i;

  get_options( argc, argv, specifiers, opt_types, &n, &alphabet, &base, &check, &help, &mute );

  if(help) {
    fprintf(stderr, "Usage: lcs [-n size] [-a alphabet size] ");
    fprintf(stderr, "[-b base size] [-c] [-h] [-m] [<cilk options>]\n");
    fprintf(stderr, "if -c is set,"
            "check result against serial lcs O(n^2).\n");
    fprintf(stderr, "if -m is set,"
            "all output will be muted except computation time.\n");
    exit(1);
  }

  srand(time(0));

  /* Generate random inputs */
  a = (char*)malloc( (n+1) * sizeof(char) );
  b = (char*)malloc( (n+1) * sizeof(char) );

  genRandString(a, n, alphabet);
  genRandString(b, n, alphabet);
  long long result;


  if(!mute) {
    printf("\nCalculate using divide and conquer method... ");
    printf("(timing start here)\n");
  }

  int **stor = NULL;

#if TIMING_COUNT
  clockmark_t begin, end;
  uint64_t elapsed[TIMING_COUNT];

  for(int i=0; i < TIMING_COUNT; i++) {
    if (stor) {
      for (int fidx = 0; fidx < n + 1; ++fidx) {
        free( stor[fidx] );
      }
      free(stor);
    }
    stor = (int**)malloc( (n+1) * sizeof(int*) );
    init_stor(stor, n);
    __cilkrts_init();
    __asm__ volatile ("" ::: "memory");
    begin = ktiming_getmark();
    result = parallelNEWdivLCS(stor, 1, 1, n, base);
    end = ktiming_getmark();
    __asm__ volatile ("" ::: "memory");
    elapsed[i] = ktiming_diff_usec(&begin, &end);
  }
  print_runtime(elapsed, TIMING_COUNT);

#else 
  stor = (int**)malloc( (n+1) * sizeof(int*) );
  init_stor(stor, n);
  result = parallelNEWdivLCS(stor, 1, 1, n, base);
#endif

  /* Terminate Cilk runtime */
  //__cilkrts_end_cilk();

  if ( !mute ) {
    printf("\nCilk Example: LCS\n");
    printf("        running on %d processor%s\n\n",
           __cilkrts_get_nworkers(), __cilkrts_get_nworkers() > 1 ? "s" : "");
    printf("Options: size = %d, alphabet size = %d, base size = %d\n",
           n, alphabet, base);
    printf("Result: %llu\n", result);
    if ( check ) {
      if ( result == serialquadLCS( n, a, b ) ){
        printf("Correct Solution: YES\n");
      } else {
        printf("Correct Solution: NO\n");
      }
    }
  } 

  /* Clean up memory */

  for ( i = 0; i < n + 1; ++i) {
    free( stor[i] );
  }
  free( stor );
  free( a );
  free( b );

  return 0;
}
