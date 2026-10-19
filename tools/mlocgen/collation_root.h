// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's root collation, FractionalUCA.txt, read into 64-bit collation
// elements as ICU's runtime root holds them (record mloc-0064): a
// primary in bits 63..32 with its bytes left-aligned, a secondary in
// 31..16 and a tertiary with its case bits in 15..0. Han takes ICU's
// implicit primaries from the [Unified_Ideograph] ranges, a step of 2
// apart; U+FFFD and U+FFFF are left to the fixed weights the runtime
// gives them; FDD1 lines are script starts and FDD0 lines special
// primaries, not mappings.

#ifndef MLOCGEN_COLLATION_ROOT_H
#define MLOCGEN_COLLATION_ROOT_H

#include <stddef.h>
#include <stdint.h>

enum
{
    rootMaxText = 8,
    rootMaxElements = 32,
    rootMaxHanRanges = 32,
    // ICU's trailing lead byte; the unassigned one is collation_tables.h's.
    rootTrailingByte = 0xFF,
};

// One line's mapping: a string, with or without a prefix before it, to
// its elements.
typedef struct RootMapping
{
    uint32_t text[rootMaxText];
    uint32_t textLength;
    uint32_t prefix[rootMaxText];
    uint32_t prefixLength;
    uint64_t elements[rootMaxElements];
    uint32_t elementCount;
} RootMapping;

// An FDD1 or FDD0 line: the code point after it and its first element.
typedef struct RootMarker
{
    uint32_t codePoint;
    uint64_t element;
} RootMarker;

typedef struct RootTable
{
    RootMapping* mappings;
    size_t count;
    // FDD1 lines (a script's first primary) and FDD0 lines (the numeric
    // lead byte, the reserved ranges, the inverse table's extra
    // elements).
    RootMarker* scriptStarts;
    size_t scriptStartCount;
    RootMarker* specials;
    size_t specialCount;
    // [Unified_Ideograph] ranges, start and end, in the file's order.
    uint32_t hanRanges[2 * rootMaxHanRanges];
    size_t hanRangeCount;
    uint32_t firstHanPrimary;
    // [top_byte XX ... COMPRESS]: lead bytes whose primaries compress.
    uint8_t compressible[256];
    // [UCA version = a.b.c]
    uint32_t version[3];
} RootTable;

// Reads the file; 0 with a message (a line number and what is wrong)
// when it does not read.
int RootRead(const char* path, RootTable* out, char* error, size_t errorSize);

void RootFree(RootTable* table);

// A Han code point's element (common secondary and tertiary), or 0 when
// the code point is in no [Unified_Ideograph] range.
uint64_t RootHanElement(const RootTable* table, uint32_t codePoint);

#endif
