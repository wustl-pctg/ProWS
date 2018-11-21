#include "bintree.hpp"
#include <cstdlib>

#ifndef SERIAL_ELISION
  #include <cilk/cilk.h>
#else
  #define cilk_spawn
#endif
  

using node = bintree::node;
using key_t = bintree::key_t;

extern key_t g_key_max;

static inline key_t randkey() { return rand() % (g_key_max + 1); }

bintree* prepare(const size_t size) {
  bintree* t = new bintree();
  for (int i = 0; i < size; ++i) t->insert(randkey());
  assert(t->validate()); 
  return t;
}

void prepare_trees(bintree **t1, bintree **t2, const size_t &t1_size, const size_t &t2_size) {
  *t1 = prepare(t1_size);
  *t2 = prepare(t2_size);
}

node* bintree::insert(node* n, const key_t k) {
  if (!n) return new node(k);
  if (k < n->key)
    n->left = insert(n->left, k);
  else
    n->right = insert(n->right, k);
  return n;
}
