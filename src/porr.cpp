#include <iostream>
#include <fstream>
#include <cstdlib> // getenv
#include <algorithm> // count
#include <csignal> // raise
#include <cstring> // memset

#include "porr.h"
#include "spinlock.h"

#include <internal/abi.h>

#ifdef USE_LOCKSTAT
struct spinlock_stat *the_sls_list = NULL;
pthread_spinlock_t the_sls_lock;
int the_sls_setup;
#endif

namespace porr {

  void reserve_locks(size_t n) { g_rr_state->reserve(n); }

  struct ped_chunk {
    size_t size;
    rank_t *array;
    //struct ped_chunk *next;
  };

  struct ped_chunk* alloc_chunk(size_t size)
  {
    struct ped_chunk *chunk = new ped_chunk();
    chunk->size = size;
    chunk->array = (rank_t*) malloc(sizeof(rank_t) * size);
    //chunk->next = nullptr;
    return chunk;
  }

  rank_t* allocate_full_ped_array(size_t len)
  {
    __thread static struct ped_chunk* current_chunk = nullptr;
    __thread static size_t index = 0;

    if (!current_chunk) current_chunk = alloc_chunk(256);

    if (index + len >= current_chunk->size) {
      current_chunk = alloc_chunk(current_chunk->size * 2);
      index = 0;
    }

    rank_t* ped = &current_chunk->array[index];
    index += len;
    return ped;
  }

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

    //p.array = (uint64_t*) malloc(sizeof(uint64_t) * p.length);
    p.array = allocate_full_ped_array(p.length);
    size_t ind = p.length - 1;
    // Note that we get this backwards!
    current = &tmp;
    while (current) {
      p.array[ind--] = current->rank;
      current = current->parent;
    }

