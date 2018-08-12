/* AUTORIGHTS
Copyright (C) 2007 Princeton University
Copyright (C) 2010 Christian Fensch

TBB version of ferret written by Christian Fensch.

Ferret Toolkit is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include <stdio.h>
#include "cilk/cilk.h"
#include "cilk/cilk_api.h"
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/cass.h"
#include "../include/cass_timer.h"
#include "../image/image.h"

#include "ferret-cilk-future.h"

#define DEFAULT_DEPTH	25

#include <list>

using cilk::future;
using std::list;

static FILE *fout;
static int top_K = 10;
static const char *extra_params = (const char *)"-L 8 - T 20";

static cass_table_t *table;
static cass_table_t *query_table;

static int vec_dist_id = 0;
static int vecset_dist_id = 0;

struct load_data {
	int width, height;
	char *name;
	unsigned char *HSV, *RGB;
};

struct seg_data {
	int width, height, nrgn;
	char *name;
	unsigned char *mask;
	unsigned char *HSV;
};

struct extract_data {
	cass_dataset_t ds;
	char *name;
};

struct vec_query_data {
	char *name;
	cass_dataset_t *ds;
	cass_result_t result;
};

struct rank_data {
	char *name;
	cass_dataset_t *ds;
	cass_result_t result;
};


struct all_data {
	union {
		struct load_data      load;
		struct rank_data      rank;
	} first;
	union {
		struct seg_data       seg;
		struct vec_query_data vec;
	} second;
	struct extract_data extract;
};


/* ------- The Helper Functions ------- */
static int cnt_enqueue;
static int cnt_dequeue;

/* the whole path to the file */
struct all_data *file_helper (const char *file) {

	int r;
	struct all_data *data;

    //fprintf(stderr, "name: %s\n", file);
	data = (struct all_data *)malloc(sizeof(struct all_data));
	assert(data != NULL);

	data->first.load.name = strdup(file);

	r = image_read_rgb_hsv(file,
			       &data->first.load.width,
			       &data->first.load.height,
			       &data->first.load.RGB,
			       &data->first.load.HSV);
	assert(r == 0);

	cnt_enqueue++;

	return data;
}

/* ------ The Stages ------ */


filter_load::filter_load(const char * dir) {

	m_path[0] = 0;
	
	if (strcmp(dir, ".") == 0) {
		m_single_file = NULL;
		push_dir(".");
	}
	else if (strcmp(dir, "..") == 0) {
		m_single_file = NULL;
	}
	else {
		int ret;
		struct stat st;
		
		ret = stat(dir, &st);
		if (ret != 0) {
			perror("Error:");
			m_single_file = NULL;
		}
		if (S_ISREG(st.st_mode))
			m_single_file = dir;
		else if (S_ISDIR(st.st_mode)) {
			m_single_file = NULL;
			push_dir(dir);
		}
	}
}

void filter_load::push_dir(const char * dir) {
	int path_len = strlen(m_path);
	DIR *pd = NULL;
	
	strcat(m_path, dir);
	pd = opendir(m_path);
	if (pd != NULL) {
		strcat(m_path, "/");
		m_dir_stack.push(pd);
		m_path_stack.push(path_len);
	} else {
		m_path[path_len] = 0;
	}
}

void *filter_load::operator()( void* item ) {

	if(m_single_file) {
		struct all_data *ret;
		ret = file_helper(m_single_file);
		m_single_file = NULL;
		return ret;
	}

	if(m_dir_stack.empty())
		return NULL;

	for(;;) {
		DIR *pd = m_dir_stack.top();
		struct dirent *ent = NULL;
		int res = 0;
		struct stat st;
		int path_len = strlen(m_path);

		ent = readdir(pd);
		if (ent == NULL) {
			closedir(pd);
			m_path[m_path_stack.top()] = 0;
			m_path_stack.pop();
			m_dir_stack.pop();
			if(m_dir_stack.empty())
				return NULL;
		}
		
		if((ent->d_name[0] == '.') &&
		   ((ent->d_name[1] == 0) || ((ent->d_name[1] == '.') && 
					      (ent->d_name[2] == 0)) ) )
			continue;
		
		strcat(m_path, ent->d_name);
		res = stat(m_path, &st);
		if (res != 0) {
			perror("Error:");
			return NULL;
		}
		if (S_ISREG(st.st_mode)) {
			struct all_data *ret;
			ret = file_helper(m_path);
			m_path[path_len]=0;
			return ret;
		} else if (S_ISDIR(st.st_mode)) {
			m_path[path_len]=0;
			push_dir(ent->d_name);
		} else {
			m_path[path_len]=0;
        }
	}
}


