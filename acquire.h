#ifndef _ACQUIRE_H
#define _ACQUIRE_H

#include <list>
#include <unordered_map>

#include "util.h"

namespace cilkrr {

	class acquire_info {
	public:
		pedigree_t ped;
		int worker_id; /// for debugging in record @todo{Remove worker field from acquire_info}
#if PTYPE == PINT && IMETHOD == IHASHCONFLICT
		full_pedigree_t actual;
#endif
		acquire_info *next;
		void *suspended_deque;
		acquire_info();
		acquire_info(pedigree_t p);
	};
	std::ostream& operator<< (std::ostream &out, acquire_info s);

	class acquire_container {
	private:
#if IMETHOD == IHASH || IMETHOD == INONE || IMETHOD == IHASHCONFLICT
#if PTYPE == PSTRING || PTYPE == PNONE
		std::unordered_map<pedigree_t, acquire_info> m_container;
#elif PTYPE == PARRAY
		std::unordered_map<pedigree_t, acquire_info, array_hasher> m_container;
#else // PTYPE == PINT
		std::unordered_multimap<pedigree_t, acquire_info> m_container;
#endif
#elif IMETHOD == ILL
		size_t m_list_size = 0;
		std::list<acquire_info> m_container;
		//std::unordered_map<pedigree_t, acquire_info> m_container;
#else
#error "Invalid IMETHOD"
#endif

		// This is not really necessary (cilkrr_mode()), but maybe is more
		// efficient?
		enum mode m_mode;

		/// @todo{Write a real iterator for acquires.}
		acquire_info* m_first;
		acquire_info* m_it; // "iterator"

	public:
#if PTYPE == PINT && IMETHOD == IHASHCONFLICT
		size_t m_cont_size = 0;
		size_t m_num_conflicts = 0;
#endif
		acquire_container() = delete;
		acquire_container(enum mode m);

		acquire_info* begin();
		acquire_info* end();
		acquire_info* current();
		acquire_info* next();
		void reset();
		
		acquire_info* add(pedigree_t p);
		acquire_info* find(pedigree_t& p);

	};

}

#endif // ifndef _ACQUIRE_H
