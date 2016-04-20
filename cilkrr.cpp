#include <iostream>
#include <fstream>
#include <cstdlib> // getenv
#include <limits>

#include "cilkrr.h"

#include <internal/abi.h>

namespace cilkrr {

	size_t num_places(size_t n)
	{
		if (n < 10) return 1;
		if (n < 100) return 2;
		if (n < 1000) return 3;
		if (n < 10000) return 4;
		if (n < 100000) return 5;
		if (n < 1000000) return 6;
		if (n < 10000000) return 7;
		if (n < 100000000) return 8;
		if (n < 1000000000) return 9;
		if (n < 10000000000) return 10;
		if (n < 100000000000) return 11;
		if (n < 1000000000000) return 12;
		// unlikely
		fprintf(stderr, "Very deep parallelism...\n");
		std::abort();
	}

#if PMETHOD == PMDOT
	// 2^64 - 59
	static size_t big_prime = std::numeric_limits<size_t>::max() - 58;
#endif

#if PTYPE == PINT && IMETHOD == IHASHCONFLICT
	full_pedigree_t get_full_pedigree()
	{
		const __cilkrts_pedigree tmp = __cilkrts_get_pedigree();
		const __cilkrts_pedigree *current = &tmp;
		full_pedigree_t p = {0, nullptr};

		// If we're not precomputing the dot product, we /could/ have already obtained this...
		while (current) {
			p.length++;
			current = current->parent;
		}

		p.array = (uint64_t*) malloc(sizeof(uint64_t) * p.length);
		size_t ind = 0;
		// Note that we get this backwards!
		current = &tmp;
		while (current) {
			p.array[ind++] = current->rank;
			current = current->parent;
		}

		return p;
	}
#endif	

	pedigree_t get_pedigree()
	{
		const __cilkrts_pedigree tmp = __cilkrts_get_pedigree();
		const __cilkrts_pedigree *current = &tmp;
		pedigree_t p;

#if PMETHOD == PMNONE
		// do nothing
#elif PMETHOD == PMWALK
		while (current) {
			current = current->parent;
		}
#elif PMETHOD == PMPRE
		p = current->actual;
#elif PMETHOD == PMGET
#  if PTYPE == PSTRING
		size_t nchars = 2; // [ and ] and null
		while (current) {
			nchars += num_places(current->rank) + 1; // for comma
			current = current->parent;
		}
		char* chars = (char*)malloc(sizeof(char) * (nchars+1));
		size_t ind = nchars;
		chars[ind] = '\0';
		chars[--ind] = ']';
		current = &tmp;

		// It's illegal to use itoa in C++, so we're forced to use sprintf
		// which ALWAYS puts a null character at the end.
		while (current) {
			size_t comma = --ind;
			ind -= num_places(current->rank);
			sprintf(&chars[ind], "%lu", current->rank);
			chars[comma] = ',';
			current = current->parent;
		}

		chars[--ind] = '[';
		assert(ind == 0);
		p.assign(chars, nchars);
#  elif PTYPE == PARRAY
		p.length = 0;
		while (current) {
			p.length++;
			current = current->parent;
		}
		p.array = (uint64_t*)malloc(sizeof(uint64_t) * p.length);
		size_t ind = p.length - 1;
		current = &tmp;
		while (current) {
			p.array[ind--] = current->rank;
			current = current->parent;
		}
		// 	//ped += g_rr_state->randvec[ind--] * current->rank;
		// 	ped += g_rr_state->randvec[ind++] * current->rank;
		// 	current = current->parent;
		// }
		// ped %= big_prime;
#  else
#    error "Invalid PTYPE for method PMGET"
#  endif
#elif PMETHOD == PMDOT
		p = 0;
		size_t ind = 0;
		while (current) {
			p += g_rr_state->randvec[ind++] * current->rank;
			current = current->parent;
		}
		p %= big_prime;
#else
#error "Invalid PMETHOD"
#endif
		
		return p;
	}

	state::state() : m_size(0), m_active_size(0)
	{
		char *env;

#if PMETHOD == PMDOT
		srand(0);
		for (int i = 0; i < 256; ++i)
			randvec[i] = rand() % big_prime;
#endif

		// Seed Cilk's pedigree seed
		__cilkrts_set_param("ped seed", "0");

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
				fprintf(stderr, "Replay mode currently disabled.");
				std::abort();

				// Read from file
				// std::ifstream input;
				// std::string line;
				// size_t size;
				// input.open(m_filename);
				
				// input >> size;
				// m_all_acquires.reserve(size);


				// for (int i = 0; i < size; ++i) {

				// 	getline(input, line); // Advance line
				// 	assert(input.good());
				// 	assert(input.peek() == '{');
				// 	input.ignore(1, '{');
				// 	size_t num; input >> num;
				// 	assert(num == i);
				// 	getline(input, line);

				// 	m_all_acquires.push_back(new acquire_container(m_mode));
				// 	acquire_container* cont = m_all_acquires[i];
					

				// 	while (input.peek() != '}') {
				// 		// Get rid of beginning
				// 		input.ignore(std::numeric_limits<std::streamsize>::max(), '[');

				// 		getline(input, line);
				// 		cont->add("[" + line);
				// 		//cont->add(std::stoul(line));
				// 	}
				// 	cont->reset();
				// }
				
				// input.close();
			} else {
				m_mode = NONE;
			}
		}
	}

	state::~state()
	{
		assert(m_active_size == 0);
#if PTYPE == PINT && IMETHOD == IHASHCONFLICT
		fprintf(stderr, "Num conflicts: %zu out of %zu\n",
						m_all_acquires[0]->m_num_conflicts,
						m_all_acquires[0]->m_cont_size);
#endif
		return; // Only for measuring the overhead
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