filter_seg::filter_seg() {}

void *filter_seg::operator()( void* item ) {
	struct all_data *data = (struct all_data*)item;
	
	data->second.seg.name = data->first.load.name;

	data->second.seg.width = data->first.load.width;
	data->second.seg.height = data->first.load.height;
	data->second.seg.HSV = data->first.load.HSV;
	image_segment(&data->second.seg.mask,
		      &data->second.seg.nrgn,
		      data->first.load.RGB,
		      data->first.load.width,
		      data->first.load.height);

	free(data->first.load.RGB);
	return item;
}


filter_extract::filter_extract() {}

void *filter_extract::operator()( future<void*>* item ) {
    assert(item != NULL && "filter extract");
	struct all_data *data = (struct all_data *)cilk_future_get(item);
    delete item;
    //delete item;

	data->extract.name = data->second.seg.name;

	image_extract_helper(data->second.seg.HSV,
			     data->second.seg.mask,
			     data->second.seg.width,
			     data->second.seg.height,
			     data->second.seg.nrgn,
			     &data->extract.ds);

	free(data->second.seg.mask);
	free(data->second.seg.HSV);

	return (void*)data;
}


filter_vec::filter_vec() {}

void *filter_vec::operator()(future<void*>* item) {
    assert(item != NULL && "filter vec");
	struct all_data *data = (struct all_data *) cilk_future_get(item);
    delete item;
    //delete item;
	cass_query_t query;

	data->second.vec.name = data->extract.name;

	memset(&query, 0, sizeof query);
	query.flags = CASS_RESULT_LISTS | CASS_RESULT_USERMEM;

	data->second.vec.ds = query.dataset = &data->extract.ds;
	query.vecset_id = 0;

	query.vec_dist_id = vec_dist_id;

	query.vecset_dist_id = vecset_dist_id;

	query.topk = 2*top_K;

	query.extra_params = extra_params;

	cass_result_alloc_list(&data->second.vec.result,
			       data->second.vec.ds->vecset[0].num_regions,
			       query.topk);

	cass_table_query(table, &query, &data->second.vec.result);

	return (void*)data;
}


filter_rank::filter_rank() {}

void *filter_rank::operator()(future<void*>* item) {
	
	cass_result_t *candidate;
	cass_query_t query;

    assert(item != NULL && "filter rank");
	struct all_data *data = (struct all_data*) cilk_future_get(item);
    delete item;
    //delete item;
	data->first.rank.name = data->second.vec.name;

	query.flags = CASS_RESULT_LIST | CASS_RESULT_USERMEM | CASS_RESULT_SORT;
	query.dataset = data->second.vec.ds;
	query.vecset_id = 0;

	query.vec_dist_id = vec_dist_id;

	query.vecset_dist_id = vecset_dist_id;

	query.topk = top_K;

	query.extra_params = NULL;

	candidate = cass_result_merge_lists(&data->second.vec.result,
					    (cass_dataset_t *)query_table->__private,
					    0);
	query.candidate = candidate;

	cass_result_alloc_list(&data->first.rank.result,
			       0, top_K);
	cass_table_query(query_table, &query,
			 &data->first.rank.result);

	cass_result_free(&data->second.vec.result);
	cass_result_free(candidate);
	free(candidate);
	cass_dataset_release(data->second.vec.ds);

	return (void*)data;
}


filter_out::filter_out() {}

