// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog's benchmark cases: a set of 2,000 keys, compiled by mloccat
// when the tools are built, in en and fr. A key found, a message got by
// index, a catalog opened, a chain made and bound, and a message formatted
// through the chain by index and by key, beside the same message bound and
// formatted directly. GNU gettext's lookup of the same set is timed by
// tools/oracle/bench/gettext_catalog_bench.c.

#ifndef MLOC_BENCH_CATALOG_H
#define MLOC_BENCH_CATALOG_H

#include "bench_dates.h"

#include <stdbool.h>
#include <stddef.h>

// Reads the catalogs and binds the chain; false when a file cannot be
// read or a catalog opened. Without the catalog tool there are no
// catalogs and no cases.
bool PrepareCatalogBench(void);

// The cases, and their count: 0 without the catalogs.
extern const BenchCase g_catalogCases[];
extern size_t g_catalogCaseCount;

#endif
