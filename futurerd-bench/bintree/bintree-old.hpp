// An simple binary tree with key only; no data.
#ifndef __BINTREE_HPP__
#define __BINTREE_HPP__

#include <cstdio>
#include <future.hpp>

using namespace std;

class bintree {
public:
    // key_t has to be something that can be used as array index 
    using key_t = int;
    struct node {
        key_t key;
        union {
            node *left = nullptr;
            cilk::future<node*> *fut_left = nullptr;
        };
        union {
            node *right = nullptr;
            cilk::future<node *> *fut_right = nullptr;
        };
        node() = delete; 
        node(key_t k) : key(k), left(nullptr), right(nullptr) {}
    }; // struct node

    ~bintree() {
        if(m_root) {
            cleanup(m_root);
        }
    }

    void merge(bintree *that);
    
    // Utility
    inline size_t size() const { return m_size; }

    inline void insert(key_t k) {
        if(m_root == nullptr) { m_root = new node(k);
        } else { insert(m_root, k); }
        m_size++; // insert always succeeds barring out of memory
    }

    inline int validate() const {
        size_t size = validate(m_root);
        assert(size == m_size);
        return 0; // always return 0 at root if successful
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

private:
    node* m_root = nullptr;
    size_t m_size = 0;

    // Technically part of node class, but defined as static methods in bintree class
    static node * merge(node *this_root, node *that_root); 
    static node * split(key_t s, node *n, // structured; the default one
                    cilk::future<node *> **res_left,
                    cilk::future<node *> **res_right);
    static void split_unstructured(key_t s, node *n, // unstructured
                    cilk::future<node *> **res_left,
                    cilk::future<node *> **res_right);
    static void insert(node* n, key_t k);
    static size_t validate(node* n);
    static void get_key_counts(node* n, int *counts, key_t max_key);
    static void cleanup(node* n);
    static void replace_all(node *n);
    static void print_keys(node* n);

}; // class bintree

#endif // __BINTREE_HPP__
