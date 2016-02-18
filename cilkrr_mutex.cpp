#include "cilkrr_mutex.h"
#include <cstdlib>
#include <fstream>

std::ostream& operator<< (std::ostream &out, struct acquire_info_s s)
{
	out << "w: " << s.worker_id << "; " << s.ped;
	//out << s.ped;
	return out;
}


namespace cilkrr {

	enum cilkrr_mode {
		NONE = 0,
		RECORD,
		REPLAY,
	};

	uint64_t g_next_mutex_id = 0;
	enum cilkrr_mode g_mode = NONE;

	// The *only* purpose of this class is to control when we output the
	// lock acquires. Normally I would use a program destructor, but
	// m_mutexes might have already been destroyed.
	class cilkrr_context {
	public:
		std::vector< std::list< acquire_info_t > * > m_context;
		std::string m_filename;

		void info();
		cilkrr_context()
		{
			char *env;
			
			m_filename = ".cilkrecord";
			env = std::getenv("CILKRR_FILE");
			if (env) m_filename = env;

			env = std::getenv("CILKRR_MODE");
			if (env) {
				std::string mode = env;
				if (mode == "record") {
					g_mode = RECORD;
				} if (mode == "replay") {
					g_mode = REPLAY;
					
				}
			}
		}
		
		~cilkrr_context()
		{
			std::cerr << "Destroying global cilkrr structs." << std::endl;
			std::ofstream output;
			output.open(m_filename);
		
			int i = 0;
			for (auto it = m_context.cbegin(); it != m_context.cend(); ++it) {
				std::cerr << "Acquires of mutex " << i << ":" << std::endl;
				output << "{" << i << ":" << std::endl;
				for (auto acq = (*it)->cbegin(); acq != (*it)->cend(); ++acq) {
					std::cerr << "\t" << *acq << std::endl;
					output << "\t" << *acq << std::endl;
				}
				output << "}" << std::endl;
				++i;
			}
			output.close();
		}
	};
	cilkrr_context *g_context = nullptr;

	mutex::mutex()
	{
		// Static initialization order is tricky, but this hack works.
		if (g_context == nullptr)
			g_context = new cilkrr_context();
		
		// You *could* protect this increment with a lock, but we must
		// have deterministic ids anyway, so the lock would be pointless.
		m_id = g_next_mutex_id++;

		m_acquires = new std::list<acquire_info_t>;
		g_context->m_context.push_back(m_acquires);
		std::cerr << "Creating cilkrr mutex: " << this
							<< ", " << m_id << std::endl;

	}

	mutex::~mutex()
	{
		std::cerr << "Destroy cilkrr mutex: " << this << std::endl;

		// Warning: All mutexes must be created before any can be destroyed!
		if (--g_next_mutex_id == 0)
			delete g_context;
	}

	inline void mutex::acquire() { m_owner = __cilkrts_get_tls_worker(); }
	inline void mutex::release() { m_owner = nullptr; m_mutex.unlock(); }

	
	void mutex::lock()
	{
		if (g_mode == REPLAY) return replay_lock();
		m_mutex.lock();
		acquire();
		if (g_mode == RECORD) record_acquire();
	}

	bool mutex::try_lock()
	{
		bool result;
		if (g_mode == REPLAY)
			result = replay_try_lock();
		else
			result = m_mutex.try_lock();
		
		if (result) {
			acquire();
			if (g_mode == RECORD) record_acquire();
		}
	}

	void mutex::unlock()
	{
		release();
		if (g_mode == REPLAY) replay_unlock();
	}

	void mutex::record_acquire()
	{
		//		pedigree_t ped = get_pedigree();
		acquire_info_t a = {get_pedigree(), __cilkrts_get_worker_number()};
		m_acquires->push_back(a);
		__cilkrts_bump_worker_rank();
	}

	void mutex::replay_lock()
	{

	}

	bool mutex::replay_try_lock()
	{
		return m_mutex.try_lock();
	}

	void mutex::replay_unlock()
	{

	}

	pedigree_t mutex::get_pedigree()
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

}
