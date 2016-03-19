#include <iostream>
#include <fstream>
#include <cstdlib> // getenv
#include <limits>

#include "cilkrr.h"
#include "syncstream.h"

#include <internal/abi.h>

namespace cilkrr {

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

	state::state() : m_size(0), m_active_size(0)
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
				m_all_acquires.reserve(size);


				for (int i = 0; i < size; ++i) {

					getline(input, line); // Advance line
					assert(input.good());
					assert(input.peek() == '{');
					input.ignore(1, '{');
					size_t num; input >> num;
					assert(num == i);
					getline(input, line);

					m_all_acquires.push_back(new acquire_container(m_mode));
					acquire_container* cont = m_all_acquires[i];
					

					while (input.peek() != '}') {
						// Get rid of beginning
						input.ignore(std::numeric_limits<std::streamsize>::max(), '[');

						getline(input, line);
						cont->add("[" + line);
					}
					cont->reset();
				}
				
				input.close();
			} else {
				m_mode = NONE;
			}
		}
	}

	state::~state()
	{
		assert(m_active_size == 0);
		if (m_mode != RECORD) return;
		std::ofstream output;
		acquire_container* cont;
		output.open(m_filename);
		
		output << m_size << std::endl;
		for (int i = 0; i < m_size; ++i) {
			output << "{" << i << ":" << std::endl;
			
			cont = m_all_acquires[i];
		 	cont->reset();
			for (auto acq = cont->begin(); acq != cont->end(); acq = cont->next()) {
				output << "\t" << *acq << std::endl;
			}
			output << "}" << std::endl;
		}
		output.close();
	}

	// This may be called multiple times, but it is not thread-safe!
	// The use case is for an algorithm that has consecutive rounds with
	// parallelism (only) within each round.
	void state::resize(size_t n)
	{
		m_size += n;
		/* Unfortunately, this will initialize all the new elements, which
		 * is a waste. As with many STL classes, std::vector treats every
		 * user like a child, so it makes it a pain to avoid this. If it
		 * becomes a bottleneck, we will need some dirty hacks to avoid
		 * it. */
		if (m_mode == RECORD)
			m_all_acquires.resize(m_size);
	}

	size_t state::register_mutex(size_t local_id)
	{
		// We assume resize() was previously called.
		size_t id = m_size - local_id - 1;
		m_active_size++;
		assert(id < m_size);
		if (m_mode == RECORD)
			m_all_acquires[id] = new acquire_container(m_mode);
		return id;
	}

	size_t state::register_mutex()
	{
		size_t id = m_size++;
		m_active_size++;

		if (m_mode == RECORD) {
			m_all_acquires.emplace_back(new acquire_container(m_mode));
			assert(m_size == m_all_acquires.size());
		}
		
		return id;
	}

	acquire_container* state::get_acquires(size_t id)
	{
		assert(id < m_size);
		return m_all_acquires[id];
	}

	size_t state::unregister_mutex(size_t id)
	{
		// This is a bit hacky, but we don't care until the size reaches 0.
		return --m_active_size;
	}

	void reserve_locks(size_t n) { g_rr_state->resize(n); }

	/** Since the order of initialization between compilation units is
			undefined, we want to make sure the global cilkrr state is created
			before everything else and destroyed after everything else. 

			I also need to use a pointer to the global state; otherwise the
			constructor and destructor will be called multiple times. Doing so
			overwrites member values, since some members are constructed to
			default values before the constructor even runs.
	*/
	state *g_rr_state;

	__attribute__((constructor(101))) void cilkrr_init(void)
	{
		cilkrr::g_rr_state = new cilkrr::state();
	}

	__attribute__((destructor(101))) void cilkrr_deinit(void)
	{
		delete cilkrr::g_rr_state;
	}
}



