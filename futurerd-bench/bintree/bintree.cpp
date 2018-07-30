#include <cassert>
#include <cstdlib> // malloc
#include <cstdint> // uintptr_t

#include "bintree.hpp"

#include <cilk/cilk.h>
#define spawn cilk_spawn
#define sync cilk_sync

using key_t = bintree::key_t;
using node = bintree::node;
using fut_t = bintree::fut_t;
using futpair_t = bintree::futpair_t;

// Helper macros for dealing with pointers that may or may not be
// future pointers.
#ifdef NONBLOCKING_FUTURES
#define IS_FUTPTR(f)                                            \
  (((void*) (((uintptr_t)(f)) & ((uintptr_t)0x1))) != nullptr)

// get the underlying future pointer
#define GET_FUTPTR(f)                                               \
  ((cilk::future<node*>*) ( ((uintptr_t)(f)) & ((uintptr_t)~0x1) ))

// mark the underlying future pointer
// SET_FUTPTR expects loc to be an address to future pointer, so future **
#define SET_FUTPTR(loc,f)                                   \
  (*(loc) = ((node*)(((uintptr_t)(f)) | ((uintptr_t)0x1))))

// set loc to point to the real value gotten from the future pointer
// REPLACE expects address to future pointer, so future **
#define REPLACE(loc)                            \
  if (IS_FUTPTR(*loc)) {                        \
    auto f = GET_FUTPTR(*loc);                  \
    *loc = f->get();                            \
    delete f;                                   \
  }

// We don't actually support put/get style, but this makes things clearer.
static node* immediate(node *n) { return n; }
#define put(fut,res) reasync_helper<node*,node*>((fut), immediate, (res))

#else
#define IS_FUTPTR(f) false
#define GET_FUTPTR(f) nullptr
#define SET_FUTPTR(loc,f) *(loc) = nullptr
#define REPLACE(loc)
#endif

void bintree::merge(bintree *that) {
  this->m_size += that->m_size;
  m_root = merge(this->m_root, that->m_root, 0);

  // NB: some child pointers will actually be future pointers. The
  // results are all ready, but may not have been touched.  This can
  // happen, e.g. when an empty node (nullptr) is merged with a
  // subtree, in which case there was no need to descend down the
  // subtree and touch its descendants.
  // replace_all(m_root); // have to do this to "touch" every node

  that->m_root = nullptr;
  that->m_size = 0;
  delete that;
}

node* bintree::insert(node* n, const key_t k) {
  if (!n) return new node(k);
  if (k < n->key)
    n->left = insert(n->left, k);
  else
    n->right = insert(n->right, k);
  return n;
}

std::size_t bintree::validate(node *n) {
  if (n == nullptr) return 0;
  assert(n->key >= 0);
  assert(!n->left || n->left->key <= n->key);
  assert(!n->right || n->right->key >= n->key);
  return validate(n->left) + validate(n->right) + 1;
}

void bintree::get_key_counts(node* n, int *counts, key_t max_key) {
  if (n == nullptr) return;
  get_key_counts(n->left, counts, max_key);
  get_key_counts(n->right, counts, max_key);
  assert(n->key >= 0);
  assert(n->key <= max_key);
  counts[n->key]++;
}

void bintree::replace_all(node *n) {
  if(!n) return;
  if(IS_FUTPTR(n->left)) { REPLACE(&n->left); }
  if(IS_FUTPTR(n->right)) { REPLACE(&n->right); }
  replace_all(n->left);
  replace_all(n->right);
}

void bintree::print_keys(node *n) {
  if(!n) return;
  fprintf(stderr, "%i", n->key);

  fprintf(stderr, " (");
  if (IS_FUTPTR(n->left)) {
    fprintf(stderr, "f");
    REPLACE(&n->left);
  }
  print_keys(n->left);
  fprintf(stderr, ")");

  fprintf(stderr, " (");
  if (IS_FUTPTR(n->right)) {
    fprintf(stderr, "f");
    REPLACE(&n->right);
  }
  print_keys(n->right);
  fprintf(stderr, ")");

  // print_keys(n->left);
  // fprintf(stderr, "%p: %i, ", n, n->key);
  // print_keys(n->right);
}