void filter_out::operator()(future<void>* prev, future<void*>* item) {
    if (prev) {
        cilk_future_get(prev);
        delete prev;
    }
    assert(item != NULL && "filter out");
	struct all_data *data = (struct all_data *) cilk_future_get(item);
    //delete item;
	
	fprintf(fout, "%s", data->first.rank.name);

	ARRAY_BEGIN_FOREACH(data->first.rank.result.u.list, cass_list_entry_t p) {
		char *obj = NULL;
		if (p.dist == HUGE) continue;
		cass_map_id_to_dataobj(query_table->map, p.id, &obj);
		assert(obj != NULL);
		fprintf(fout, "\t%s:%g", obj, p.dist);
	} ARRAY_END_FOREACH;

	fprintf(fout, "\n");

	cass_result_free(&data->first.rank.result);
	free(data->first.rank.name);
	free(data);

	cnt_dequeue++;
}

void* s2(filter_seg& seg, void* item) {
    //printf("In s2\n");
    return seg(item);
}

void __attribute__((noinline)) s2_helper(cilk::future<void*> *fut, filter_seg& seg, void* item) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    void *__cilkrts_deque = fut->put(s2(seg, item));
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

void* s3(filter_extract& ext, future<void*>* item) {
    //printf("In s3\n");
    return ext(item);
}

void __attribute__((noinline)) s3_helper(cilk::future<void*> *fut, filter_extract& ext, cilk::future<void*>* item) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    void *__cilkrts_deque = fut->put(s3(ext, item));
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

void* s4(filter_vec& vec, future<void*>* item) {
    //printf("In s4\n");
    return vec(item);
}

void __attribute__((noinline)) s4_helper(cilk::future<void*> *fut, filter_vec& vec, cilk::future<void*>* item) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    void *__cilkrts_deque = fut->put(s4(vec, item));
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

void* s5(filter_rank& rank, future<void*>* item) {
    //printf("In s5\n");
    return rank(item);
}

void __attribute__((noinline)) s5_helper(cilk::future<void*> *fut, filter_rank& rank, cilk::future<void*>* item) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    void *__cilkrts_deque = fut->put(s5(rank, item));
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

void s6(filter_out& out, future<void>* prev, future<void*>* item) {
    //printf("In s6\n");
    out(prev, item);
    //delete prev;
    //delete item;
}

void __attribute__((noinline)) s6_helper(cilk::future<void> *fut, filter_out& out, cilk::future<void>* prev, cilk::future<void*>* item) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    s6(out, prev, item);
    void *__cilkrts_deque = fut->put();
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

static volatile int done = 0;

