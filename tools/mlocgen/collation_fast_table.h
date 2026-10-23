// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin table of a unit (src/collation_fast_latin.h), as ICU4C's
// CollationFastLatinBuilder builds it, from the elements the builder's
// lookup gives the NFD of each fast character and of each pair of them:
// the library reads NFD and has no canonical closure, so a precomposed
// letter's elements are its NFD's, as ICU's closure makes them. A
// character whose NFD has a contraction anywhere in the unit is read as a
// contraction, with an entry for each fast character after it whose pair's
// elements are not its own and the other's, so that what follows it is
// always looked at; a character after a prefix, an element the format
// cannot hold, more than two elements or a contraction that goes on past a
// pair bails out.

#ifndef MLOCGEN_COLLATION_FAST_TABLE_H
#define MLOCGEN_COLLATION_FAST_TABLE_H

#include "collation_mappings.h"
#include "collation_tables.h"

#include <stddef.h>
#include <stdint.h>

// The table's words, the caller frees them; null when ICU builds none
// (the letters need more short primaries than there are).
uint16_t* CollationFastLatinTable(CollationMappings* mappings, const CollationRoot* root,
                                  size_t* countOut);

// A unit with its table section after it (collation_fast_latin.h): the
// count and the words, or only a count of 0 or fastLatinRootTable when
// words is null; the unit is reallocated.
uint8_t* CollationFastLatinAppend(uint8_t* unit, size_t* size, const uint16_t* words, size_t count);

// A type's unit with its section after it: none for the search types,
// as ICU4C builds none for them; the root's (rootWords, rootCount) when
// the type's table is the same; the unit is reallocated.
uint8_t* CollationFastLatinSection(uint8_t* unit, size_t* size, CollationMappings* mappings,
                                   const CollationRoot* root, const uint16_t* rootWords,
                                   size_t rootCount, const char* type);

#endif