// Sequential split, for fork-join merge and base case for pipelined
// split.  Without NONBLOCKING_FUTURES defined, the uppercase macros
// won't do anything.
std::pair<node*,node*> bintree::split(node* n, key_t s) {
  if (!n) return {nullptr, nullptr};
  assert(!IS_FUTPTR(n));
  node *left, *right;
  if (s < n->key) { // go left
    right = n;
    REPLACE(&n->left);
    auto next = split(n->left, s);
    left = next.first;
    n->left = next.second;
  } else { // go right
    left = n;
    REPLACE(&n->right);
    auto next = split(n->right, s);
    n->right = next.first;
    right = next.second;
  }
  return {left, right};
}

// Pipelined splitting
#ifdef NONBLOCKING_FUTURES
#define async_split(fut, args...)                                     \
  reasync_helper<node*,node*,key_t,fut_t*,fut_t*>((fut), split, args)

static node* split(node* n, key_t s,
                   fut_t* res_left, fut_t* res_right,
                   int depth) {
  assert(!IS_FUTPTR(n));
  assert(n);

  if (s < n->key) { // go left
    REPLACE(&n->left); // make it ready, if it wasn't already
    auto next = n->left;

    if (!next) {
      put(res_left, nullptr);
    } else { // lookahead

      if (depth >= bintree::DEPTH_LIMIT) {
        auto res = bintree::split(next, s);
        put(res_left, res.first);
        n->left = res.second;
        return n;
      }

      auto next_res_right = (fut_t*) malloc(sizeof(fut_t));
      SET_FUTPTR(&n->left, next_res_right);

      if (s < next->key) { // left-left case
        async_split(next_res_right, next, s, res_left, next_res_right, depth+1);
      } else { // left-right case
        async_split(res_left, next, s, res_left, next_res_right, depth+1);
      }
    }
  } else { // go right
    REPLACE(&n->right);
    auto next = n->right;

    if (!next) {
      put(res_right, nullptr);
    } else { // lookahead

      if (depth >= bintree::DEPTH_LIMIT) {
        auto res = bintree::split(next, s);
        n->right = res.first;
        put(res_right, res.second);
        return n;
      }

      auto next_res_left = (fut_t*) malloc(sizeof(fut_t));
      SET_FUTPTR(&n->right, next_res_left);

      if (s < next->key) { // right-left case
        async_split(res_right, next, s, next_res_left, res_right, depth+1);
      } else { // right-right case
        async_split(next_res_left, next, s, next_res_left, res_right, depth+1);
      }
    }
  }
  return n;
}

// Helper to "launch" two futures for splitting
static futpair_t split2(node* n, key_t s) {
  auto left = (fut_t*) malloc(sizeof(fut_t));
  auto right = (fut_t*) malloc(sizeof(fut_t));

  // lookahead
  if (s < n->key)
    async_split(right, n, s, left, right, 0);
  else
    async_split(left, n, s, left, right, 0);

  return {left, right};
}
#endif

// Merging
#ifdef NONBLOCKING_FUTURES
// If I don't give this a unique name (not "merge"), for some reason
// the compiler will not consider it as a possible candidate for the
// spawns below, even though it has a different signature...
static node* merge_helper(node* lr, fut_t* rr, int depth) {
  auto res = bintree::merge(lr, rr->get(), depth);
  free(rr);
  return res;
}
#endif

node* bintree::merge(node* lr, node* rr, int depth) {
  if (!lr) return rr;
  if (!rr) return lr;

  assert(!IS_FUTPTR(rr));

  // TODO: fast sequential merge?
  if (depth >= DEPTH_LIMIT) {
    auto res = split(rr, lr->key);
    lr->left  = merge(lr->left, res.first, depth+1);
    lr->right = merge(lr->right, res.second, depth+1);
    return lr;
  }

#ifdef STRUCTURED_FUTURES
#define split2 split
#define merge_helper merge
#endif

  auto res = split2(rr, lr->key);
  auto left = res.first;
  auto right = res.second;

  lr->left  = spawn merge_helper(lr->left, left, depth+1);
  lr->right =       merge_helper(lr->right, right, depth+1);
  sync;

  return lr;
}
