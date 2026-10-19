// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog set's report (record mloc-0057): per locale, the coverage
// (translated keys of those the source asks to translate) and each
// finding:
// - missing: a key to translate that the translation lacks;
// - stale: a translation made from another source message (stale.h);
// - unrecorded: a translation with no record of its source message;
// - orphaned: a translation's key the source lacks;
// - differs: a translation of a key the source keeps (@translate no),
//   which the compiler ignores;
// - long: a message above its source entry's @mloc-max-length, counted
//   in code points of its text formatted in its locale with the source
//   entry's @mloc-example arguments (a variable without one is written
//   as its fallback, "{$name}").
// The source is reported too, for its long messages.

#ifndef MLOCCAT_REPORT_H
#define MLOCCAT_REPORT_H

#include "catalog_set.h"

#include <stdint.h>
#include <stdio.h>

typedef enum ReportKind
{
    reportMissing,
    reportStale,
    reportUnrecorded,
    reportOrphaned,
    reportDiffers,
    reportLong,
    reportKindCount,
} ReportKind;

typedef struct Finding
{
    ReportKind kind;
    // The key, owned by the set read.
    const char* key;
    // For a long message: its length and the most allowed.
    uint32_t length;
    uint32_t most;
} Finding;

typedef struct LocaleReport
{
    const CatalogInput* input;
    uint32_t translatable;
    uint32_t translated;
    Finding* findings;
    size_t count;
} LocaleReport;

typedef struct Report
{
    LocaleReport* locales;
    size_t count;
} Report;

// Builds the report of a set read; 0 when a locale has no formatter.
int ReportBuild(const CatalogSet* set, Report* out);

void ReportFree(Report* report);

// The kinds found, a bit per ReportKind.
unsigned ReportKindsFound(const Report* report);

// The kind's name ("missing", "stale", ...).
const char* ReportKindName(ReportKind kind);

// Reads a comma-separated list of kind names into bits; 0 for a name it
// does not know.
int ReportReadKinds(const char* list, unsigned* kindsOut);

// A table of counts per locale, then a line per finding.
void ReportWriteText(const Report* report, FILE* out);

// One JSON object: the source's locale, the key count, and per locale its
// counts and findings by kind.
void ReportWriteJson(const Report* report, FILE* out);

#endif
