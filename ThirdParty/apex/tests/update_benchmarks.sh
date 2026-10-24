#!/bin/bash

cd $HOME/Desktop/Code/apex

./tests/benchmark.sh >BENCHMARK.md
./tests/benchmark_comparison.sh >BENCHMARK_COMPARISON.md

git add BENCHMARK.md BENCHMARK_COMPARISON.md
git commit -m "Update benchmarks"
