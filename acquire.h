#ifndef _ACQUIRE_H
#define _ACQUIRE_H

#include <list>
#include <unordered_map>
#include <unordered_set>

#include "util.h"

namespace cilkrr {

	std::string get_pedigree_str();

	class acquire_info {
	public:
		const pedigree_t ped;
		full_pedigree_t full;

		acquire_info *chain_next;
		acquire_info *next; // Needed for replay only
		void * volatile suspended_deque;
    
		acquire_info() = delete;
		acquire_info(pedigree_t p);
		acquire_info(pedigree_t p, full_pedigree_t f);

    std::string str();
  private:
    std::string array_str();
	};
	std::ostream& operator<< (std::ostream &out, acquire_info s);

	class acquire_container {
	private:

#define RESERVE_SIZE 4

    // Hash table
		size_t m_unique = 0;
    size_t m_num_conflicts; /// @todo{ Remove either m_num_conflicts
                            /// or m_unique}


		size_t m_num_buckets = RESERVE_SIZE;
    uint64_t m_mask = ((uint64_t)1 << 2) - 1;
		acquire_info** m_buckets;
		void bucket_add(acquire_info** bucket, acquire_info* item);
#if PTYPE == PARRAY
    size_t hash(pedigree_t k);
#else
    inline size_t hash(pedigree_t k) {
      // Alternatively, we can just take the low-order gits of k.
      // Specifically,
      return k & m_mask;
      //   where mask is ((uint64_t)1 << log_2(m_num_buckets)) - 1
      //   and we reset mask during each rehash
      // This only works when m_num_buckets is a power of 2.
      // But testing this only made an insignificant difference.
      /* return k % m_num_buckets; */
    }
#endif

		void rehash(size_t new_cap);

    /// @todo{ It is a bit silly to have an allocator inside each
    // acquire_container. Instead, just have each thread use its own
    // allocator, that way it is shared between locks }
    
    // Chunked linked list that stores the actual acquire_info structs
    acquire_info* new_acquire_info();
		size_t m_size = 0;
		// chunked Linked list
		struct chunk {
			size_t size;
			acquire_info *array;
			struct chunk *next;
		};
		size_t m_index = 0;
		size_t m_chunk_size = RESERVE_SIZE;
		struct chunk* m_first_chunk;
		struct chunk* m_current_chunk;

    acquire_info* m_it = nullptr;
    acquire_info* m_first = nullptr;

	public:
    acquire_container(size_t size = RESERVE_SIZE);

    // Approximate memory allocated
    size_t memsize();
    void stats();

		acquire_info* begin();
		acquire_info* end();
		acquire_info* current();
		acquire_info* next();
		void reset();
		void print(std::ofstream& output);
		
		acquire_info* add(pedigree_t p); // for record
#if PTYPE != PARRAY
    acquire_info* add(pedigree_t p, full_pedigree_t full); // for replay
#endif
		acquire_info* find_first(const pedigree_t& p);
    acquire_info* find(const pedigree_t& p);
    acquire_info* find(const pedigree_t& p, const full_pedigree_t& full);

	};

}

#endif // ifndef _ACQUIRE_H