    return p;
  }

  pedigree_t get_pedigree()
  {
    const __cilkrts_pedigree tmp = __cilkrts_get_pedigree();
    const __cilkrts_pedigree* current = &tmp;
    pedigree_t p;

#if PTYPE == PPRE
    p = current->actual;
#elif PTYPE == PDOT
    pedigree_t actual = current->actual;
    size_t len = 0;
    const __cilkrts_pedigree* curr = current;
    while (curr) {
      len++;
      curr = curr->parent;
    }

    p = 1;
    size_t ind = len - 1;
    while (current) {
      p += g_rr_state->randvec[ind--] * current->rank;
      current = current->parent;
    }
    p %= big_prime;
#endif
    
    return p;
  }

  state::state() : m_mode(NONE)
  {
    char *env;
    g_rr_state = this;
    m_first_chunk = nullptr;
    //m_acquires = nullptr;

#if PTYPE != PPRE
    srand(0);
    for (int i = 0; i < 256; ++i)
      randvec[i] = rand() % big_prime;
    //randvec[i] = i+1;
#endif

    // Seed Cilk's pedigree seed
    __cilkrts_set_param("ped seed", "0");

    m_filename = ".cilkrecord";
    env = std::getenv("PORR_FILE");
    if (env) m_filename = env;


    env = std::getenv("PORR_MODE");
    if (!env) return;
    
    std::string mode = env;
    if (mode == "record") {
      m_mode = RECORD;
      //m_output.open(m_filename);
      return;
    } else if (mode != "replay") {
      return;
    }

    // replay
    m_mode = REPLAY;

    // Read from file
    std::ifstream input;
    std::string line;
    input.open(m_filename);
        
    input >> m_num_locks;
    input >> m_num_acquires;

    m_current_chunk = new achunk<CHUNK_TYPE>(m_num_locks);

    // I allocate an extra to store the size of each...very hackish
    // @TODO{ Remove fake acquire_info used to store size during replay}
    m_acquires = (acquire_info*)
      malloc((m_num_acquires+m_num_locks) * sizeof(acquire_info));
    assert(m_acquires);

    m_tables = (acquire_info**)
      calloc(m_num_acquires, sizeof(acquire_info*));

    size_t global_index = 0;
    for (int i = 0; i < m_num_locks; ++i) {

      getline(input, line); // Advance line
      assert(input.good());
      assert(input.peek() == '{');
      // input.ignore(1, '{');
      // finish reading line
      getline(input, line);

      m_current_chunk->data[i] = nullptr;
      
      size_t size = 0;
      size_t start = global_index++;
      m_current_chunk->data[i] = &m_acquires[start];

      while (input.peek() != '}') {
        size++;
        pedigree_t p;
        full_pedigree_t full = {0, nullptr};

        // Get rid of beginning
        input.ignore(std::numeric_limits<std::streamsize>::max(), '\t');

        getline(input, line);
        size_t ind = line.find('[');
        
        p = std::stoul(line.substr(0,ind)); // read in compressed pedigree
        if (ind != std::string::npos) { // found full pedigree
          full.length = std::count(line.begin(), line.end(), ',');
          full.array = (rank_t*) malloc(full.length * sizeof(rank_t));
          for (int j = 0; j < full.length; ++j) {
            ind++;
            size_t next_ind = line.find(',', ind);
            full.array[j] = std::stoul(line.substr(ind, next_ind - ind));
            ind = next_ind;
          }
        }
        
        acquire_info *a = new (&m_acquires[global_index]) acquire_info(p, full);
        // if (m_current_chunk->data[i] == nullptr)
        //   m_current_chunk->data[i] = a;
        // else
        //m_acquires[global_index-1].next = a;

        m_acquires[global_index-1].next = a;
        global_index++;
        //cont->add(p, full);
      }
      acquire_info* debug = m_acquires;
      size_t table_begin = global_index-i-1-size;
      m_acquires[start].full.length = size;
      m_acquires[start].full.array = (rank_t*) &m_tables[table_begin];
      for (int j = 0; j < size; ++j) {
        acquire_info *a = &m_acquires[global_index-1-j];
        uint64_t h = a->ped % size;
        a->chain_next = m_tables[table_begin+h];
        m_tables[table_begin+h] = a;
      }

    }
        
    input.close();
  }

  size_t state::num_digits(size_t num)
  {
    size_t digits = 0;
    while (num) {
      num /= 10;
      digits++;
    }
    return digits;
  }

  state::~state()
  {
    if (m_mode != RECORD) return;

    fprintf(stderr, "Base lock size: %lu\n", sizeof(pthread_spinlock_t));
    fprintf(stderr, "PORR spinlock size: %lu\n", sizeof(porr::spinlock));
    fprintf(stderr, "Acquire container size: %lu\n", sizeof(acquire_container));
    fprintf(stderr, "Acquire info size: %lu\n", sizeof(acquire_info));
    fprintf(stderr, "Locks: %lu\n", m_num_locks);
    
#if STAGE < 4
    return;
#endif

    std::ofstream output;
    output.open(m_filename);

    // Reserve space for writing out the number of locks and acquires
    // This is pretty hackish....
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 20; ++j)
        output << " ";
      output << std::endl;
    }
    size_t global_index = 0;
    size_t num_acquires = 0;

    achunk<CHUNK_TYPE> *c = m_first_chunk;
    while (c) {
      for (int i = 0; i < c->size; ++i) {

        // @TODO{Figure out how to write the number of acquires for
        // this lock...but is this necessary?}
        // We could reserve space, save the position, then after we're
        // done seek back to it and write the size...
        output << "{" << global_index++ << std::endl;

        // First one is fake
        acquire_info *a = c->data[i]->next;

        while (a) {
          num_acquires++;
          output << '\t' << *a << std::endl;
          a = a->next;
        }
        output << "}" << std::endl;
      }
      c = c->next;
    }

    // output << "---- Stats ----" << std::endl;
    // output << "Conflicts: " << num_conflicts << std::endl;


    // Go back to beginning and write number of locks and acquires
    output.seekp(0, std::ios_base::beg);
    output << global_index;
    for (int i = 0; i < 20-num_digits(global_index); ++i)
      output << " ";
    output << std::endl;
    output << num_acquires;
    for (int i = 0; i < 20-num_digits(num_acquires); ++i)
      output << " ";
    output << std::endl;


    output.close();
  }

    // This may be called multiple times, but it is not thread-safe!
  // The use case is for an algorithm that has consecutive rounds with
  // parallelism (only) within each round.
  void state::reserve(size_t n)
  {
    if (m_mode == NONE) return;
    else if (m_mode == REPLAY) {
      m_curr_base_index = m_next_base_index;
      m_next_base_index = m_curr_base_index + n;
      return;
    }

    auto c = new achunk<CHUNK_TYPE>(n);
    if (!m_first_chunk)
      m_first_chunk = m_current_chunk = c;
    else
      m_current_chunk = m_current_chunk->next = c;

    m_num_locks += n;
  }

  CHUNK_TYPE* state::register_spinlock()
  {
    state::reserve(1);
    return register_spinlock(0);
  }

  CHUNK_TYPE* state::register_spinlock(size_t id)
  {
    if (m_mode == NONE) return nullptr;
    return &m_current_chunk->data[id + m_curr_base_index];
  }

  void state::unregister_spinlock(size_t size)
  {
    //m_num_acquires += size;
    // if (m_mode == RECORD)
    //   __sync_fetch_and_add(&m_num_acquires, size);
  }

  /** Since the order of initialization between compilation units is
      undefined, we want to make sure the global porr state is created
      before everything else and destroyed after everything else. 

      I also need to use a pointer to the global state; otherwise the
      constructor and destructor will be called multiple times. Doing so
      overwrites member values, since some members are constructed to
      default values before the constructor even runs.
  */
  state *g_rr_state;

