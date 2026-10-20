// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A tailoring's mappings over the root while the builder makes them
// (record mloc-0071), and the elements a string has through them, as
// ICU4C's CollationDataBuilder holds its mappings and its
// DataBuilderCollationIterator reads them: a code point the tailoring
// maps is read from the tailoring alone, any other from the root.
//
// The first mapping of a code point copies the root's mappings of it, so
// that its list stays whole, and a mapping of the same prefix and text
// replaces the one there.
//
// The root's mappings are its file's, and the contractions of U+FDD1 that
// ICU's root data adds from the script starts.
//
// A code point's elements: its mappings with the longest prefix that the
// code points before it end with, then the longest contraction the code
// points after it spell, contiguous or not (UTS #10's discontiguous
// rule, as the runtime's iterator reads the root). Where the prefix has
// contractions but no mapping of the code point alone and none matches,
// the next shorter prefix that matches is taken, down to no prefix.
// Strings are NFD code points; completely ignorable elements are left
// out.

#ifndef MLOCGEN_COLLATION_MAPPINGS_H
#define MLOCGEN_COLLATION_MAPPINGS_H

#include "collation_root.h"
#include "collation_tables.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CollationMapping
{
    // The prefix in text order; the text, whose first code point keys
    // the mapping, then the contraction's.
    uint32_t prefix[collationMaxContext];
    uint32_t prefixLength;
    uint32_t text[collationMaxContext + 1];
    uint32_t textLength;
    // The elements: [elementStart, elementStart + elementCount) of the
    // pool.
    uint32_t elementStart;
    uint32_t elementCount;
    // The next mapping of the same first code point, or -1.
    int32_t next;
} CollationMapping;

// A code point's first mapping, its index plus one; 0 for an empty slot.
typedef struct CollationMappingSlot
{
    uint32_t codePoint;
    int32_t head;
} CollationMappingSlot;

// A mapping as the lookup reads it, from the root or the tailoring.
typedef struct CollationCandidate
{
    const uint32_t* prefix;
    uint32_t prefixLength;
    const uint32_t* text;
    uint32_t textLength;
    const uint64_t* elements;
    uint32_t elementCount;
} CollationCandidate;

typedef struct CollationMappings
{
    const RootTable* root;
    // The root's mappings by first code point: pairs of the code point
    // and the mapping's index, sorted.
    uint64_t* rootIndex;
    size_t rootIndexCount;
    // ICU's root maps the script starts' FDD1 lines as contractions of
    // U+FDD1, and FDD1 FDD0 to the first unassigned primary.
    RootMapping* markers;
    size_t markerCount;
    // The tailoring's mappings, chained by first code point from the
    // slots (an open-addressed table, a power of two long).
    CollationMapping* mappings;
    size_t count;
    size_t capacity;
    CollationMappingSlot* slots;
    size_t slotCount;
    size_t slotsUsed;
    uint64_t* elements;
    size_t elementCount;
    size_t elementCapacity;
    // The candidates of one code point, gathered for a lookup.
    CollationCandidate* candidates;
    size_t candidateCapacity;
    // The element the root gives by rule (Han, U+FFFD, U+FFFF, the
    // unassigned) to the code point gathered last.
    uint64_t ruleElement;
    uint32_t ruleText;
} CollationMappings;

// Prepares the mappings over a root, which must outlive them; 0 when out
// of memory.
int CollationMappingsInit(CollationMappings* m, const RootTable* root);

void CollationMappingsFree(CollationMappings* m);

// Adds a mapping (ICU's addCE32); 0 for a text empty or longer than a
// contraction may be, a prefix too long, more than collationMaxExpansion
// elements, or out of memory.
int CollationMappingsAdd(CollationMappings* m, const uint32_t* prefix, uint32_t prefixLength,
                         const uint32_t* text, uint32_t textLength, const uint64_t* elements,
                         uint32_t elementCount);

// Copies the root's mappings of a code point the tailoring has none of
// (ICU's copyFromBaseCE32); 0 when out of memory.
int CollationMappingsCopyRoot(CollationMappings* m, uint32_t codePoint);

// Leaves a code point its mapping alone, without prefixes or
// contractions (ICU's suppressContractions for one code point): the
// tailoring's own, or the root's when the tailoring has none and the
// root's has a context; 0 when out of memory.
int CollationMappingsSuppress(CollationMappings* m, uint32_t codePoint);

// The elements of text[start, length) after text[0, start), its context
// (ICU's getCEs): appended to out from outLength, as many as fit in
// collationMaxExpansion; the count they would take, which may be more.
uint32_t CollationMappingsElements(CollationMappings* m, const uint32_t* text, uint32_t length,
                                   uint32_t start, uint64_t* out, uint32_t outLength);

// The tailoring's mappings of a code point, chained by next from the
// index returned; -1 when the tailoring has none.
int32_t CollationMappingsFirst(const CollationMappings* m, uint32_t codePoint);

#endif
