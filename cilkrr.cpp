#include <iostream>
#include <fstream>
#include <cstdlib> // getenv
#include <limits>

#include "cilkrr.h"
#include "syncstream.h"
#include <internal/abi.h>

namespace cilkrr {

	acquire_info::acquire_info()
	{
		ped = get_pedigree();
		worker_id = __cilkrts_get_worker_number();
		suspended_deque = nullptr;
	}

	acquire_info::acquire_info(std::string s)
	{
		ped = s;
		worker_id = -1;
		suspended_deque = nullptr;
	}

	pedigree_t get_pedigree()
	{
		const __cilkrts_pedigree tmp = __cilkrts_get_pedigree();
		const __cilkrts_pedigree *current = &tmp;
		
		pedigree_t p = "]";
		while (current) {
			/// @todo Prepending is probably not very performant...
			p = std::to_string(current->rank) + "," + p;
			current = current->parent;
		}
		p = "[" + p;
		return p;
	}

	std::ostream& operator<< (std::ostream &out, acquire_info s)
	{
		out << "w: " << s.worker_id << "; " << s.ped;
		return out;
	}

	state *g_rr_state = nullptr;
	
	state::state() : m_size(0)
	{
		char *env;
			
		m_filename = ".cilkrecord";
		env = std::getenv("CILKRR_FILE");
		if (env) m_filename = env;

		env = std::getenv("CILKRR_MODE");
		if (env) {
			std::string mode = env;
			if (mode == "record") {
				m_mode = RECORD;
			} else if (mode == "replay") {
				m_mode = REPLAY;

				// Read from file
				std::ifstream input;
				std::string line;
				size_t size;
				input.open(m_filename);
				
				input >> size;

				for (int i = 0; i < size; ++i) {
					while (input.peek() != '}') {
						// Get rid of beginning
						input.ignore(std::numeric_limits<std::streamsize>::max(), '[');

						getline(input, line);
						m_all_acquires.push_back(new std::list<acquire_info>);
						m_all_acquires[i]->push_back(acquire_info("[" + line));
					}
				}
				
				input.close();
			} else {
				m_mode = NONE;
			}
		}
	}

	state::~state()
	{
		if (m_mode != RECORD) return;
		std::ofstream output;
		output.open(m_filename);
		
		int i = 0;
		output << m_all_acquires.size() << std::endl;
		for (auto it = m_all_acquires.cbegin(); it != m_all_acquires.cend(); ++it) {
			output << "{" << i << ":" << std::endl;
			for (auto acq = (*it)->cbegin(); acq != (*it)->cend(); ++acq) {
				output << "\t" << *acq << std::endl;
			}
			output << "}" << std::endl;
			++i;
		}
		output.close();
	}

	size_t state::add_mutex()
	{
		m_all_acquires.push_back(new std::list<acquire_info>);
		return m_size++;
	}

	std::list<acquire_info>* state::get_mutex(size_t index)
	{
		return m_all_acquires[index];
	}

	size_t state::remove_mutex(size_t index)
	{
		return --m_size;
	}

	void suspend(std::list<acquire_info> * acquires, pedigree_t p)
	{
		cilkrr::sout << "Suspend at: " << p << cilkrr::endl;

		/// @todo use a hash table so we don't have to iterate through
		acquire_info* a = nullptr;
		for (auto it = acquires->begin(); it != acquires->end(); ++it) {
			if (it->ped == p) {
				a = &(*it);
				break;
			}
		}
		assert(a != nullptr);

		// We actually don't even need to spawn, because in
		//		__cilkrts_suspend_deque I suspend the current fiber and
		//		resume the scheduling fiber, which implicitly does a setjmp.
		a->suspended_deque = __cilkrts_get_deque();
		__cilkrts_suspend_deque();
		cilkrr::sout << "Resume at: " << get_pedigree() << cilkrr::endl;
	}

}
