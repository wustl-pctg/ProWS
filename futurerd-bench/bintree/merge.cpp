// Repeatedly merge binary trees.
#include <cstdio>
#include <cstdlib>
#include <chrono>

// #include <cilk/cilk.h>
// #include <cilk/cilk_api.h>

#include "bintree.hpp"
#include "../util/util.hpp"
#include "../util/getoptions.hpp"

using key_t = bintree::key_t;

// these will be initialized later
key_t g_key_max = 0;


// size of first tree, size of second tree, max key to use
const char* specifiers[] = {"-s1", "-s2", "-kmax", "-c"};
int opt_types[] = { INTARG, INTARG, INTARG, BOOLARG };

extern void prepare_trees(bintree **t1, bintree **t2, const size_t &t1_size, const size_t &t2_size);

int main(int argc, char* argv[]) {

  size_t t1_size = 4096;
  size_t t2_size = 2078;
  key_t key_max = 4096 * 4;
  int check = 0;

  //ensure_serial_execution();
  get_options(argc, argv, specifiers, opt_types, &t1_size, &t2_size, &key_max, &check);
  g_key_max = key_max;

  if( t1_size < t2_size ) { // always make t2, the one we split, smaller
    size_t tmp = t1_size;
    t1_size = t2_size;
    t2_size = tmp;
  }

  int *t1_key_counts = nullptr, *t2_key_counts = nullptr;

  // Prep the trees
  //bintree* t1 = prepare(t1_size);
  //bintree* t2 = prepare(t2_size);
  bintree *t1;
  bintree *t2;
  prepare_trees(&t1, &t2, t1_size, t2_size);

  // fprintf(stderr, "t1: ");
  // t1->print_keys();
  // fprintf(stderr, "t2: ");
  // t2->print_keys();

  if(check) {
    t1_key_counts = new int[g_key_max+1];
    t2_key_counts = new int[g_key_max+1];
    t1->get_key_counts(t1_key_counts, g_key_max);
    t2->get_key_counts(t2_key_counts, g_key_max);

    for(int i = 0; i < (g_key_max + 1); i++) {
      t1_key_counts[i] += t2_key_counts[i];
      t2_key_counts[i] = 0; // reset t2_key_counts;
    }
  }

  auto start = std::chrono::steady_clock::now();
  __asm__ volatile ("" ::: "memory");
  t1->merge(t2);
  __asm__ volatile ("" ::: "memory");
  t1->replace_all();
  __asm__ volatile ("" ::: "memory");
  auto end = std::chrono::steady_clock::now();

  // fprintf(stderr, "merged: ");
  // t1->print_keys();

  if(check) {
    t1->validate();
    t1->get_key_counts(t2_key_counts, g_key_max);
    // make sure that we got all the keys correctly
    for(int i = 0; i < (g_key_max + 1); i++) {
      assert(t1_key_counts[i] == t2_key_counts[i]);
    }
    printf("Check passed.\n");

    delete[] t1_key_counts;
    delete[] t2_key_counts;
  }

  auto time = std::chrono::duration <double, std::milli> (end-start).count();
  printf("%s: s1=%zu s2=%zu\n", argv[0], t1_size, t2_size);
  printf("Benchmark time: %f ms\n", time);

  delete t1; // The merge takes care of t2

  return 0;
}
