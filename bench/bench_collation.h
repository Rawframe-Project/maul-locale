// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation part's benchmark cases: collators made for de and zh;
// compares of eight German, French, Chinese (pinyin) and Korean words, and
// of German file names sharing a 32-byte prefix; the German words in
// UTF-16 and their sort keys; the German alphabetic index built and a
// name's bucket found; a word searched for at primary strength. The inputs
// are those of the ICU4C baseline
// (tools/oracle/bench/icu4c_collation_bench.cpp), whose compare is the
// bar.

#ifndef MLOC_BENCH_COLLATION_H
#define MLOC_BENCH_COLLATION_H

#include "bench_dates.h"

#include <stdbool.h>
#include <stddef.h>

// Reads the collation data file and makes the collators, the index and
// the searcher; false when one cannot be made.
bool PrepareCollationBench(void);

// The cases, and their count.
extern const BenchCase g_collationCases[];
extern const size_t g_collationCaseCount;

#endif
