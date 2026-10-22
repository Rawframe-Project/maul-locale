// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Grapheme cluster boundaries at any position of a text, for search's
// match bounds (record mloc-0074), as ICU4C's character break iterator
// gives them to usearch: Maul Unicode's grapheme iterator run from a
// safe code point before the position. A code point whose break is
// Other, Control, CR or LF, that is not Extended_Pictographic and has
// no Indic_Conjunct_Break, starts no chain any rule reads back through
// (regional indicator pairs, emoji ZWJ sequences, conjuncts, Hangul
// syllables, a prepend's cluster), so every boundary after it is the
// same whether the text starts there or before. UTF-16 is read through
// its UTF-8 form, a lone surrogate as U+FFFD, as every other text the
// library reads.

#ifndef MLOC_SRC_SEARCH_BOUNDARY_H
#define MLOC_SRC_SEARCH_BOUNDARY_H

#include "nfd_stream.h"

#include <stdbool.h>
#include <stddef.h>

// Whether a code point boundary of the text is a grapheme cluster
// boundary; the text's start and end are.
bool mlocSearchIsBoundary(NfdText text, size_t at);

// The first grapheme cluster boundary after a code point boundary, or
// the text's length when none is.
size_t mlocSearchFollowing(NfdText text, size_t at);

#endif
