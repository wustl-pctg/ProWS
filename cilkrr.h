#include <string>
#include <vector>
#include <list>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

namespace cilkrr {

	typedef std::string pedigree_t;

	class acquire_info {
	public:
		pedigree_t ped;
		int worker_id; // for debugging
		void *suspended_deque;
		acquire_info();
		acquire_info(std::string s);
	};

	std::ostream& operator<< (std::ostream &out, acquire_info s);
	pedigree_t get_pedigree();

	enum mode {
		NONE = 0,
		RECORD,
		REPLAY,
	};

	// The *only* purpose of this class is to control when we output the
	// lock acquires. Normally I would use a program destructor, but
	// m_mutexes might have already been destroyed.
	class state {
	private:
		// We can't guarantee the order of construction/destruction
		// between this state and any of the mutexes, so we store the
		// actual information here.
		std::vector< std::list< acquire_info> * > m_all_acquires;
		std::string m_filename;
		size_t m_size;
		
	public:
		enum mode m_mode;

		state();
		~state();

		size_t add_mutex();
		std::list<acquire_info>* get_mutex(size_t index);
		size_t remove_mutex(size_t index);
	};
	extern state *g_rr_state;

	inline enum mode cilkrr_mode() { return g_rr_state->m_mode;	}

	void suspend(std::list<acquire_info> * acquires, pedigree_t p);
}
