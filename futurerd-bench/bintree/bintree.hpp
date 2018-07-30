#pragma once
// Binary search tree, duplicates allowed

#include <cstdio>
#include <utility> // pair

#include <future.hpp>
//#include <futptr.hpp>

class bintree {
public:
  static constexpr int DEPTH_LIMIT = 5;
  using key_t = int;
  struct node {
    key_t key;
    node *left = nullptr;
    node *right = nullptr;

    node() = delete;
    node(key_t k) : key(k) {}
    ~node() { delete left; delete right; }
  };
  using fut_t = cilk::future<node*>;
  using futpair_t = std::pair<fut_t*, fut_t*>;

  ~bintree() { delete m_root; }

  node* insert(node* n, const key_t k);
  inline void insert(key_t k) {
    m_size++; // We allow duplicates
    m_root = insert(m_root, k);
  }
  void merge(bintree* that);
  inline void replace_all() { replace_all(m_root); }

  // Utility
  inline std::size_t size() const { return m_size; }
  inline bool validate() const {
    std::size_t size = validate(m_root);
    assert(size == m_size);
    return true; // always return true at root if successful
  }

  inline int get_key_counts(int *counts, key_t max_key) {
    get_key_counts(m_root, counts, max_key);
    return 0; // always return 0 at root if successful
  }

  inline void print_keys() {
    fprintf(stderr, "size: %lu.\n", m_size);
    print_keys(m_root);
    fprintf(stderr, "\n\n");
  }

  static std::pair<node*,node*> split(node* n, key_t s);
  static node* merge(node* lr, node* rr, int depth);

private:
  node* m_root = nullptr;

  // Size is a bit harder to maintain if you want to get rid of
  // duplicates. Would need to make splitL and splitR non-static
  // methods (so they can change m_size, when they find a duplicate
  // while merging), but then they become harder to call in the "future"
  // helper.
  std::size_t m_size = 0;


  //static node* split(node* n, key_t s, fut_t* res_left, fut_t* res_right, int depth);
  //futpair_t split2(node* n, key_t s);
  //node* merge(node* lr, cilk::future<node*>* rr, int depth);

  static std::size_t validate(node* n);
  static void get_key_counts(node* n, int *counts, key_t max_key);
  static void print_keys(node* n);
  static void replace_all(node *n);

}; // class bintree
