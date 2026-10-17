// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The datetime part's benchmark cases (record mloc-0004): formatters made
// and dates, times, date-times in a zone, ranges, relative times and
// durations written, on the inputs the ICU4C and ICU4X baselines time.

#ifndef MLOC_BENCH_DATES_H
#define MLOC_BENCH_DATES_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*BenchBody)(void);

typedef struct BenchCase
{
    const char* name;
    BenchBody body;
    long iterations;
} BenchCase;

// Reads the data files and makes the formatters; false when a file cannot
// be read or a formatter made.
bool PrepareDateBench(void);

// Prints each case's text, as BENCH_SHOW asks.
void ShowDateBench(void);

// The cases, and their count.
extern const BenchCase g_dateCases[];
extern const size_t g_dateCaseCount;

#endif
