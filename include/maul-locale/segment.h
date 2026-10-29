// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Segmentation as a language tailors it: word and sentence boundaries
// (UAX #29) and line break opportunities (UAX #14), from Maul Unicode's
// rules and the tailorings of CLDR's segments files in the segmentation
// data file (src/generated/segments.mloc). Grapheme clusters have no
// tailoring; a program takes them from Maul Unicode.
//
// A locale's -u-ss-standard keeps a sentence going after the abbreviations
// CLDR lists for its language ("Mr.", "z.B."), as ICU4C's
// FilteredBreakIterator does; without it, or with -u-ss-none, every
// sentence break stays. Its -u-lb picks the line rules: strict breaks
// before small kana and the prolonged sound mark as UAX #14 does, normal
// and loose may break there (CSS's line-break); without the key a locale
// takes its language's (normal for Chinese and Japanese, strict for the
// others).
//
// A segmenter walks UTF-8 text, given whole or in pieces, and reports each
// boundary after the start of the text, the last one being its end, as
// Maul Unicode's iterators do; offsets are in bytes from the start of the
// whole text.

#ifndef MAUL_LOCALE_SEGMENT_H
#define MAUL_LOCALE_SEGMENT_H

#include "maul-locale/locale.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // What a segmenter finds.
    typedef enum mlocSegmentKind
    {
        // Word boundaries: between them lies a word, a run of spaces or a
        // punctuation mark, which the caller tells apart.
        mloc_segmentWord = 0,
        // Sentence boundaries.
        mloc_segmentSentence = 1,
        // Line break opportunities, each mandatory or not.
        mloc_segmentLine = 2,
    } mlocSegmentKind;

    // A caller's word segmenter for Thai, Lao, Khmer and Burmese, which
    // write words without spaces, as Maul Unicode's muniComplexBreakFunction: it
    // receives a maximal run of such text as UTF-8 and a byte offset from
    // inside it, and returns the byte offset of the first break between
    // words after from, or length when there is none. It must return the
    // same answer for the same run and offset each time. It must not make
    // again the segmenter it serves; any other call on that segmenter from
    // inside it is refused with mloc_errorInvalid and changes nothing.
    typedef size_t (*mlocComplexBreakFunction)(void* context, const char* run, size_t length,
                                               size_t from);

    // A segmenter: Maul Unicode's iterator with a locale's tailoring, over
    // the segmentation data it was made from, which must outlive it. It
    // keeps no pointer to a piece of text once it has asked for the next
    // one. Opaque; make it with mlocInitSegmenter. It may be copied.
    typedef struct mlocSegmenter
    {
        uint64_t opaque[64];
    } mlocSegmenter;

    /// Starts a segmenter over a UTF-8 text or its first piece, with a
    /// locale's tailoring of that kind of segmentation: the first unit
    /// along the locale's segmentation chain that tailors it (a variant,
    /// then the region, then the script are dropped; "en-US-u-va-posix"
    /// takes POSIX English's words and English's sentences), else Maul Unicode's
    /// rules alone. Each maximal ill-formed UTF-8 subpart counts as one
    /// U+FFFD.
    ///
    /// @param data          A data set holding the segmentation data file.
    /// @param locale        A canonical locale.
    /// @param kind          What the segmenter finds.
    /// @param text          The text; may be null when length is 0.
    /// @param length        Its length in bytes.
    /// @param moreFollows   true when more pieces of the text will be fed.
    /// @param segmenterOut  Receives the segmenter; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_errorNotFound for a data set without the
    ///         segmentation data; mloc_errorInvalid for a null pointer, a
    ///         null text with a nonzero length or an unknown kind.
    /// @par Thread safety
    /// Safe from any thread; a segmenter is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitSegmenter(const mlocDataSet* data,
                                                         const mlocLocale* locale,
                                                         mlocSegmentKind kind, const char* text,
                                                         size_t length, bool moreFollows,
                                                         mlocSegmenter* segmenterOut);

    /// Sets the word segmenter a word or line segmenter hands runs of
    /// Thai, Lao, Khmer and Burmese text to, as Maul Unicode's
    /// muniSetComplexBreaker: inside such a run its breaks replace the
    /// default ones. Without one, such text follows rule LB1 of UAX #14 and
    /// the default word rules.
    ///
    /// @param segmenter  A word or line segmenter not yet asked for a break.
    /// @param breaker    The word segmenter, or null for the default rules.
    /// @param context    Passed to the word segmenter unchanged.
    /// @return mloc_success; mloc_errorInvalid for a null segmenter, one the
    ///         library did not make, a sentence segmenter or one already
    ///         asked for a break.
    /// @par Thread safety
    /// Safe from any thread; a segmenter is used by one thread at a time,
    /// and calls the word segmenter on that thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocSegmenter_SetComplexBreaker(
        mlocSegmenter* segmenter, mlocComplexBreakFunction breaker, void* context);

    /// Hands a segmenter the next piece of its text, after it returned
    /// mloc_needMoreText. The piece may begin in the middle of a UTF-8
    /// sequence.
    ///
    /// @param segmenter    The segmenter.
    /// @param text         The next piece; may be null when length is 0.
    /// @param length       Its length in bytes.
    /// @param moreFollows  true when still more pieces will be fed.
    /// @return mloc_success; mloc_errorInvalid for a null segmenter, one
    ///         the library did not make, a null text with a nonzero length,
    ///         a segmenter started without more text to follow, or a piece
    ///         fed before the segmenter asked for it.
    /// @par Thread safety
    /// Safe from any thread; a segmenter is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocSegmenter_Feed(mlocSegmenter* segmenter,
                                                          const char* text, size_t length,
                                                          bool moreFollows);

    /// Finds the next boundary, or line break opportunity, and whether the
    /// line must end there: after a mandatory break character (BK, CR, LF
    /// or NL) and at the end of the text.
    ///
    /// @param segmenter     The segmenter.
    /// @param offsetOut     Receives the boundary's byte offset from the
    ///                      start of the whole text.
    /// @param mandatoryOut  Receives whether the line must end there:
    ///                      always false for words and sentences. May be
    ///                      null.
    /// @return mloc_success with a boundary; mloc_done after the last one;
    ///         mloc_needMoreText when the next boundary depends on text not
    ///         yet fed; mloc_errorInvalid for a null segmenter or offset,
    ///         or a segmenter the library did not make.
    /// @par Thread safety
    /// Safe from any thread; a segmenter is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocSegmenter_NextBreak(mlocSegmenter* segmenter,
                                                               size_t* offsetOut,
                                                               bool* mandatoryOut);

#ifdef __cplusplus
}
#endif

#endif
