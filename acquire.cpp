#include "cilkrr.h"
#include <sstream>
#include <cstdio>
#include <fstream>

namespace cilkrr {
  acquire_info::acquire_info() {}

  acquire_info::acquire_info(pedigree_t p)
    : ped(p), next(nullptr), suspended_deque(nullptr) {}

#if PTYPE != PARRAY
	acquire_info::acquire_info(pedigree_t p, full_pedigree_t full)
		: ped(p), next(nullptr), suspended_deque(nullptr)
	{
		actual = full;
	}
#endif

  std::string acquire_info::array_str() // length must be > 0
  {
#if PTYPE == PARRAY
    pedigree_t p = ped;
#else
    full_pedigree_t p = actual;
#endif
    std::string s = "[";
    for (int i = 0; i < p.length; ++i)
      s += std::to_string(p.array[i]) + ",";
    s += "]";
    return s;
  }

  // We don't really care about speed here, maybe reeval later.
  std::string acquire_info::str()
  {
#if PTYPE == PARRAY
    return array_str();
#else
    std::string s = std::to_string(ped);
    if (actual.length > 0)
      s += array_str();
    return s;
#endif
  }
  
  std::ostream& operator<< (std::ostream &out, acquire_info s)
  {
    out << s.str();
    return out;
  }

  acquire_container::acquire_container(enum mode m)
    : m_mode(m)
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
  size_t acquire_container::hash(pedigree_t k)
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

  /// @todo{Refactor so that this is also used in RECORD, in place of
  /// add_acquire} Note that this will require synchronizing access to
  /// the container, since multiple workers will try to allocate
  /// acquire_info structs at the same time.
  acquire_info* acquire_container::find(const pedigree_t& p)
  {
    assert(m_mode == REPLAY);
    acquire_info *a = m_buckets[hash(p)];

    int found = 0;
    acquire_info *match = nullptr;
#if PTYPE == PARRAY
    while (a) {
      if (a->ped == p) return a;
      a = a->chain_next;
    }
		goto not_found;
#else
    while (a) {
      if (a->ped == p) {
        if (++found > 1) break;
        else match = a;
      }
      a = a->chain_next;
    }
    full_pedigree_t full;

    if (found == 0) {
      full = get_full_pedigree();
      goto not_found;
    }

    if (found > 1) { // check conflicts
      match = nullptr;
      full = get_full_pedigree();
      a = m_buckets[hash(p)];

      while (a) {
        if (a->ped == p && a->actual == full) {
          match = a;
          break;
        }
        a = a->chain_next;
      }
      if (!match) goto not_found;
    }
#endif
    return match;
  not_found:
    a = new acquire_info(p);
#if PTYPE != PARRAY
    a->actual = full;
#endif
    fprintf(stderr, "Cilkrecord error: can't find pedigree %s\n",
            a->str().c_str());
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
  
  size_t acquire_container::bucket_add(acquire_info** bucket, acquire_info* item)
  {
    item->chain_next = nullptr;

    if (*bucket == nullptr) {
      *bucket = item;
      return 1;
    }

    acquire_info *current = *bucket;
    while (current) {
      if (current->ped == item->ped) {
        // We DON'T increment size here, since we don't need to keep track of item
        return 0;
        break;
      }
      current = current->chain_next;
    }

    assert(current == nullptr);
    item->chain_next = (*bucket)->chain_next;
    (*bucket)->chain_next = item;
    return 1;

  }

  void acquire_container::rehash(size_t new_cap)
  {
    if (new_cap <= m_num_buckets) return;
    fprintf(stderr, "rehashing to %zu\n", new_cap);

    acquire_info** new_buckets = (acquire_info**) calloc(new_cap, sizeof(acquire_info*));
    size_t new_size = 0;
    size_t num_old_buckets = m_num_buckets;
    m_num_buckets = new_cap; // I change this so the hash will work

    for (int i = 0; i < num_old_buckets; ++i) {
      acquire_info *current = m_buckets[i];
      acquire_info *next;
      while (current) {
        next = current->chain_next;
        size_t check = bucket_add(&new_buckets[hash(current->ped)], current);
        new_size += check;
        assert(check == 1);
        current = next;
      }
    }

    assert(new_size == m_unique);
    m_num_buckets = new_cap;
    free(m_buckets);
    m_buckets = new_buckets;
  }

#if PTYPE != PARRAY  
  acquire_info* acquire_container::add(pedigree_t p, full_pedigree_t full)
  {
    acquire_info *a = &m_current_chunk->array[m_index++];
    a->ped = p;
    a->next = nullptr;
    a->suspended_deque = nullptr;

    if (m_index >= m_chunk_size) {
      m_index = 0;
      //m_chunk_size *= 2;
      if (m_chunk_size < 1024) m_chunk_size *= 2;
      m_current_chunk = m_current_chunk->next = new chunk();
      m_current_chunk->size = m_chunk_size;
      m_current_chunk->next = nullptr;
      m_current_chunk->array = (acquire_info*) malloc(m_chunk_size * sizeof(acquire_info));
    }
    m_size++;

    acquire_info **bucket = &m_buckets[hash(p)];
    bucket_add(bucket, a);
    a->actual = full;
    if (m_first == nullptr) m_first = m_it = a;
    else m_it = m_it->next = a;
    return a;
  }
#endif

  acquire_info* acquire_container::add(pedigree_t p)
  {
    acquire_info *a = &m_current_chunk->array[m_index++];
    a->ped = p;
    a->next = nullptr;
    a->suspended_deque = nullptr;
    if (m_index >= m_chunk_size) {
      m_index = 0;
      m_chunk_size *= 2;
      m_current_chunk = m_current_chunk->next = new chunk();
			assert(m_current_chunk);
      m_current_chunk->size = m_chunk_size;
      m_current_chunk->next = nullptr;
      m_current_chunk->array = (acquire_info*) malloc(m_chunk_size * sizeof(acquire_info));
			assert(m_current_chunk->array);
    }
    m_size++;

    acquire_info **bucket = &m_buckets[hash(p)];
    size_t check = bucket_add(bucket, a);


#if PTYPE != PARRAY

		// for debugging
		a->actual = get_full_pedigree();

    // if (check == 0) {
    //   a->actual = get_full_pedigree();
    //   m_num_conflicts++;
    // } else {
    //   a->actual.length = 0;
    //   a->actual.array = nullptr;
    // }
#endif

    m_unique += check;

    if (m_unique / ((double) m_num_buckets) > 1.0)
      rehash(2*m_num_buckets);

    if (m_first == nullptr) m_first = m_it = a;
    else m_it = m_it->next = a;

    return a;
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

}
