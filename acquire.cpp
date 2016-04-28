#include "acquire.h"
#include <sstream>
#include <cstdio>
#include <fstream>

namespace cilkrr {
	acquire_info::acquire_info() {}

	acquire_info::acquire_info(pedigree_t p)
		: ped(p), next(nullptr), suspended_deque(nullptr) {}
	
	std::ostream& operator<< (std::ostream &out, acquire_info s)
	{
#if PTYPE != PARRAY
		out << std::to_string(s.ped);
#else
		out << "[";
		for (int i = 0; i < s.ped.length; ++i)
			out << s.ped.array[i] << ",";
		out << "]";
#endif
		return out;
	}
	std::ofstream& operator<< (std::ofstream &out, acquire_info s) 
	{
#if PTYPE != PARRAY
		out << std::to_string(s.ped);
#else
		out << "[";
		for (int i = 0; i < s.ped.length; ++i)
			out << s.ped.array[i] << ",";
		out << "]";
#endif
		return out;
	}

	acquire_container::acquire_container(enum mode m)
		: m_mode(m), m_first(&m_sentinel), m_it(&m_sentinel)
	{
#if (IMETHOD == IHASH && defined(USE_STL)) || IMETHOD == IMIXED
		m_container.reserve(RESERVE_SIZE);
#endif

#if IMETHOD == ICHUNKLL || IMETHOD == IMYHASH || IMETHOD == IMIXED
		m_first_chunk = m_current_chunk = new chunk();
		m_current_chunk->size = m_chunk_size;
		m_current_chunk->array = (acquire_info*) malloc(m_chunk_size * sizeof(acquire_info));
		m_current_chunk->next = nullptr;
#endif

#if IMETHOD == IMYHASH
		m_buckets = (acquire_info**) calloc(m_num_buckets, sizeof(acquire_info*));
		assert(m_buckets);
#endif
	}

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
		fprintf(stderr, "find currently disabled because replay is disabled.\n");
		return NULL;
		// assert(m_mode == REPLAY);

		// auto it = m_container.find(p);
		// if (it == m_container.end()) return nullptr;
		// return &it->second;
	}

	void acquire_container::print(std::ofstream& output)
	{
		// struct chunk *c = m_first_chunk;
		// while (c) {
		// 	size_t size = (c == m_current_chunk) ? m_index : c->size;
		// 	for (int i = 0; i < size; ++i)
		// 		output << "\t" << c->array[i] << std::endl;
		// 	c = c->next;
		// }

	}

#if IMETHOD == IMYHASH
	size_t acquire_container::bucket_add(acquire_info** bucket, acquire_info* item)
	{
		item->chain_next = nullptr;

		if (*bucket == nullptr) {
			*bucket = item;
			return 1;
		}

		acquire_info *current = *bucket;
		while (current) {
			if (current->ped == item->ped) {
				// We DON'T increment size here, since we don't need to keep track of item
				return 0;
				break;
			}
			current = current->chain_next;
		}

		assert(current == nullptr);
		item->chain_next = (*bucket)->chain_next;
		(*bucket)->chain_next = item;
		return 1;

	}

	void acquire_container::rehash(size_t new_cap)
	{
		if (new_cap <= m_num_buckets) return;

		acquire_info** new_buckets = (acquire_info**) calloc(new_cap, sizeof(acquire_info*));
		size_t new_size = 0;
		size_t num_old_buckets = m_num_buckets;
		m_num_buckets = new_cap; // I change this so the hash will work

		for (int i = 0; i < num_old_buckets; ++i) {
			acquire_info *current = m_buckets[i];
			acquire_info *next;
			while (current) {
				next = current->chain_next;
				size_t check = bucket_add(&new_buckets[hash(current->ped)], current);
				new_size += check;
				assert(check == 1);
				current = next;
			}
		}

		assert(new_size == m_unique);
		m_num_buckets = new_cap;
		free(m_buckets);
		m_buckets = new_buckets;
	}
#endif // if IMETHOD == IHASH

	acquire_info* acquire_container::add(pedigree_t p)
	{
		acquire_info *a;
#if IMETHOD == ILL
#ifdef USE_STL
		m_container.emplace_back(p);
#else
		a = new acquire_info(p);
		m_it = m_it->next = a;
#endif
#endif

#if IMETHOD == ICHUNKLL || IMETHOD == IMYHASH || IMETHOD == IMIXED
		a = &m_current_chunk->array[m_index++];
		a->ped = p;
		a->next = nullptr;
		a->suspended_deque = nullptr;
		if (m_index >= m_chunk_size) {
			m_index = 0;
			m_chunk_size *= 2;
			m_current_chunk = m_current_chunk->next = new chunk();
			m_current_chunk->size = m_chunk_size;
			m_current_chunk->next = nullptr;
			m_current_chunk->array = (acquire_info*) malloc(m_chunk_size * sizeof(acquire_info));
		}
		m_size++;
#endif

#if IMETHOD == IHASH
#ifdef USE_STL
		auto res = m_container.emplace(p, p);
		a = &res->second;
		m_it = m_it->next = a;

#ifdef CONFLICT_CHECK
		if (m_container.count(p) > 1) {
			a->actual = get_full_pedigree();
			m_num_conflicts++;
		}
#endif // CONFLICT_CHECK

#else // Not USE_STL
		acquire_info **bucket = &m_buckets[hash(p)];
		size_t check = bucket_add(bucket, a);
#ifdef CONFLICT_CHECK
		if (check == 0) {
			a->actual = get_full_pedigree();
			m_num_conflicts++;
		} else {
			a->actual.length = 0;
			a->actual.array = nullptr;
		}
#endif // CONFLICT_CHECK
		m_unique += check;

		if (m_unique / ((double) m_num_buckets) > 1.0)
			rehash(2*m_num_buckets);
#endif // USE_STL
#endif // IHASH

#if IMETHOD == IMIXED
		auto res = m_container.insert(p);
		#ifdef CONFLICT_CHECK
		if (res.second) { // successfully inserted
			a->actual.length = 0;
			a->actual.array = nullptr;
		} else {
			a->actual = get_full_pedigree();
			m_num_conflicts++;
		}
#endif // CONFLICT_CHECK
#endif // IMIXED

		return a;
	}

}
