#ifndef _ACQUIRE_H
#define _ACQUIRE_H

#include <list>
#include <unordered_map>
#include <unordered_set>

#include "util.h"

namespace cilkrr {

	class acquire_info {
	public:
		pedigree_t ped;
#if PTYPE != PARRAY
		full_pedigree_t actual;
#endif
		acquire_info *chain_next;
		acquire_info *next; // Needed for replay only
		void * volatile suspended_deque;
    int checking = 0; // I think this can be per-lock instead
    
		acquire_info();
		acquire_info(pedigree_t p);

		// for debugging
#if PTYPE != PARRAY
		acquire_info(pedigree_t p, full_pedigree_t f);
#endif
		
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
		size_t m_num_buckets = RESERVE_SIZE;
		acquire_info** m_buckets;
		size_t bucket_add(acquire_info** bucket, acquire_info* item);
#if PTYPE == PARRAY
    size_t hash(pedigree_t k);
#else
    inline size_t hash(pedigree_t k) { return k % m_num_buckets; }
#endif

		void rehash(size_t new_cap);

    /// @todo{ It is a bit silly to have an allocator inside each
    // acquire_container. Instead, just have each thread use its own
    // allocator, that way it is shared between locks }
    
    // Chunked linked list that stores the actual acquire_info structs
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

		// This is not really necessary (cilkrr_mode()), but maybe is more
		// efficient?
		enum mode m_mode;

	public:
		size_t m_num_conflicts;

		acquire_container() = delete;
		acquire_container(enum mode m);

    // Approximate memory allocated
    size_t memsize();

		acquire_info* begin();
		acquire_info* end();
		acquire_info* current();
		acquire_info* next();
		void reset();
		void print(std::ofstream& output);
		
		acquire_info* add(pedigree_t p);
#if PTYPE != PARRAY
    acquire_info* add(pedigree_t p, full_pedigree_t full);
#endif
		acquire_info* find(const pedigree_t& p);

	};

}

#endif // ifndef _ACQUIRE_H
