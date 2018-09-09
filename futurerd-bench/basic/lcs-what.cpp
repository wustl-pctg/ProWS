int __attribute__((noinline)) wave_lcs_with_futures(int *stor, char *a, char *b, int n) {
  int nBlocks = NUM_BLOCKS(n);
  
  // walk the upper half of triangle, including the diagonal (we assume square NxN LCS)
  for(int wave_front = 0; wave_front < nBlocks; wave_front++) {
    #pragma cilk grainsize = 1
    cilk_for(int jB = 0; jB <= wave_front; jB++) {
      int iB = wave_front - jB;
      process_lcs_tile(stor, a, b, n, iB, jB);
    }
  }

  // walk the lower half of triangle
  for(int wave_front = 1; wave_front < nBlocks; wave_front++) {
    int iBase = nBlocks + wave_front - 1;
    #pragma cilk grainsize = 1
    cilk_for(int jB = wave_front; jB < nBlocks; jB++) {
      int iB = iBase - jB;
      process_lcs_tile(stor, a, b, n, iB, jB);
    }
  }

  return stor[n*(n-1) + n-1];
}
