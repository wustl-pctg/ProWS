#include <cassert>
#include <cstdint>
#include <cstdio>

#include "bintree.hpp"
using key_t = bintree::key_t;
using node = bintree::node;


// check whether f is marked as a future pointer
#define IS_FUTPTR(f) (((void*) (((uintptr_t)(f)) & ((uintptr_t)0x1))) != nullptr)
// get the underlying future pointer
#define GET_FUTPTR(f) ((cilk::future<node*>*) ( ((uintptr_t)(f)) & ((uintptr_t)~0x1) ))
// mark the underlying future pointer
// SET_FUTPTR expects loc to be an address to future pointer, so future **
#define SET_FUTPTR(loc,f)                                               \
  (*(loc) = ((cilk::future<node*>*)(((uintptr_t)(f)) | ((uintptr_t)0x1))))
// set loc to point to the real value gotten from the future pointer
// REPLACE expects address to future pointer, so future **
#define REPLACE(loc)                            \
  if (IS_FUTPTR(*loc)) {                        \
    auto f = GET_FUTPTR(*loc);                  \
    *loc = f->get();                            \
    delete f;                                   \
  }

// only used for base case; SET_FUTPTR and REPLACE in one.
// we are setting the future pointer to point to nullptr
// and recast it back to node
#define SET_AND_REPLACE(loc, v) {               \
    auto f = *loc;                              \
    SET_FUTPTR(loc, f);                         \
    f->finish(v);  /* put here? */              \
  }

void bintree::merge(bintree *that) {
  m_root = merge(this->m_root, that->m_root);

  // TODO: Angelina claims this isn't necessary. I don't understand.
  replace_all(m_root); // have to do this to "touch" every node
  this->m_size += that->m_size;
  that->m_root = nullptr;
  that->m_size = 0;
}

/* technically member functions of a node */

// use this as default --- structured use of futures
// one of the res_left / res_right should correspond to the spawning of this computation
using result_t = cilk::future<node*>**;
node * bintree::split(key_t s, node *n,
                      cilk::future<node *> **res_left,
                      cilk::future<node *> **res_right) {

  assert(!IS_FUTPTR(n));
  assert(n != nullptr);

  if(s < n->key) {
    REPLACE(&n->left); // go to left; need the pointer now
    node *left = n->left;

    if(left == nullptr) {
      assert(!IS_FUTPTR(*res_left) && !IS_FUTPTR(*res_right));
      SET_AND_REPLACE(res_left, nullptr);
      SET_AND_REPLACE(res_right, nullptr);
      n->left = nullptr; // set up node(v, R1, R)
    } else {
      // need a new res_right, since we are resolving the res_right at this level
      cilk::future<node *> *new_res_right = new cilk::future<node *>();
      if(s < left->key) { // we would go to left at the next level
        //reuse_future(node *, new_res_right, split, s, left, res_left, &new_res_right);
        reasync_helper<node*,key_t,node*,result_t,result_t>
          (new_res_right, split, s, left, res_left, &new_res_right);
      } else { // we would go to right at the next level
        //reuse_future(node *, (*res_left), split, s, left, res_left, &new_res_right);
        reasync_helper<node*,key_t,node*,result_t,result_t>
          ((*res_left), split, s, left, res_left, &new_res_right);
        SET_FUTPTR(res_left, *res_left);
      }
      SET_FUTPTR(&n->fut_left, new_res_right); // set up node(v, R1, R)
    }

  } else {
    REPLACE(&n->right); // go to right; need the pointer now
    node *right = n->right;

    if(right == nullptr) {
      assert(!IS_FUTPTR(*res_left) && !IS_FUTPTR(*res_right));
      SET_AND_REPLACE(res_left, nullptr);
      SET_AND_REPLACE(res_right, nullptr);
      n->right = nullptr; // set up node(v, L, nullptr)
    } else {
      // need a new res_left, since we are resolving the res_left at this level
      cilk::future<node *> *new_res_left = new cilk::future<node *>();
      if(s < right->key) { // we would go to left at the next level
        //reuse_future(node *, (*res_right), split, s, right, &new_res_left, res_right);
        reasync_helper<node*,key_t,node*,result_t,result_t>
          ((*res_right), split, s, right, &new_res_left, res_right);
        SET_FUTPTR(res_right, *res_right);
      } else { // we would go to right at the next level
        //reuse_future(node *, new_res_left, split, s, right, &new_res_left, res_right);
        reasync_helper<node*,key_t,node*,result_t,result_t>
          (new_res_left, split, s, right, &new_res_left, res_right);
      }
      SET_FUTPTR(&n->fut_right, new_res_left); // set up node(v, L, L1)
    }
  }
  return n;
}


