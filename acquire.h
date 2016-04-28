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
#ifdef CONFLICT_CHECK
		full_pedigree_t actual;
#endif
		acquire_info *chain_next;
		acquire_info *next; // we no longer need this!
		void *suspended_deque;
		acquire_info();
		acquire_info(pedigree_t p);
	};
	std::ostream& operator<< (std::ostream &out, acquire_info s);
	std::ofstream& operator<< (std::ofstream &out, acquire_info s);

	class acquire_container {
	private:

		acquire_info* m_first;
		acquire_info* m_it;
		acquire_info m_sentinel;

#if IMETHOD == IHASH && defined(USE_STL)
		std::unordered_multimap<pedigree_t, acquire_info, compressed_hasher> m_container;
#elif IMETHOD == IMYHASH
		size_t m_unique = 0;
		size_t m_num_buckets = RESERVE_SIZE;
		size_t m_num_buckets_used;
		acquire_info** m_buckets;
		inline size_t hash(pedigree_t k) { return k % m_num_buckets; }
		size_t bucket_add(acquire_info** bucket, acquire_info* item);
		void rehash(size_t new_cap);
#endif

#if IMETHOD == ILL && defined(USE_STL)
		std::list<acquire_info> m_container;
#endif

#if IMETHOD == IMIXED
		std::unordered_set<pedigree_t, compressed_hasher> m_container;
#endif

#if IMETHOD == ICHUNKLL || IMETHOD == IMYHASH || IMETHOD == IMIXED
		size_t m_size = 0;
		// chunked Linked list
		struct chunk {
			size_t size;
			acquire_info *array;
			struct chunk *next;
		};
		size_t m_index;
		size_t m_chunk_size = RESERVE_SIZE;
		struct chunk* m_first_chunk;
		struct chunk* m_current_chunk;
#endif

		// This is not really necessary (cilkrr_mode()), but maybe is more
		// efficient?
		enum mode m_mode;

	public:
		size_t m_num_conflicts;

		acquire_container() = delete;
		acquire_container(enum mode m);

		acquire_info* begin();
		acquire_info* end();
		acquire_info* current();
		acquire_info* next();
		void reset();
		void print(std::ofstream& output);
		
		acquire_info* add(pedigree_t p);
		acquire_info* find(pedigree_t& p);

	};

}

#endif // ifndef _ACQUIRE_H
