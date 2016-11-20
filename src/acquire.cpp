#include "porr.h"
#include <sstream>
#include <cstdio>
#include <fstream>
#include <cstring> // memset

namespace porr {

  // Initialize static thread locals
  __thread size_t acquire_container::t_index = 0;
  __thread acquire_container::chunk_t* acquire_container::t_first_chunk = nullptr;
  __thread acquire_container::chunk_t* acquire_container::t_current_chunk = nullptr;

  std::string get_pedigree_str()
  {
    acquire_info tmp = acquire_info(porr::get_pedigree());
    return tmp.str();
  }

  acquire_info::acquire_info(pedigree_t p) : ped(p) {}
  acquire_info::acquire_info(pedigree_t p, full_pedigree_t f)
  //: ped(p), full(f) {}
  {
    ped = p;
    full = f;
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

  acquire_container::acquire_container(acquire_info** start_ptr)
  {
    //return;
    enum mode m = g_rr_state->m_mode;
    if (m == NONE) return;

    // m_size = m_index = 0;
    // m_it = nullptr;
    
    acquire_info *first;
    if (m == RECORD) {
      first = new_acquire_info();
      memset(first, 0, sizeof(acquire_info));
      *start_ptr = first;
    } else {
      first = *start_ptr;
      m_size = first->full.length;
      //m_table = (acquire_info**)first->full.array;
      first = first->next;

      // Using array to search
      m_start = first;
    }
    m_it = first;
  }

  // acquire_container::acquire_container(acquire_info *first, acquire_info *table)
  //   : m_it(first), m_table(table) {}

#if PTYPE == PARRAY
  size_t acquire_container::hash(full_pedigree_t k)
  {
    size_t h = 0;
    for (int i = 0; i < k.length; ++i)
      h += g_rr_state->randvec[i] * k.array[i];
    //return h % m_num_buckets;
    //return h % m_filter_size;
    return h;
  }
#endif

  acquire_info* acquire_container::find(const pedigree_t& p)
  {
    //return nullptr
    size_t num_matches = 0;
    acquire_info* first_match = nullptr;

    // Simple search (use a->next in loop)
    acquire_info* it = m_it;

    // Hash table search (use a->chain_next in loop)
    // acquire_info** debug = m_table;
    // acquire_info* it = m_table[p % m_size];

    // if (&m_start[m_index] != m_it) {
    //   fprintf(stderr, "Error in find\n");
    // }

    full_pedigree_t full;
    for (acquire_info *a = &m_start[m_index]; a != &m_start[m_size]; ++a) {
    // for (size_t i = m_index; i < m_size; ++i) {
    //   acquire_info *a = &m_start[i];
    // for (acquire_info* a = it; a != nullptr; a = a->next) {
      if (a->ped == p) {
        num_matches++;
        if (num_matches == 1)
          first_match = a;
        else {
          if (num_matches == 2)
            full = get_full_pedigree();
          if (a->full == full) {
            free(full.array);
            return a;
          }
        }
      }
    }
    if (num_matches == 0) {
      fprintf(stderr, "Error: %zu not found!\n", p);
      std::exit(1);
    }
    return first_match;

    // if (num_matches > 1) {
    //   full_pedigree_t full = get_full_pedigree();
    //   for (acquire_info* a = first_match; a->next != nullptr; a = a->next) {
    //     if (a->ped == p && a->full == full)
    //       return a;
    //     first_match = first_match->next;
    //   }
    // }
    // return first_match;
      
  }

//   acquire_info* acquire_container::find(const pedigree_t& p)
//   {
//     acquire_info *current, *base, *match;
//     size_t found = 0;

//     match = current = base = find_first(p);
//     while (current) {
//       if (current->ped == p) found++;
//       if (found > 1) break;
//       current = current->chain_next;
//     }

//     if (found == 0) {
//       fprintf(stderr, "Cilkrecord error: can't find pedigree %zu\n", p);
//       std::abort();
//     }

// #if PTYPE == PARRAY
//     found = 2; // just make sure we get the full pedigree
// #endif

//     if (found > 1) {
//       const full_pedigree_t& full = get_full_pedigree();
//       current = base;

//       while (current) {
//         if (current->ped == p) match = current;
//         if (current->full == full) break;
//         current = current->chain_next;
//       }
//       assert(match->full.length == 0 || match->full == full);
//     }
//     assert(match != nullptr);
//     // if (match->next)
//     //   __builtin_prefetch(&m_buckets[hash(match->next->ped)]);
//     return match;
//   }

  // // Just find the first
  // acquire_info* acquire_container::bucket_find(acquire_info **const bucket,
  //                                              const pedigree_t& p)
  // {
  //   acquire_info *current = (*bucket);

  //   while (current) {
  //     if (current->ped == p)
  //       break;
  //     current = current->chain_next;
  //   }
  //   return current;

  // }

  // void acquire_container::rehash(size_t new_cap)
  // {
  //   if (new_cap <= m_num_buckets) return;
  //   //auto start = std::chrono::high_resolution_clock::now();
    
  //   acquire_info** new_buckets = (acquire_info**) calloc(new_cap, sizeof(acquire_info*));
  //   assert(new_buckets);
  //   assert(new_buckets[0] == nullptr);
  //   if (m_buckets)
  //     acquire_info *test = m_buckets[0];
  //   size_t num_old_buckets = m_num_buckets;
  //   m_num_buckets = new_cap; // I change this so the hash will work
  //   //m_mask = ((m_mask + 1) << 1) - 1;

  //   // struct chunk *current = m_first_chunk;
  //   // while (current) {
  //   //   size_t size = (current->next) ? current->size : m_index;
  //   //   for (int i = 0; i < size; ++i) {
  //   //     acquire_info *a = &current->array[i];
  //   //     acquire_info **bucket = &new_buckets[hash(a->ped)];
        
  //   //     if (bucket_find(bucket, a->ped) != nullptr)
  //   //       continue;
  //   //     bucket_add(bucket, a);
  //   //   }
  //   //   current = current->next;
  //   // }
  //   acquire_info *a = m_first;
  //   while (a) {
  //     assert(a != a->next);
  //     acquire_info **bucket = &new_buckets[hash(a->ped)];
  //     if (bucket_find(bucket, a->ped) == nullptr)
  //       bucket_add(bucket, a);
  //     a = a->next;
  //   }

  //   free(m_buckets);
  //   m_buckets = new_buckets;
  //   // auto end = std::chrono::high_resolution_clock::now();
  //   // m_time += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
  // }

  acquire_info* acquire_container::new_acquire_info()
  {
    //if (m_num_buckets == 0) rehash(RESERVE_SIZE);
    //return static_acquire;
  
    if (t_first_chunk == nullptr) {
      t_first_chunk = t_current_chunk = new chunk();
      assert(t_first_chunk);
      t_first_chunk->size = RESERVE_SIZE;
      t_first_chunk->next = nullptr;
      t_first_chunk->array = (acquire_info*)
        malloc(RESERVE_SIZE * sizeof(acquire_info));
      assert(t_first_chunk->array);
      t_current_chunk = t_first_chunk;
      return &t_first_chunk->array[t_index++];
    }

    // recycle
    // if (t_index > t_current_chunk->size)
    //   t_index = 0;

    acquire_info *a = &t_current_chunk->array[t_index++];


    size_t current_size = t_current_chunk->size;
    if (t_index >= current_size) {

      // g_rr_state->m_output.write((const char*)t_current_chunk->array,
      //                            sizeof(acquire_info)*current_size);
      // free(t_current_chunk);
      // t_current_chunk = new chunk();

      t_index = 0;
      if (current_size < MAX_CHUNK_SIZE)
        current_size *= 2;
      t_current_chunk = t_current_chunk->next = new chunk();
      t_current_chunk->size = current_size;
      t_current_chunk->next = nullptr;
      t_current_chunk->array = (acquire_info*)
        malloc(current_size * sizeof(acquire_info));

      // Touch pages (perf. debugging)
      // int num_pages = (current_size * sizeof(acquire_info)) / 4096;
      // int incr = 4096 / sizeof(acquire_info);
      // for (int i = 0; i < num_pages; ++i)
      //   new(&t_current_chunk->array[i * incr]) acquire_info(0);
    }
    
    return a;
    //return static_acquire;
  }

  acquire_info* acquire_container::add(pedigree_t p, full_pedigree_t full)
  {
    acquire_info *a = new(new_acquire_info()) acquire_info(p, full);
    
    // acquire_info **bucket = &m_buckets[hash(p)];
    // bucket_add(bucket, a);
    
    // if (++m_unique / ((double) m_num_buckets) > 1.0)
    //   rehash(2*m_num_buckets);

    // if (full.length > 0)
    //   m_num_conflicts++;

    return a;
  }

  acquire_info* acquire_container::add(pedigree_t p)
  {

    acquire_info *a = new(new_acquire_info()) acquire_info(p);
    //acquire_info *a = new_acquire_info();
    m_size++;

  
#if PTYPE == PARRAY
    a->full = get_full_pedigree();
#else
    // acquire_info **bucket = &m_buckets[hash(p)];
    // if (bucket_find(bucket, p) != nullptr)
    //   a->full = get_full_pedigree();
    // else {
    //   bucket_add(bucket, a);
    //   if (++m_unique / ((double) m_num_buckets) > 1.0)
    //     rehash(2*m_num_buckets);
    // }

    //    a->full = get_full_pedigree();
    
    size_t num = p & (m_filter_size - 1);
    size_t index = num / bits_per_slot;
    size_t bit = 1 << (num - (index * bits_per_slot));

    uint64_t* filter = (m_filter_size > DEFAULT_FILTER_SIZE)
      ? m_filter
      : (uint64_t*)&m_filter_base;
    // uint64_t* filter = (uint64_t*)&m_filter_base;

    if (filter[index] & bit) {
      a->full = get_full_pedigree();
      //m_num_conflicts++;
    } else
      filter[index] |= bit;

#if RESIZE == 1
    resize filter
    if (m_num_conflicts > __builtin_ctzl(m_filter_size)) {
    if (m_num_conflicts > (m_filter_size >> 2)) {
      if (m_filter_size > DEFAULT_FILTER_SIZE)
        free(m_filter);

      m_resizes++;

      m_filter_size <<= 1;
      m_filter = (uint64_t*) calloc(m_filter_size / bits_per_slot,
                                    sizeof(uint64_t));
      
      acquire_info *a = m_first;
      uint64_t mask = m_filter_size - 1;
      while (a) {
        num = a->ped & mask;
        index = num / bits_per_slot;
        bit = 1 << (num - (index * bits_per_slot));
        m_filter[index] |= bit;
        
        a = a->next;
      }
    }
#endif // resize
#endif
    
    // Just search...
    // acquire_info *current = m_first;
    // while (current) {
    //   if (current->ped == p) {
    //     a->full = get_full_pedigree();
    //     break;
    //   }
    //   current = current->next;
    // }

// #ifdef ACQ_PTR
//     if (m_first == nullptr)
//       m_it = m_first = a;
//     else
//       m_it = m_it->next = a;
// #else
    m_it = m_it->next = a;
// #endif

    // Reverse
    // a->next = (m_it) ? m_it : nullptr;
    // m_it = a;

    assert(m_it->next != m_it);
    return a;
  }

}