node * bintree::merge(node *this_root, node *that_root) {

  assert(!IS_FUTPTR(this_root)); assert(!IS_FUTPTR(that_root));

  if(that_root == nullptr) {
    if(this_root) { REPLACE(&this_root->left); REPLACE(&this_root->right); }
    return this_root;
  }
  if(this_root == nullptr) {
    if(that_root) { REPLACE(&that_root->left); REPLACE(&that_root->right); }
    return that_root;
  }

  key_t s = this_root->key;
  cilk::future<node *> *split_left = new cilk::future<node*>();
  cilk::future<node *> *split_right = new cilk::future<node *>();

  if(s < that_root->key) { // going left on the split; that_root will be split_right
    //reuse_future(node *, split_right, split, s, that_root, &split_left, &split_right);
    reasync_helper<node*,key_t,node*,result_t,result_t>
      (split_right, split, s, that_root, &split_left, &split_right);
    SET_FUTPTR(&split_right, split_right);
  } else { // going right on the split; that_root will be split_left
    //reuse_future(node *, split_left, split, s, that_root, &split_left, &split_right);
    reasync_helper<node*,key_t,node*,result_t,result_t>
      (split_left, split, s, that_root, &split_left, &split_right);

    SET_FUTPTR(&split_left, split_left);
  }


  REPLACE(&this_root->left); REPLACE(&this_root->right);
  REPLACE((node **)&split_left); REPLACE((node **)&split_right);

  //create_future(node *, merged_left, merge, this_root->left, (node *)split_left);
  cilk::future<node *> *merged_left = new cilk::future<node *>();
  cilk::future<node *> *merged_right = new cilk::future<node *>();

  // TODO: why reasync? Just use async. (Actually, why not spawn?)
  reasync_helper<node*,node*,node*>(merged_left, merge, this_root->left, (node*)split_left);
  //SET_FUTPTR(&this_root->fut_left, merged_left);
  //create_future(node *, merged_right, merge, this_root->right, (node *)split_right);
  reasync_helper<node*,node*,node*>(merged_right, merge, this_root->right, (node*)split_right);
  //SET_FUTPTR(&this_root->fut_right, merged_right);

  return this_root;
}

void bintree::insert(node *n, key_t k) {
  assert(n != nullptr);

  if(k < n->key) { // insert into left
    if(n->left) { insert(n->left, k); }
    else { n->left = new node(k); }
  } else { // otherwise insert into right
    if(n->right) { insert(n->right, k); }
    else { n->right = new node(k); }
  }
}

size_t bintree::validate(node *n) {
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

void bintree::cleanup(node* n) {
  if (n == nullptr) return;
  if (n->left) cleanup(n->left);
  if (n->right) cleanup(n->right);
  delete n;
}

void bintree::replace_all(node *n) {
  if(!n) return;
  assert(!IS_FUTPTR(n->left));
  assert(!IS_FUTPTR(n->right));
  if(IS_FUTPTR(n->left)) { REPLACE(&n->left); }
  if(IS_FUTPTR(n->right)) { REPLACE(&n->right); }
  replace_all(n->left);
  replace_all(n->right);
}

void bintree::print_keys(node *n) {
  if(!n) return;
  assert(!IS_FUTPTR(n));
  print_keys(n->left);
  fprintf(stderr, "%p: %i, ", n, n->key);
  print_keys(n->right);
}
