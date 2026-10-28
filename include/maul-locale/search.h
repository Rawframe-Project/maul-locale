// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation search: a pattern found in a text where a collator holds them
// equal, as ICU4C's usearch finds it: the pattern's collation elements
// against the text's, a match starting and ending on grapheme cluster
// boundaries (or, where ICU allows one, inside a cluster at a
// normalization boundary), the identical strength also asking for the same
// NFD. The searcher is a value the caller keeps; it holds a copy of the
// collator and the pattern's elements, and keeps no pointer to either. A
// find is a call from a position: to find every match, call again from the
// last match's end, or from its start plus one code point for overlapping
// matches; walking back, from the last match's start.

#ifndef MAUL_LOCALE_SEARCH_H
#define MAUL_LOCALE_SEARCH_H

#include "maul-locale/collate.h"

#ifdef __cplusplus
extern "C"
{
#endif

// The most collation elements a search pattern has, and at the identical
// strength the most code points of its NFD.
#define MLOC_MAX_SEARCH_PATTERN 480

// The most collation elements a searcher holds at once, as ICU4C's
// CEIBuffer: the pattern's, 32 more, and with a wildcard comparison 3 for
// each UTF-16 unit of the pattern (8 for one that may be a leading jamo).
// The text's elements between a match's ends must fit the same room.
#define MLOC_MAX_SEARCH_WINDOW 512

// The longest run a backward search reads at once: elements without a
// primary weight, or a piece of unsafe code points.
#define MLOC_MAX_SEARCH_BACKWARD_RUN 128

    // How the elements compare, as ICU4C's USEARCH_ELEMENT_COMPARISON:
    // equal to the strength; or a pattern element with a base weight
    // only (no accent, no case) matching the text's with any of the
    // level; or either side's base weight matching the other's.
    typedef enum mlocSearchComparison
    {
        mloc_searchExact = 0,
        mloc_searchPatternBaseWildcard = 1,
        mloc_searchAnyBaseWildcard = 2,
    } mlocSearchComparison;

    // A searcher's options; the zero value compares exactly.
    typedef struct mlocSearchOptions
    {
        mlocSearchComparison comparison;
    } mlocSearchOptions;

    typedef struct mlocSearcher
    {
        uint64_t opaque[1024];
    } mlocSearcher;

    /// Makes a searcher for a UTF-8 pattern in a collator's order.
    ///
    /// @param searcherOut  Receives the searcher.
    /// @param collator     A collator mlocInitCollator made; copied.
    /// @param pattern      The pattern; may be null when length is 0.
    /// @param length       Its length in bytes.
    /// @param options      The options, or null for exact comparison.
    /// @return mloc_success; mloc_errorInvalid for a pattern with no
    ///         element at the collator's strength (none but ignorables,
    ///         or only variable ones when shifted), a collator with
    ///         numeric ordering (as ICU4C refuses one: a number's
    ///         elements stand for all its digits), an unknown comparison,
    ///         a collator the library did not make or a null pointer;
    ///         mloc_errorLimit for a pattern of more than
    ///         MLOC_MAX_SEARCH_PATTERN elements, or at the identical
    ///         strength NFD code points, or one whose elements and the room
    ///         a wildcard comparison keeps for the text's ignorables pass
    ///         MLOC_MAX_SEARCH_WINDOW.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitSearcher(mlocSearcher* searcherOut,
                                                        const mlocCollator* collator,
                                                        const char* pattern, size_t length,
                                                        const mlocSearchOptions* options);

    /// Makes a searcher for a UTF-16 pattern, as mlocInitSearcher does
    /// for its UTF-8 form. A lone surrogate reads as U+FFFD.
    ///
    /// @param searcherOut  Receives the searcher.
    /// @param collator     A collator mlocInitCollator made; copied.
    /// @param pattern      The pattern; may be null when length is 0.
    /// @param length       Its length in 16-bit units.
    /// @param options      The options, or null for exact comparison.
    /// @return As mlocInitSearcher.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitSearcherUtf16(mlocSearcher* searcherOut,
                                                             const mlocCollator* collator,
                                                             const uint16_t* pattern, size_t length,
                                                             const mlocSearchOptions* options);

    /// Finds the pattern in a UTF-8 text, the first match whose elements
    /// start at or after a position, as ICU4C's usearch_search from that
    /// offset: the search starts at the last boundary between the text's
    /// elements at or before the position, backing up first over code
    /// points a contraction may continue to. A position inside a code
    /// point counts as its start.
    ///
    /// @param searcher  A searcher mlocInitSearcher made.
    /// @param text      The text; may be null when length is 0.
    /// @param length    Its length in bytes.
    /// @param from      The position, at most length.
    /// @param startOut  Receives the match's first byte.
    /// @param endOut    Receives the byte after the match.
    /// @return mloc_success with a match; mloc_errorNotFound when there
    ///         is none; mloc_errorLimit when the text's elements
    ///         between a match's ends pass MLOC_MAX_SEARCH_WINDOW (a run
    ///         of ignorables too long); mloc_errorInvalid for a
    ///         searcher the library did not make, a position past the
    ///         end or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindNext(const mlocSearcher* searcher, const char* text,
                                                    size_t length, size_t from, size_t* startOut,
                                                    size_t* endOut);

    /// Finds the pattern in a UTF-16 text, as mlocFindNext does in its
    /// UTF-8 form. A lone surrogate reads as U+FFFD; a position on the
    /// second unit of a pair counts as the pair's first.
    ///
    /// @param searcher  A searcher mlocInitSearcher or
    ///                  mlocInitSearcherUtf16 made.
    /// @param text      The text; may be null when length is 0.
    /// @param length    Its length in 16-bit units.
    /// @param from      The position in units, at most length.
    /// @param startOut  Receives the match's first unit.
    /// @param endOut    Receives the unit after the match.
    /// @return As mlocFindNext.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindNextUtf16(const mlocSearcher* searcher,
                                                         const uint16_t* text, size_t length,
                                                         size_t from, size_t* startOut,
                                                         size_t* endOut);

    /// Finds the pattern in a UTF-8 text walking back from a position,
    /// the match nearest before it, as ICU4C's usearch_searchBackwards
    /// from that offset: the walk starts after the grapheme cluster at
    /// the position (at the last boundary between the text's elements
    /// at or before that cluster's end), and a match's last element
    /// starts before the position. A match with nothing after it in the
    /// walk ends at the position at the latest. A position inside a code
    /// point counts as its start.
    ///
    /// @param searcher  A searcher mlocInitSearcher made.
    /// @param text      The text; may be null when length is 0.
    /// @param length    Its length in bytes.
    /// @param from      The position, at most length.
    /// @param startOut  Receives the match's first byte.
    /// @param endOut    Receives the byte after the match.
    /// @return mloc_success with a match; mloc_errorNotFound when there
    ///         is none; mloc_errorLimit when the text's elements
    ///         between a match's ends pass MLOC_MAX_SEARCH_WINDOW, or a
    ///         run of more than MLOC_MAX_SEARCH_BACKWARD_RUN elements
    ///         without a primary weight or of unsafe code points lies in
    ///         the walk;
    ///         mloc_errorInvalid for a searcher the library did not make,
    ///         a position past the end or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindPrevious(const mlocSearcher* searcher,
                                                        const char* text, size_t length,
                                                        size_t from, size_t* startOut,
                                                        size_t* endOut);

    /// Finds the pattern in a UTF-16 text walking back, as
    /// mlocFindPrevious does in its UTF-8 form. A lone surrogate reads as
    /// U+FFFD; a position on the second unit of a pair counts as the
    /// pair's first.
    ///
    /// @param searcher  A searcher mlocInitSearcher or
    ///                  mlocInitSearcherUtf16 made.
    /// @param text      The text; may be null when length is 0.
    /// @param length    Its length in 16-bit units.
    /// @param from      The position in units, at most length.
    /// @param startOut  Receives the match's first unit.
    /// @param endOut    Receives the unit after the match.
    /// @return As mlocFindPrevious.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindPreviousUtf16(const mlocSearcher* searcher,
                                                             const uint16_t* text, size_t length,
                                                             size_t from, size_t* startOut,
                                                             size_t* endOut);

#ifdef __cplusplus
}
#endif

#endif