int my_fancy_wrapper(int argc, char *argv[]) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);

    future<void>* prev = NULL; 
    printf("In fancy_wrapper\n");
    char *db_dir = NULL;
    const char *table_name = NULL;
    const char *query_dir = NULL;
    const char *output_path = NULL;
    cass_env_t *env;

    int depth = 1;
    int nthreads;
    const char* nthread_string;

    stimer_t tmr;

    int ret, i;

    if (argc < 8) {
	printf("%s <database> <table> <query dir> <top K> <n> <out> <depth>\n",
               argv[0]);
	return 0;
    }

    db_dir = argv[1];
    table_name = argv[2];
    query_dir = argv[3];
    top_K = atoi(argv[4]);

    nthreads = atoi(argv[5]);
    nthread_string = argv[5];
    output_path = argv[6];
    depth = atoi(argv[7]);

    fout = fopen(output_path, "w");
    assert(fout != NULL);

    cass_init();

    ret = cass_env_open(&env, db_dir, 0);
    //fprintf(stderr, "approach the env open\n");
    if (ret != 0) { printf("ERROR: %s\n", cass_strerror(ret)); return 0; }
    //fprintf(stderr, "pass the env open\n");
    vec_dist_id = cass_reg_lookup(&env->vec_dist, "L2_float");
    assert(vec_dist_id >= 0);
 
    vecset_dist_id = cass_reg_lookup(&env->vecset_dist, "emd");
    assert(vecset_dist_id >= 0);

    i = cass_reg_lookup(&env->table, table_name);

    table = query_table = (cass_table_t *) cass_reg_get(&env->table, i);
    i = table->parent_id;
    if (i >= 0) {
	query_table = (cass_table_t *)cass_reg_get(&env->table, i);
    }

    if (query_table != table) cass_table_load(query_table);

    cass_map_load(query_table->map);
    cass_table_load(table);
    image_init(argv[0]);

    stimer_tick(&tmr);
    
    filter_load    my_load_filter(query_dir);
    filter_seg     my_seg_filter;
    filter_extract my_extract_filter;
    filter_vec     my_vec_filter;
    filter_rank    my_rank_filter;
    filter_out     my_out_filter;
    cnt_enqueue = cnt_dequeue = 0;

    // Set number of threads.
    //int code = __cilkrts_set_param("nworkers", nthread_string);
    //assert(0 == code);
    cilk_fiber *initial_fiber = cilk_fiber_get_current_fiber();
    
    for (void *chunk = my_load_filter(NULL); chunk != NULL; chunk = my_load_filter(NULL)) {

        future<void*>* stage2 = new cilk::future<void*>();
        sf.flags |= CILK_FRAME_FUTURE_PARENT;
        if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
            char *new_sp = __cilkrts_switch_fibers();
            char *old_sp = NULL;

            __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));
            __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

            s2_helper(stage2, my_seg_filter, chunk);

            __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));
            __cilkrts_switch_fibers_back(initial_fiber);
        }
        cilk_fiber_do_post_switch_actions(initial_fiber);
        sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);

        future<void*>* stage3 = new cilk::future<void*>();
        sf.flags |= CILK_FRAME_FUTURE_PARENT;
        if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
            char *new_sp = __cilkrts_switch_fibers();
            char *old_sp = NULL;

            __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));
            __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

            s3_helper(stage3, my_extract_filter, stage2);

            __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));
            __cilkrts_switch_fibers_back(initial_fiber);
        }
        cilk_fiber_do_post_switch_actions(initial_fiber);
        sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);

        future<void*>* stage4 = new cilk::future<void*>();
        sf.flags |= CILK_FRAME_FUTURE_PARENT;
        if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
            char *new_sp = __cilkrts_switch_fibers();
            char *old_sp = NULL;

            __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));
            __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

            s4_helper(stage4, my_vec_filter, stage3);

            __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));
            __cilkrts_switch_fibers_back(initial_fiber);
        }
        cilk_fiber_do_post_switch_actions(initial_fiber);
        sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);

        future<void*> *stage5 = new cilk::future<void*>();
        sf.flags |= CILK_FRAME_FUTURE_PARENT;
        if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
            char *new_sp = __cilkrts_switch_fibers();
            char *old_sp = NULL;

            __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));
            __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

            s5_helper(stage5, my_rank_filter, stage4);

            __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));
            __cilkrts_switch_fibers_back(initial_fiber);
        }
        cilk_fiber_do_post_switch_actions(initial_fiber);
        sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);


        future<void> *stage6 = new cilk::future<void>();
        sf.flags |= CILK_FRAME_FUTURE_PARENT;
        if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
            char *new_sp = __cilkrts_switch_fibers();
            char *old_sp = NULL;

            __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));
            __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

            s6_helper(stage6, my_out_filter, prev, stage5);

            __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));
            __cilkrts_switch_fibers_back(initial_fiber);
        }
        cilk_fiber_do_post_switch_actions(initial_fiber);
        sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);

        prev = stage6;
    }
    cilk_future_get(prev);
    delete prev;
    
    
    // XXX This is where the old ROI timing ends 
    //    ferret_pipeline.clear();

    stimer_tuck(&tmr, "QUERY TIME");

    ret = cass_env_close(env, 0);
    if (ret != 0) { printf("ERROR: %s\n", cass_strerror(ret)); return 0; }

    cass_cleanup();
    image_cleanup();
    fclose(fout);
    done = 1;

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

    return 0;
}

int __attribute__((noinline)) run(int argc, char *argv[]) {
    int result;
    result = my_fancy_wrapper(argc, argv);

    return result;
}

int main(int argc, char *argv[]) {
    __cilkrts_set_param("local stacks", "128");
    __cilkrts_set_param("global stacks", "128");


    int result = run(argc, argv);
    return result;
}