#if STATS > 0
  long long papi_counts[NUM_PAPI_EVENTS];
  int papi_events[NUM_PAPI_EVENTS] = {PAPI_L1_DCM, PAPI_L2_TCM, PAPI_L3_TCM};
  const char* papi_strings[NUM_PAPI_EVENTS] = {"L1 data misses",
                                         "L2 misses",
                                         "L3 misses"};
  uint64_t g_stats[NUM_GLOBAL_STATS];
  const char* g_stat_strings[NUM_GLOBAL_STATS] = {};

  uint64_t* t_stats[NUM_LOCAL_STATS];
  const char* t_stat_strings[NUM_LOCAL_STATS] = {"Suspensions"};

#endif

  __attribute__((constructor(101))) void porr_init(void)
  {
    porr::g_rr_state = new porr::state();
#ifdef USE_LOCKSTAT
    sls_setup();
#endif    

#if STATS > 0
    memset(g_stats, 0, sizeof(uint64_t) * NUM_GLOBAL_STATS);
    int p = __cilkrts_get_nworkers();
    for (int i = 0; i < NUM_LOCAL_STATS; ++i)
      t_stats[i] = (uint64_t*) calloc(p, sizeof(uint64_t));

    assert(PAPI_num_counters() >= NUM_PAPI_EVENTS);
    int ret = PAPI_start_counters(papi_events, NUM_PAPI_EVENTS);
    assert(ret == PAPI_OK);
#endif
  }

  __attribute__((destructor(101))) void porr_deinit(void)
  {
#ifdef USE_LOCKSTAT
    // sls_print_stats();
    sls_print_accum_stats();
#endif    

#if STATS > 0
    int ret = PAPI_read_counters(papi_counts, NUM_PAPI_EVENTS);
    assert(ret == PAPI_OK);
    for (int i = 0; i < NUM_PAPI_EVENTS; ++i)
      printf("%s: %lld\n", papi_strings[i], papi_counts[i]);

    for (int i = 0; i < NUM_GLOBAL_STATS; ++i)
      printf("%s: %lu\n", g_stat_strings[i], g_stats[i]);

    uint64_t accum[NUM_LOCAL_STATS];
    int p = __cilkrts_get_nworkers();
    
    for (int i = 0; i < NUM_LOCAL_STATS; ++i) {
      accum[i] = 0;
      for (int j = 0; j < p; ++j)
        accum[i] += t_stats[i][j];
    }

    for (int i = 0; i < NUM_LOCAL_STATS; ++i)
      free(t_stats[i]);

    for (int i = 0; i < NUM_LOCAL_STATS; ++i)
      printf("%s: %lu\n", t_stat_strings[i], accum[i]);

    __cilkrts_dump_porr_stats(stdout);

#endif

    delete porr::g_rr_state;
  }
}

#ifdef USE_LOCKSTAT
extern "C" {
void sls_setup(void)
{
    pthread_spin_init(&the_sls_lock, 0);
    the_sls_list = NULL;
    the_sls_setup = 1;
}

static void sls_print_lock(struct spinlock_stat *l)
{
    // fprintf(stderr, "id %7lu addr %p ", l->m_id, l);
    fprintf(stderr, "id %7lu: ", l->m_id);
    fprintf(stderr, "contend %8lu, acquire %8lu, wait %10lu, held %10lu\n",
            l->contend, l->acquire, l->wait, l->held);
}

static void sls_accum_lock_stat(struct spinlock_stat *l, 
                                unsigned long *contend, unsigned long *acquire,
                                unsigned long *wait, unsigned long *held) 
{
    *contend += l->contend; 
    *acquire += l->acquire; 
    *wait += l->wait; 
    *held += l->held;
    assert(*contend >= l->contend);
    assert(*acquire >= l->acquire);
    assert(*wait >= l->wait);
    assert(*held >= l->held);
}

void sls_print_stats(void)
{
    fprintf(stderr, "Printing lock stats.\n");
    struct spinlock_stat *l;
    pthread_spin_lock(&the_sls_lock);
    for (l = the_sls_list; l; l = l->next)
            sls_print_lock(l);
    pthread_spin_unlock(&the_sls_lock);
}

void sls_print_accum_stats(void)
{
    struct spinlock_stat *l;
    unsigned long contend = 0UL, acquire = 0UL, wait = 0UL, held = 0UL;
    pthread_spin_lock(&the_sls_lock);
    unsigned long count = 0;

    for (l = the_sls_list; l; l = l->next) {
        count++;
        sls_accum_lock_stat(l, &contend, &acquire, &wait, &held);
    }
    pthread_spin_unlock(&the_sls_lock);

    printf("Total of %lu locks found.\n", count);
    printf("contend %8lu, acquire %8lu, wait %10lu (cyc), held %10lu (cyc)\n",
           contend, acquire, wait, held);
}
}
#endif
