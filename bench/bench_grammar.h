// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The grammar part's benchmark cases (records mloc-0047 to mloc-0052):
// an inflector made, a short name, a common noun and a number inflected
// in Turkish, a name in Finnish, a word in Hungarian, a Korean particle,
// a list's gender, and mloc:case in a message. No rival has an API that
// inflects, so the bar is the number formatter's call (number-format).

#ifndef MLOC_BENCH_GRAMMAR_H
#define MLOC_BENCH_GRAMMAR_H

#include "bench_dates.h"

#include <stdbool.h>
#include <stddef.h>

// Makes the inflectors and the message; false when one cannot be made.
bool PrepareGrammarBench(void);

// Prints each case's text, as BENCH_SHOW asks.
void ShowGrammarBench(void);

// The cases, and their count.
extern const BenchCase g_grammarCases[];
extern const size_t g_grammarCaseCount;

#endif
