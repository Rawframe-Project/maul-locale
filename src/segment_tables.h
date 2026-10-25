// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit of the segmentation data file, segments.mloc, which the
// generator compiles from CLDR's common/segments: a locale's tailoring of
// Maul Unicode's word, sentence and line breaking. All integers are
// little-endian.
//
// Segmentation, a unit per locale key whose file gives a segmentation.
// The payload is a header (u32 types given, u32 flags, u32 word
// overrides, u32 sentence overrides, u32 backward count, u32 forward
// count, u32 text size), the backward entries' values (u32 each), the
// entries' offsets (u32 each, the backward entries' then the forward
// entries', then the text size: backward count + forward count + 1, from
// 0, ascending), then the text, UTF-8 with no byte below 0x20.
//
// The types given are the segmentations the locale's file has. Each
// replaces the same type of the locale's parents; a type a unit does not
// give comes from the next unit along the locale's segmentation chain
// that gives it, else is root's, which is Maul Unicode's own. What a unit
// holds belongs to a type it gives: the word overrides and the Japanese
// word rules to the word type, the sentence overrides and suppressions to
// the sentence type, CJ as an ideograph to the line type.
//
// An override gives a code point another class. The overrides are no
// data of the unit: they are a code table, segment_overrides.h's, which
// the generator writes and the data version's tables hash covers, so that
// Maul Unicode reads them from constant memory and a segmenter holds no
// pointer into itself. A unit's word overrides and sentence overrides are
// 0 for none, else the number plus one of a set of that type in the
// table.
// A suppression is an abbreviation after which a sentence goes on ("Mr.",
// "z.B."), as ICU4C's FilteredBreakIterator takes them with -u-ss-standard,
// in the two tables its builder makes of them. An abbreviation with a full
// stop before its end ("z.B.") is partial; it and every abbreviation that
// begins with the same text through its first full stop ("z.", "z.T.")
// are a family, whose members are the forward entries. The backward
// entries are each family's text through its first full stop ("z."), with
// the value partial, and every abbreviation of no family, with the value
// match. The backward entries are distinct and sorted by their bytes read
// from the end, the forward entries distinct and sorted by their bytes.
//
// Each sentence break the segmenter finds, but one at the end of the text,
// is checked as ICU4C checks it. Step back over one U+0020 before the break
// if there is one; then take the longest backward entry that the text
// before that point ends with. A match suppresses the break. A partial
// reads the text from that entry's start one character at a time and stops
// at the first of: a character with which no forward entry goes on from the
// characters read before it, an entry that no other entry goes on from, or
// the end of the text. The break is suppressed only when the walk stops at
// such a final entry, or when the text ends right after an entry. Stopping
// at a character no entry goes on with keeps the break, even right after an
// entry: with the forward entries "z." and "z.B.", a break after "z. " in
// "Er kam z. Bald" stays, since no entry goes on from "z." with a space.
// The walk may read past the break: with the partial "trav." and the
// forward entry "trav. publ.", a break after "trav. " before "publ." is
// suppressed. A suppressed break is replaced by the next one, which is
// checked the same way.
//
// A data layout: tools/check_data_format.py watches this header.

#ifndef MLOC_SEGMENT_TABLES_H
#define MLOC_SEGMENT_TABLES_H

#include "data_view.h"

#include <stdint.h>

// The segmentations a unit gives, as bits.
enum
{
    segmentGivesWord = 1u << 0,
    segmentGivesSentence = 1u << 1,
    segmentGivesLine = 1u << 2,
};

// The flags, as bits. CJ as an ideograph: Line_Break=CJ breaks as ID, not
// NS (CLDR's ja, zh and zh_Hant lines, CSS's normal line breaking).
// Japanese words: CLDR's ja rules 13.3 and 13.4, no break between two
// Hiragana or between two ideographs (Ideographic, U+3005, U+3007 and
// U+303B), each with the Extend and Format characters after it.
enum
{
    segmentCjIdeographic = 1u << 0,
    segmentJapaneseWords = 1u << 1,
};

// A backward entry's value.
enum
{
    segmentSuppressionMatch = 0,
    segmentSuppressionPartial = 1,
};

enum
{
    segmentHeaderSize = 28,
    // The longest entry, in bytes: a segmenter keeps this many before a
    // break to look them up.
    segmentMaxSuppression = 32,
    segmentMaxSuppressions = 4096,
};

// Whether a payload is a valid segmentation unit.
bool mlocValidSegments(const DataView* view, DataSpan payload);

#endif
