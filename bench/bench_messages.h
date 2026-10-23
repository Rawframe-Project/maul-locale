// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message part's benchmark cases: a message compiled and bound, four
// messages and one of each function family formatted, the ones ICU4C's MF2
// and, where MF1 can say them, its MF1 are timed on; parts, unreached
// functions and a formatter with program functions; and the stack a format
// call takes.

#ifndef MLOC_BENCH_MESSAGES_H
#define MLOC_BENCH_MESSAGES_H

#include "bench_dates.h"

#include <stdbool.h>
#include <stddef.h>

// Compiles and binds the messages; false when one fails.
bool PrepareMessageBench(void);

// Prints each message's text, as BENCH_SHOW asks.
void ShowMessageBench(void);

// Prints the stack bytes a format call takes for the plural message and
// for a chain of 63 :number declarations; false when it cannot measure.
bool PrintMessageStack(void);

// The cases, and their count.
extern const BenchCase g_messageCases[];
extern const size_t g_messageCaseCount;

#endif
