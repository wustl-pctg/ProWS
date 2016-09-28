#include <string.h>
#include <pthread.h>
#include <assert.h>

struct spinlock_stat {
	unsigned long contend;
	unsigned long acquire;
	unsigned long wait;
	unsigned long held;
	unsigned long held_start;

	uint64_t m_id;
	struct spinlock_stat *next;

	volatile unsigned int slock;
	// pthread_spinlock_t my_lock;
	unsigned char __pad[64];
} __attribute__((aligned(64)));

extern struct spinlock_stat *the_sls_list;
extern pthread_spinlock_t the_sls_lock;
extern int the_sls_setup;

static inline unsigned long sls_read_tsc(void)
{
	unsigned int a, d;
	__asm __volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((unsigned long) a) | (((unsigned long) d) << 32);
}


static inline void sls_init(struct spinlock_stat *lock, uint64_t id)
{
	struct spinlock_stat *l;

	assert(the_sls_setup);

  lock->m_id = id;
	lock->contend = 0UL;
	lock->acquire = 0UL;
	lock->wait = 0UL;
	lock->held = 0UL;
  lock->next = NULL;

	lock->slock = 0;
	// pthread_spin_init(&(lock->my_lock), PTHREAD_PROCESS_PRIVATE);

  /*
    for (l = the_sls_list; l; l = l->next) {
		if (l == lock) {
    pthread_spin_unlock(&the_sls_lock);
    return;
		}
    } */
	pthread_spin_lock(&the_sls_lock);
	lock->next = the_sls_list;
	the_sls_list = lock;
	pthread_spin_unlock(&the_sls_lock);
}

static inline void sls_destroy(struct spinlock_stat *lock)
{
  /*
    struct spinlock_stat *l, *p;
    p = 0;
    pthread_spin_lock(&the_sls_lock);
    for (l = the_sls_list; l; l = l->next) {
		if (l == lock) {
    if (p)
    p->next = l->next;
    else
    the_sls_list = l->next;
    break;
		}
		p = l;
    }
    pthread_spin_unlock(&the_sls_lock);
  */
}

/* Upon success lock acquire, returns nonzero */
static inline int __sls_trylock(struct spinlock_stat *lock)
{
	int tmp, new_v;

	asm volatile("movzwl %2, %0\n\t"
               "cmpb %h0,%b0\n\t"
               "leal 0x100(%q0), %1\n\t"
               "jne 1f\n\t"
               "lock; cmpxchgw %w1,%2\n\t"
               "1:"
               "sete %b1\n\t"
               "movzbl %b1,%0\n\t"
               : "=&a" (tmp), "=&q" (new_v), "+m" (lock->slock)
               :
               : "memory", "cc");

	return tmp;

  // return (pthread_spin_trylock(&(lock->my_lock)) == 0);
}

static inline void sls_lock(struct spinlock_stat *lock)
{
	unsigned long start;
	short inc = 0x0100;

	if (__sls_trylock(lock)) {
		lock->held_start = sls_read_tsc();
		lock->acquire++;
		return;
	}

	start = sls_read_tsc();
       
	asm volatile (
                "lock; xaddw %w0, %1\n"
                "1:\t"
                "cmpb %h0, %b0\n\t"
                "je 2f\n\t"
                "rep ; nop\n\t"
                "movb %1, %b0\n\t"
                // don't need lfence here, because loads are in-order 
                "jmp 1b\n"
                "2:"
                : "+Q" (inc), "+m" (lock->slock)
                :
                : "memory", "cc");
        
  /*
    int ret = 0;
    do {
    ret = pthread_spin_trylock(&(lock->my_lock));
    } while( ret == EBUSY );
    assert(ret == 0); // if reach here, we should have the lock
  */

	lock->contend++;
	lock->acquire++;
	lock->wait += sls_read_tsc() - start;
	lock->held_start = sls_read_tsc();
}

static inline int sls_trylock(struct spinlock_stat *lock)
{
	if (__sls_trylock(lock)) {
		lock->held_start = sls_read_tsc();
		lock->acquire++;
		return 1;
	}
	
	return 0;
}

static inline void sls_unlock(struct spinlock_stat *lock)
{
	lock->held = sls_read_tsc() - lock->held_start;

	asm volatile("incb %0"
               : "+m" (lock->slock)
               :
               : "memory", "cc");
  /*
    int ret = pthread_spin_unlock(&(lock->my_lock));
    assert(ret == 0);
  */
}

void sls_setup(void);
void sls_print_stats(void);
void sls_print_accum_stats(void);

