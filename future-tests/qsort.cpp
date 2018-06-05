#include <cstdlib>
#include <iostream>
#include <future>
#include <cilk/cilk.h>
#include "../src/future.h"

#define LIST_SIZE (2000)
#define SEQ_THRESH (10)

inline void swap(long *l, unsigned long x, unsigned long y) {
    long tmp = l[x];
    l[x] = l[y];
    l[y] = tmp;
}

unsigned long partition(long *l, unsigned long n) {
    unsigned long low = 0;
    for (unsigned long i = 0; i < n-1; i++) {
        if (l[i] <= l[n-1]) {
            swap(l, low, i);
            low++;
        }
    }
    swap(l, n-1, low);

    return low;
}

void qsort(long *l, unsigned long n) {
    if (n > 1) {
        unsigned long q = partition(l, n);
        qsort(l, q);
        qsort(l+q+1, n-q-1);
    } // end if (n > 1)
    // else n <= 1, which is trivially sorted
}

unsigned long future_partition(long *l, unsigned long n) {
    unsigned long low = 0;
    for (unsigned long i = 0; i < n-1; i++) {
        if (l[i] <= l[n-1]) {
            swap(l, low, i);
            low++;
        }
    }
    swap(l, n-1, low);

    return low;
}

void future_qsort(long *l, unsigned long n, std::future<void>* rest) {
    //std::launch launch_type = std::launch::async;
    if (n <= SEQ_THRESH) {
        qsort(l, n);
    } else if (n > 1) {
        unsigned long q = future_partition(l, n);
        std::future<void> left;
        left = std::async(std::launch::async, future_qsort, l, q, rest);
        future_qsort(l+q+1, n-q-1, &left);
    }
    
    if (rest != NULL && rest->valid()) {
        rest->get();
    }
}

bool cilk_future_qsort(long *l, unsigned long n, cilk::future<bool>* rest) {
    if (n <= SEQ_THRESH) {
        qsort(l, n);
    } else if (n > 1) {
        unsigned long q = partition(l, n);
        cilk::future<bool> *left;
        cilk_future_create(bool, left, cilk_future_qsort, l, q, rest);
        cilk_future_qsort(l+q+1, n-q-1, left);
    }

    if (rest != NULL) {
        return cilk_future_get(rest);
    }
    return true;
}

int run(void) {
    long *l = (long *) malloc(LIST_SIZE * sizeof(long));;
    for (unsigned long i = 0; i < LIST_SIZE; i++) {
        l[i] = rand() % 25;
    }

    //qsort(l, LIST_SIZE);
    //future_qsort(l, LIST_SIZE, NULL);
    cilk_future_qsort(l, LIST_SIZE, NULL);

    for (unsigned long i = 0; i < LIST_SIZE-1; i++) {
        if (l[i] > l[i+1]) {
            std::cout << "ERR: " << l[i] << " should come after " << l[i+1] << std::endl;
            break;
        }
    }

    printf("Success!\n");

    //for (unsigned long i = 0; i < LIST_SIZE; i++) {
    //    std::cout << l[i] << ", ";
    //    if (i % 20 == 19) {
    //        std::cout << std::endl;
    //    }
    //}
    //std::cout << std::endl;

    return 0;
}

int main(void) {
    cilk_spawn run();    
    cilk_sync;

    return 0;
}

