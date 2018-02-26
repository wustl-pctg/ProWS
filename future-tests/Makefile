include ~/porridge/common.mk

.PHONY = default
default: simple_future_cilkr.cpp
	c++ -o simple_future_cilkr -fcilkplus -std=c++11 simple_future_cilkr.cpp

old: main.cpp
	c++ -fcilkplus -std=c++11 main.cpp

