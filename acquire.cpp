#include "cilkrr.h"
#include <sstream>
#include <cstdio>
#include <fstream>

namespace cilkrr {
  std::string get_pedigree_str()
  {
    acquire_info tmp = acquire_info(cilkrr::get_pedigree());
    return tmp.str();
  }

  acquire_info::acquire_info(pedigree_t p)
    : ped(p), next(nullptr), suspended_deque(nullptr) {}

  acquire_info::acquire_info(pedigree_t p, full_pedigree_t _full)
    : acquire_info(p)
  {
    full = _full;
  }

  std::string acquire_info::array_str()
  {
    full_pedigree_t p = full;
    std::string s = "[";
    for (int i = 0; i < p.length; ++i)
      s += std::to_string(p.array[i]) + ",";
    s += "]";
    return s;
  }

  // We don't really care about speed here, maybe reeval later.
  std::string acquire_info::str()
  {
    std::string s = std::to_string(ped);
    if (full.length > 0)
      s += array_str();
    return s;
  }
  
  std::ostream& operator<< (std::ostream &out, acquire_info s)
  {
    out << s.str();
    return out;
  }

  acquire_container::acquire_container(size_t size)
    : m_chunk_size(size)
  {
    m_first_chunk = m_current_chunk = new chunk();
    assert(m_first_chunk);
    
    m_current_chunk->size = m_chunk_size;
    m_current_chunk->array = (acquire_info*) malloc(m_chunk_size * sizeof(acquire_info));
    assert(m_current_chunk->array);
    m_current_chunk->next = nullptr;

    m_buckets = (acquire_info**) calloc(m_num_buckets, sizeof(acquire_info*));
    assert(m_buckets);
  }

#if PTYPE == PARRAY
  size_t acquire_container::hash(full_pedigree_t k)
  {
    size_t h = 0;
    for (int i = 0; i < k.length; ++i)
      h += g_rr_state->randvec[i] * k.array[i];
    return h % m_num_buckets;
  }
#endif

  void acquire_container::reset() { m_it = m_first; }
  acquire_info* acquire_container::next()
  {
    if (m_it == end()) return nullptr;
    return m_it = m_it->next;
  }
  acquire_info* acquire_container::current() { return m_it; }
  acquire_info* acquire_container::begin() { return m_first; }
  acquire_info* acquire_container::end() { return nullptr; }

  acquire_info* acquire_container::find(const pedigree_t& p)
  {
    acquire_info *current, *base, *match;
    size_t found = 0;

    match = current = base = find_first(p);
    while (current) {
      if (current->ped == p) found++;
      if (found > 1) break;
      current = current->chain_next;
    }

    if (found == 0) {
      fprintf(stderr, "Cilkrecord error: can't find pedigree %zu\n", p);
      std::abort();
    } else if (found > 1) {
      
      const full_pedigree_t& full = get_full_pedigree();
      current = base;

      while (current) {
        if (current->ped == p) match = current;
        if (current->full == full) break;
        current = current->chain_next;
      }
      assert(match->full.length == 0 || match->full == full);
    }
    assert(match != nullptr);
    if (match->next)
      __builtin_prefetch(&m_buckets[hash(match->next->ped)]);
    return match;
  }

  acquire_info* acquire_container::find_first(const pedigree_t& p)
  {
    acquire_info *current = m_buckets[hash(p)];

    while (current) {
      if (current->ped == p)
        return current;
      current = current->chain_next;
    }
    return nullptr;
  }
  
  acquire_info* acquire_container::find(const pedigree_t& p,
                                        const full_pedigree_t &full)
  {
    acquire_info *current = find_first(p);

    while (current) {
      if (current->ped == p && current->full == full)
        return current;
      current = current->chain_next;
    }

    // Not found!!
    current = new acquire_info(p, full);
    fprintf(stderr, "Cilkrecord error: can't find pedigree %s\n",
            current->str().c_str());
    std::abort();
  }

  void acquire_container::print(std::ofstream& output)
  {
    struct chunk *c = m_first_chunk;
    while (c) {
      size_t size = (c == m_current_chunk) ? m_index : c->size;
      for (int i = 0; i < size; ++i)
        output << "\t" << c->array[i] << std::endl;
      c = c->next;
    }
  }
  
  void acquire_container::bucket_add(acquire_info** bucket, acquire_info* item)
  {
    item->chain_next = *bucket;
    *bucket = item;
    
    if (++m_size / ((double) m_num_buckets) > 1.0)
      rehash(2*m_num_buckets);

  }

  void acquire_container::rehash(size_t new_cap)
  {
    if (new_cap <= m_num_buckets) return;

    acquire_info** new_buckets = (acquire_info**) calloc(new_cap, sizeof(acquire_info*));
    size_t num_old_buckets = m_num_buckets;
    m_num_buckets = new_cap; // I change this so the hash will work
    m_mask = ((m_mask + 1) << 1) - 1;

    for (int i = 0; i < num_old_buckets; ++i) {
      acquire_info *current = m_buckets[i];
      acquire_info *next;
      while (current) {
        next = current->chain_next;
        
        //        size_t check = bucket_add(&new_buckets[hash(current->ped)], current);
        // essentially bucket_add, but we don't need to check for duplicates
        acquire_info** bucket = &new_buckets[hash(current->ped)];

        if (*bucket == nullptr) {
          *bucket = current;
          current->chain_next = nullptr;
        } else {
          current->chain_next = (*bucket)->chain_next;
          (*bucket)->chain_next = current;
        }

        current = next;
      }
    }

    free(m_buckets);
    m_buckets = new_buckets;
  }

  acquire_info* acquire_container::new_acquire_info()
  {
    acquire_info *a = &m_current_chunk->array[m_index++];
    a->next = nullptr;
    a->suspended_deque = nullptr;

    if (m_index >= m_chunk_size) {
      m_index = 0;
      if (m_chunk_size < 8192) m_chunk_size *= 2;
      m_current_chunk = m_current_chunk->next = new chunk();
      m_current_chunk->size = m_chunk_size;
      m_current_chunk->next = nullptr;
      m_current_chunk->array = (acquire_info*) malloc(m_chunk_size * sizeof(acquire_info));
    }

    return a;
  }

  acquire_info* acquire_container::add(pedigree_t p, full_pedigree_t full)
  {
    acquire_info *a = new(new_acquire_info()) acquire_info(p, full);
    
    acquire_info **bucket = &m_buckets[hash(p)];
    bucket_add(bucket, a);

    if (full.length > 0)
      m_num_conflicts++;

    if (m_first == nullptr) m_first = m_it = a;
    else m_it = m_it->next = a;
    return a;
  }

  acquire_info* acquire_container::add(pedigree_t p)
  {
    full_pedigree_t full = {0, nullptr};
    if (find_first(p) != nullptr)
      full = get_full_pedigree();
    return add(p, full);
  }

  size_t acquire_container::memsize()
  {
    struct chunk *c = m_first_chunk;
    size_t acquire_info_size = 0;

    while (c) {
      acquire_info_size += c->size;
      c = c->next;
    }
    return acquire_info_size + m_num_buckets;
  }

  void acquire_container::stats()
  {
    // fprintf(stderr, "Avg chain length: %lf\n",
    //         ((double)m_size) / ((double)m_num_buckets));
    // fprintf(stderr, "Num conflicts: %zu\n", m_num_conflicts);
  }

}
