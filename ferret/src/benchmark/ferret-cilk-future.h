#ifndef _ferret_piper_h_
#define _ferret_piper_h_

#include <sys/types.h>
#include <dirent.h>
#include <stack>

#include "../../../src/future.h"
#include "../../../cilkrtssuspend/include/internal/abi.h"

class cilk_fiber;

void __cilkrts_leave_future_frame(__cilkrts_stack_frame*);

char* __cilkrts_switch_fibers();
void __cilkrts_switch_fibers_back(cilk_fiber*);

extern "C" {
void** cilk_fiber_get_resume_jmpbuf(cilk_fiber*);
cilk_fiber* cilk_fiber_get_current_fiber();
void cilk_fiber_do_post_switch_actions(cilk_fiber*);
void __cilkrts_detach(__cilkrts_stack_frame*);
void __cilkrts_pop_frame(__cilkrts_stack_frame*);
}

class filter_load {
	char m_path[BUFSIZ];
	const char *m_single_file;
	
	std::stack<DIR *> m_dir_stack;
	std::stack<int>   m_path_stack;
	
	private:
		void push_dir(const char * dir);
	
	public:
		filter_load(const char * dir);
		/*override*/void* operator()( void* item );
};


class filter_seg {
	public:
		  filter_seg();
		  /*override*/void* operator()(void* item);
};

class filter_extract {
	public:
		filter_extract();
		/*override*/void* operator()(cilk::future<void*>* item);
};

class filter_vec {
	public:
		filter_vec();
		/*override*/void* operator()(cilk::future<void*>* item);
};

class filter_rank {
	public:
		filter_rank();
		/*override*/void* operator()(cilk::future<void*>* item);
};

class filter_out {
	public:
		filter_out();
		/*override*/void operator()(cilk::future<void>* prev, cilk::future<void*>* item);
};

#endif
