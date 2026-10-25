// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit of the segmentation data file, segments.mloc, which the
// generator compiles from CLDR's common/segments: a locale's tailoring of
// Maul Unicode's word, sentence and line breaking. All integers are
// little-endian.
//
// Segmentation, a unit per locale key whose file gives a segmentation.
// The payload is a header (u32 types given, u32 flags, u32 word override
// count, u32 sentence override count, u32 suppression count, u32 text
// size), the word overrides then the sentence overrides (u32 code point,
// u32 class each, strictly ascending by code point), the suppressions'
// offsets (u32 each, suppression count + 1, from 0 to the text size,
// ascending), then the text, UTF-8 with no byte below 0x20.
//
// The types given are the segmentations the locale's file has. Each
// replaces the same type of the locale's parents; a type a unit does not
// give comes from the next unit along the locale's segmentation chain
// that gives it, else is root's, which is Maul Unicode's own. What a unit
// holds belongs to a type it gives: the word overrides and the Japanese
// word rules to the word type, the sentence overrides and suppressions to
// the sentence type, CJ as an ideograph to the line type.
//
// An override gives a code point another class: a SegmentWordClass or a
// SegmentSentenceClass, the library's numbers, which it maps to Maul
// Unicode's, so that a renumbering there cannot change what a file means.
// A suppression is an abbreviation after which a sentence goes on ("Mr.",
// "z.B."), as ICU4C's FilteredBreakIterator takes them with -u-ss-standard;
// the suppressions are distinct, sorted by their bytes read from the end,
// so that a break looks up the text before it backwards.
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

// Word_Break values, in UAX #29's order.
typedef enum SegmentWordClass
{
    segmentWordCR = 0,
    segmentWordLF = 1,
    segmentWordNewline = 2,
    segmentWordExtend = 3,
    segmentWordZWJ = 4,
    segmentWordRegionalIndicator = 5,
    segmentWordFormat = 6,
    segmentWordKatakana = 7,
    segmentWordHebrewLetter = 8,
    segmentWordALetter = 9,
    segmentWordSingleQuote = 10,
    segmentWordDoubleQuote = 11,
    segmentWordMidNumLet = 12,
    segmentWordMidLetter = 13,
    segmentWordMidNum = 14,
    segmentWordNumeric = 15,
    segmentWordExtendNumLet = 16,
    segmentWordWSegSpace = 17,
    segmentWordOther = 18,
    segmentWordClassCount = 19,
} SegmentWordClass;

// Sentence_Break values, in UAX #29's order.
typedef enum SegmentSentenceClass
{
    segmentSentenceCR = 0,
    segmentSentenceLF = 1,
    segmentSentenceExtend = 2,
    segmentSentenceSep = 3,
    segmentSentenceFormat = 4,
    segmentSentenceSp = 5,
    segmentSentenceLower = 6,
    segmentSentenceUpper = 7,
    segmentSentenceOLetter = 8,
    segmentSentenceNumeric = 9,
    segmentSentenceATerm = 10,
    segmentSentenceSContinue = 11,
    segmentSentenceSTerm = 12,
    segmentSentenceClose = 13,
    segmentSentenceOther = 14,
    segmentSentenceClassCount = 15,
} SegmentSentenceClass;

enum
{
    segmentHeaderSize = 24,
    segmentOverrideSize = 8,
    // A segmenter copies a type's overrides into the caller's memory.
    segmentMaxOverrides = 4,
    // A segmenter keeps this many bytes before a break to look them up.
    segmentMaxSuppression = 32,
    segmentMaxSuppressions = 4096,
};

// Whether a payload is a valid segmentation unit.
bool mlocValidSegments(const DataView* view, DataSpan payload);

#endif
