#!/bin/sh
# ODR-violation demo for Section 5.1. Run from this directory.
for opt in -O0 -O2; do
  g++ $opt -std=c++20 fees_a.cc fees_b.cc main.cc -o odr_demo && echo "$opt: $(./odr_demo)"
done
g++ -O0 -std=c++20 -c fees_a.cc && nm -C fees_a.o
rm -f odr_demo *.o
