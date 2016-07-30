#include <iostream>
#include <fstream>
#include <cstdlib> // getenv
#include <algorithm> // count
#include <csignal> // raise

#include "cilkrr.h"

#include <internal/abi.h>

#ifdef USE_LOCKSTAT
extern "C" {
    #include "lockstat.h"
}
#endif


namespace cilkrr {

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

#if PTYPE != PDOT
    p = current->actual;
    
#else // dot product
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

    // if (actual != p) {
    //  fprintf(stderr, "Pre: %zu, Dot: %zu\n", actual, p);
    //  raise(SIGSTOP);
    //  //assert(0);
    // }
#endif
    
    return p;
  }

  state::state() : m_size(0), m_active_size(0)
  {
    char *env;
    g_rr_state = this;

#if PTYPE != PPRE
    srand(0);
    for (int i = 0; i < 256; ++i)
      randvec[i] = i+1;
    //randvec[i] = rand() % big_prime;
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

          /// @todo{ Write out # acquires for each lock, then reserve
          /// a hash table of that size when reading in }

          // get id
          size_t num; input >> num;
          assert(num == i);

          // get # acquires
          assert(input.peek() == ':');
            input.ignore(1, ':');
          input >> num;

          // finish reading line
          getline(input, line);

          m_all_acquires.push_back(new acquire_container(num));
          acquire_container* cont = m_all_acquires[i];
          
          int lineno = 0;
          while (input.peek() != '}') {
            pedigree_t p;
#if PTYPE != PARRAY
            full_pedigree_t full = {0, nullptr};
#endif

            // Get rid of beginning
            input.ignore(std::numeric_limits<std::streamsize>::max(), '\t');

            getline(input, line);
            size_t ind = line.find('[');
#if PTYPE != PARRAY
            if (ind == std::string::npos) // read in compressed pedigree
              p = std::stoul(line);
            else { // read in full pedigree
              p = std::stoul(line.substr(0,ind));
              full.length = std::count(line.begin(), line.end(), ',');
              full.array = (uint64_t*) malloc(full.length * sizeof(uint64_t));
              for (int i = 0; i < full.length; ++i) {
                ind++;
                size_t next_ind = line.find(',', ind);
                full.array[i] = std::stoul(line.substr(ind, next_ind - ind));
                ind = next_ind;
              }
            }
            
            cont->add(p, full);
#else
            p.length = std::count(line.begin(), line.end(), ',');
            p.array = (uint64_t*) malloc(p.length * sizeof(uint64_t));
            for (int i = 0; i < p.length; ++i) {
              ind++;
              size_t next_ind = line.find(',', ind);
              p.array[i] = std::stoul(line.substr(ind, next_ind - ind));
              ind = next_ind;
            }
            cont->add(p);
#endif
            lineno++;
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

    // This should be true, but some pbbs benchmarks (dictionary)
    // don't call the lock destructors I think they do this so avoid
    // the slowdown, so I don't want to mess with that
    // assert(m_active_size == 0);

    if (m_mode != RECORD) return;
    std::ofstream output;
    acquire_container* cont;
    output.open(m_filename);

    // Write out total # acquires
    output << m_size << std::endl;

    size_t mem_allocated = 0;
    size_t num_conflicts = 0;
    for (int i = 0; i < m_size; ++i) {
      cont = m_all_acquires[i];
      
      output << "{" << i << ":" << cont->size() << std::endl;
      cont->stats();
      cont->print(output);
      mem_allocated += cont->memsize();
      num_conflicts += cont->m_num_conflicts;
      output << "}" << std::endl;
    }
    output << "---- Stats ----" << std::endl;
    output << "Conflicts: " << num_conflicts << std::endl;
    output.close();

    //fprintf(stderr, "%zu bytes allocated for %zu locks\n", mem_allocated, m_size);
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

#ifdef USE_PAPI
  long long papi_counts[NUM_PAPI_EVENTS];
  int papi_events[NUM_PAPI_EVENTS] = {PAPI_L1_DCM, PAPI_L2_TCM, PAPI_L3_TCM};
  const char* papi_strings[NUM_PAPI_EVENTS] = {"L1 data misses",
                                         "L2 misses",
                                         "L3 misses"};
#endif

  __attribute__((constructor(101))) void cilkrr_init(void)
  {
    cilkrr::g_rr_state = new cilkrr::state();
#ifdef USE_LOCKSTAT
    sls_setup();
#endif    

#ifdef USE_PAPI
    assert(PAPI_num_counters() >= NUM_PAPI_EVENTS);
    int ret = PAPI_start_counters(papi_events, NUM_PAPI_EVENTS);
    assert(ret == PAPI_OK);
#endif
  }

  __attribute__((destructor(101))) void cilkrr_deinit(void)
  {
#ifdef USE_LOCKSTAT
    // sls_print_stats();
    sls_print_accum_stats();
#endif    

#ifdef USE_PAPI
    int ret = PAPI_read_counters(papi_counts, NUM_PAPI_EVENTS);
    assert(ret == PAPI_OK);

    for (int i = 0; i < NUM_PAPI_EVENTS; ++i)
      printf("%s: %lld\n", papi_strings[i], papi_counts[i]);
#endif

    delete cilkrr::g_rr_state;
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
