#ifndef _ACQUIRE_H
#define _ACQUIRE_H

#include <chrono> // perf debug timing

#include "util.h"

namespace porr {

	std::string get_pedigree_str();

	class acquire_info {
	public:
		/*const*/ pedigree_t ped;
		full_pedigree_t full = {0, nullptr};
		acquire_info *chain_next = nullptr;
		void * volatile suspended_deque = nullptr;

    // thread local allocator requires this
    acquire_info *next = nullptr;
    
		acquire_info() = delete;
		acquire_info(pedigree_t p);
		acquire_info(pedigree_t p, full_pedigree_t f);

    std::string str();
  private:
    std::string array_str();
	}; //__attribute__((aligned (64)));
	std::ostream& operator<< (std::ostream &out, acquire_info s);

  static constexpr size_t RESERVE_SIZE  = 128;
  static constexpr size_t MAX_CHUNK_SIZE = (1L << 25);

	class acquire_container {
	private:

    // Bloom filter
    // It seems it is never worth it to resize this as we go...
    // I guess at that point there are so many lock acquires you ought
    // to just get the full pedigree? Skeptical of this...
#define RESIZE 0
    static constexpr size_t DEFAULT_FILTER_SIZE = 64;
    static constexpr int bits_per_slot = sizeof(uint64_t) * 8;
    union {
      uint64_t m_filter_base[DEFAULT_FILTER_SIZE / bits_per_slot] = {0};
      uint64_t *m_filter;
      acquire_info** m_table;
      acquire_info* m_start; // For array-based search
    };
    static constexpr size_t m_filter_size = DEFAULT_FILTER_SIZE;
    
#if PTYPE == PARRAY    
    size_t hash(full_pedigree_t k);
#endif
    
    // inline size_t hash(pedigree_t k) { return k % m_num_buckets; }
		// void rehash(size_t new_cap);

    // Chunked linked list that stores the actual acquire_info structs
    acquire_info* new_acquire_info();

	public:
/* #ifdef ACQ_PTR */
/*     acquire_info *m_first = nullptr; */
/* #endif */
    /*union { */ acquire_info *m_it = nullptr;
    size_t m_size = 0; /// Is this necessary?
    /*union { */ size_t m_index = 0;
      
    typedef struct chunk {
      size_t size;
      acquire_info *array;
      struct chunk *next;
    } chunk_t;

    __thread static size_t t_index;
    __thread static struct chunk* t_first_chunk;
    __thread static struct chunk* t_current_chunk;
    

    acquire_container(acquire_container const&) = delete;
    /* acquire_container() {} */

#ifdef ACQ_PTR
    acquire_container() {}
#else
    acquire_container() = delete;
    acquire_container(acquire_info** start_ptr);
#endif

    inline acquire_info* current() { return m_it; }

    // The read of m_it is killing us during replay...
    inline void next() { m_it = m_it->next; m_index++; }
		
		acquire_info* add(pedigree_t p); // for record
    acquire_info* add(pedigree_t p, full_pedigree_t full); // for replay
    // acquire_info* bucket_find(acquire_info **const bucket, const pedigree_t& p);

    acquire_info* find(const pedigree_t& p);
    //acquire_info* find(const pedigree_t& p, const full_pedigree_t& full);

	};

}

#endif // ifndef _ACQUIRE_H
