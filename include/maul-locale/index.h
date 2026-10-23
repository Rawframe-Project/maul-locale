// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Alphabetic index: the labels of a sorted list's sections (A, B, C... in
// English; あ, か, さ... in Japanese) and the section each name falls into,
// under a collator, as ICU4C's AlphabeticIndex builds its immutable index.
// The labels are the collator's U+FDD0 contractions (the Chinese types:
// pinyin letters, stroke counts, zhuyin, radicals), else the locale's
// index labels; those that sort as their parts or past the scripts are
// dropped, and an underflow, an overflow and an inflow bucket for each run
// of scripts skipped surround them. The caller sorts and keeps its own
// names: the index gives the buckets in order and a name's bucket.
//
// The index is built into caller memory of any alignment and keeps a
// copy of the collator at primary strength; its bytes may be copied or
// moved, and the data the collator reads must outlive it.

#ifndef MAUL_LOCALE_INDEX_H
#define MAUL_LOCALE_INDEX_H

#include "maul-locale/collate.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A bucket's kind, as ICU4C's UAlphabeticIndexLabelType numbers it.
    typedef enum mlocIndexLabelType
    {
        // A label's bucket.
        mloc_indexNormal = 0,
        // Names before the first label.
        mloc_indexUnderflow = 1,
        // Names of the scripts between two labels' scripts.
        mloc_indexInflow = 2,
        // Names past the last label's script.
        mloc_indexOverflow = 3,
    } mlocIndexLabelType;

    // An index's options; the zero value gives ICU4C's defaults.
    typedef struct mlocIndexOptions
    {
        // The most labels kept, every n-th dropped past it; 0 for 99.
        uint32_t maxLabelCount;
        // The underflow, inflow and overflow buckets' labels, UTF-8; null
        // for "…" (U+2026).
        const char* underflowLabel;
        size_t underflowLength;
        const char* inflowLabel;
        size_t inflowLength;
        const char* overflowLabel;
        size_t overflowLength;
    } mlocIndexOptions;

    // A bucket as the index shows it.
    typedef struct mlocIndexBucket
    {
        // Its label, UTF-8 inside the index; not NUL-terminated.
        const char* label;
        size_t length;
        mlocIndexLabelType type;
    } mlocIndexBucket;

    /// Builds an index of a collator's order with a locale's labels. Call
    /// with a null buffer and a capacity of 0 to learn the capacity
    /// needed.
    ///
    /// @param collator  A collator mlocInitCollator made; copied.
    /// @param data      The data the locale's labels are found in (the
    ///                  collation data file); read while building.
    /// @param locale    The locale whose labels the index takes when the
    ///                  collator has no U+FDD0 contractions.
    /// @param options   The options, or null for the defaults.
    /// @param buffer    Receives the index; any alignment. May be null
    ///                  when capacity is 0.
    /// @param capacity  The buffer's size in bytes.
    /// @param sizeOut   Receives the index's size on success, or the
    ///                  capacity needed with mloc_errorCapacity, which may
    ///                  be a little more than the size.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         small; mloc_errorData for more than 512 labels or a root
    ///         without script boundaries; mloc_errorInvalid for a
    ///         collator the library did not make, a special label that
    ///         is not UTF-8 or longer than 255 bytes, or a null pointer.
    ///         The data giving no labels for the locale is no error: the
    ///         index then has its underflow bucket alone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocBuildIndex(const mlocCollator* collator,
                                                      const mlocDataSet* data,
                                                      const mlocLocale* locale,
                                                      const mlocIndexOptions* options, void* buffer,
                                                      size_t capacity, size_t* sizeOut);

    /// The number of buckets an index shows.
    ///
    /// @param index    An index mlocBuildIndex built.
    /// @param countOut Receives the count, 1 or more.
    /// @return mloc_success; mloc_errorInvalid for bytes mlocBuildIndex
    ///         did not write or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetIndexBucketCount(const void* index,
                                                               uint32_t* countOut);

    /// A bucket of an index, in the index's order.
    ///
    /// @param index      An index mlocBuildIndex built.
    /// @param bucket     The bucket's number, below the count.
    /// @param bucketOut  Receives its label, pointing into the index, and
    ///                   its type.
    /// @return mloc_success; mloc_errorInvalid for a number past the
    ///         buckets, bytes mlocBuildIndex did not write or a null
    ///         pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetIndexBucket(const void* index, uint32_t bucket,
                                                          mlocIndexBucket* bucketOut);

    /// The bucket a UTF-8 name falls into: the last whose lower boundary
    /// it sorts at or after at primary strength, as ICU4C's
    /// getBucketIndex finds it.
    ///
    /// @param index      An index mlocBuildIndex built.
    /// @param name       The name; may be null when length is 0.
    /// @param length     Its length in bytes.
    /// @param bucketOut  Receives the bucket's number.
    /// @return mloc_success; mloc_errorInvalid for bytes mlocBuildIndex
    ///         did not write or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindIndexBucket(const void* index, const char* name,
                                                           size_t length, uint32_t* bucketOut);

    /// The bucket a UTF-16 name falls into, as mlocFindIndexBucket finds
    /// its UTF-8 form's. A lone surrogate reads as U+FFFD.
    ///
    /// @param index      An index mlocBuildIndex built.
    /// @param name       The name; may be null when length is 0.
    /// @param length     Its length in 16-bit units.
    /// @param bucketOut  Receives the bucket's number.
    /// @return As mlocFindIndexBucket.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindIndexBucketUtf16(const void* index,
                                                                const uint16_t* name, size_t length,
                                                                uint32_t* bucketOut);

#ifdef __cplusplus
}
#endif

#endif
