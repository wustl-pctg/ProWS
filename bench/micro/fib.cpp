/*
 * Copyright (c) 1994-2003 Massachusetts Institute of Technology
 * Copyright (c) 2003 Bradley C. Kuszmaul
 * Copyright (c) 2013 I-Ting Angelina Lee and Tao B. Schardl 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <cstdio>
#include <chrono>
#include <iostream>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"
int count = 0;

cilkrr::mutex g_mutex;

int fib(int n) {
	if (n < 2) {
    g_mutex.lock();
    count++;
    g_mutex.unlock();
    return (n);
	} else {
		int x = 0;
		int y = 0;

		x = cilk_spawn fib(n - 1);
		y = fib(n - 2);
		cilk_sync;

		return (x + y);
	}
}

int main(int argc, char *argv[])
{
	int n, result;

	if (argc != 2) {
		fprintf(stderr, "Usage: fib [<cilk options>] <n>\n");
		exit(1); 
	}
	n = atoi(argv[1]);

	auto start = std::chrono::high_resolution_clock::now();
	result = fib(n);
	auto end = std::chrono::high_resolution_clock::now();
  
	std::cout << "PORR time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>
    (end - start).count() / 1000.0 << std::endl;

	return 0;
}
