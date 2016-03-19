#ifndef _ACQUIRE_H
#define _ACQUIRE_H

#include <list>
#include <unordered_map>

#include "util.h"

namespace cilkrr {

	class acquire_info {
	public:
		pedigree_t ped;
		int worker_id; // for debugging in record
		acquire_info *next;
		void *suspended_deque;
		acquire_info();
		acquire_info(std::string s);
	};
	std::ostream& operator<< (std::ostream &out, acquire_info s);

	class acquire_container {
	private:
		std::unordered_map<std::string, acquire_info> m_container;

		// This is not really necessary (cilkrr_mode()), but maybe is more
		// efficient?
		enum mode m_mode;

		/// @todo{Write a real iterator for acquires.}
		acquire_info* m_first;
		acquire_info* m_it; // "iterator"

	public:
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
