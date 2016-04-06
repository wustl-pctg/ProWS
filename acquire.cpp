#include "acquire.h"
#include <sstream>
#include <cstdio>

namespace cilkrr {

	acquire_info::acquire_info()
		: ped(get_pedigree()), worker_id(__cilkrts_get_worker_number()),
			next(nullptr), suspended_deque(nullptr) {}

	acquire_info::acquire_info(std::string s)
		: ped(s), worker_id(__cilkrts_get_worker_number()),
			next(nullptr), suspended_deque(nullptr) {}
	
	std::ostream& operator<< (std::ostream &out, acquire_info s)
	{
		out << "w: " << s.worker_id << "; " << s.ped;
		return out;
	}

	acquire_container::acquire_container(enum mode m)
		: m_mode(m), m_first(nullptr), m_it(nullptr) { }

	void acquire_container::reset() { m_it = m_first; }
	acquire_info* acquire_container::next()
	{
		if (m_it == end()) return nullptr;
		return m_it = m_it->next;
	}
	acquire_info* acquire_container::current() { return m_it; }
	acquire_info* acquire_container::begin() { return m_first; }
	acquire_info* acquire_container::end() { return nullptr; }

	acquire_info* acquire_container::find(pedigree_t& p)
	{
		assert(m_mode == REPLAY);

		auto it = m_container.find(p);
		if (it == m_container.end()) return nullptr;
		return &it->second;
	}

	acquire_info* acquire_container::add(pedigree_t p)
	{
		auto res = m_container.emplace(p, p);
		if (res.second == false) {
			fprintf(stderr, "Pedigree %s already in container!\n", p.c_str());
			std::abort();
		}
		acquire_info* a = &(*res.first).second;
		if (m_container.size() == 1)
			m_first = m_it = a;
		else
			m_it = m_it->next = a;
		return a;
	}

}
