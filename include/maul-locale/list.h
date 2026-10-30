// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lists (UTS #35 Part 2): items joined by a locale's list patterns ("a, b,
// and c", "a, b o c"), as ICU4C's ListFormatter and ECMA-402's
// Intl.ListFormat join them, with ICU's contextual changes (Spanish "e"
// and "u", Hebrew's vav before a word of another script). The patterns are
// compiled in.

#ifndef MAUL_LOCALE_LIST_H
#define MAUL_LOCALE_LIST_H

#include "maul-locale/locale.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // What a list says of its items: all of them ("a, b, and c"), one of
    // them ("a, b, or c"), or amounts of units together ("3 ft, 7 in").
    typedef int32_t mlocListType;
    enum
    {
        mloc_listAnd = 0,
        mloc_listOr = 1,
        mloc_listUnit = 2,
    };

    // How much room the list takes: its words written out ("a, b, and
    // c"), short ("a, b, & c" in English), or narrow ("a, b, c").
    typedef int32_t mlocListWidth;
    enum
    {
        mloc_listWide = 0,
        mloc_listShort = 1,
        mloc_listNarrow = 2,
    };

    // The parts of a list (mlocOutput's part function): an item, or the
    // text the patterns put between items. An empty item has no part.
    typedef int32_t mlocListPart;
    enum
    {
        mloc_listPartElement = 0,
        mloc_listPartLiteral = 1,
    };

// The most items a list takes: the formatter counts them in 32 bits.
#define MLOC_MAX_LIST_ITEMS 4294967295u

    // A list formatter's options; the zero value is the default (and,
    // wide).
    typedef struct mlocListOptions
    {
        mlocListType type;
        mlocListWidth width;
    } mlocListOptions;

    // A list formatter: a locale's patterns of a type and width, in the
    // data it was made from, which must outlive it. Opaque; make it with
    // mlocInitListFormatter.
    typedef struct mlocListFormatter
    {
        uint64_t opaque[8];
    } mlocListFormatter;

    // An item of a list: UTF-8 text, written as it is.
    typedef struct mlocListItem
    {
        const char* text;
        size_t length;
    } mlocListItem;

    /// Makes a locale's list formatter of a type and width. The patterns
    /// are the first the locale's chain has, root's last.
    ///
    /// @param data          The data with the list units, the parents and
    ///                      the likely subtags.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_errorNotFound for data without the list
    ///         units; mloc_errorInvalid for a null pointer or options out
    ///         of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitListFormatter(const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const mlocListOptions* options,
                                                             mlocListFormatter* formatterOut);

    /// Writes items joined by a formatter's patterns: none as no text, one
    /// as itself, two by the locale's pattern for two, more by its start,
    /// middle and end patterns. Parts are mlocListPart values.
    ///
    /// @param formatter  A formatter mlocInitListFormatter made.
    /// @param items      The items, count of them; may be null when count
    ///                   is 0.
    /// @param count      The number of items, at most MLOC_MAX_LIST_ITEMS.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (textOut->length then the size needed); mloc_errorLimit for
    ///         a count past MLOC_MAX_LIST_ITEMS; mloc_errorInvalid for a
    ///         formatter the library did not make, a null pointer, or an
    ///         item with a null text and a length.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocListFormatter_Format(const mlocListFormatter* formatter,
                                                                const mlocListItem* items,
                                                                size_t count, mlocOutput* textOut);

    // A person's gender, as a message's gender select reads it: other
    // (unknown or neither), male, female. Not a grammatical gender
    // (mlocGender).
    typedef int32_t mlocPersonGender;
    enum
    {
        mloc_personOther = 0,
        mloc_personMale = 1,
        mloc_personFemale = 2,
    };

    /// Gives the gender of a list of persons ("Tom and Mary") by the locale's
    /// rule in CLDR's genderList.xml; the rules are built in. The locale is
    /// canonicalized with the data set's aliases first (iw is he), then read by
    /// its language and script ("zh_Hant"), then its language; a locale CLDR
    /// does not list is neutral. An empty list is other and a list of one its
    /// person's gender; a longer list is other in a neutral locale (en, de), in
    /// a mixed neutral one (el, is) male or female when every person is, else
    /// other, and in a male-taints one (fr, es, ar) female when every person
    /// is, else male.
    ///
    /// @param data       The data set the locale's aliases come from.
    /// @param locale     The locale.
    /// @param genders    The persons' genders; null when count is 0.
    /// @param count      The number of persons.
    /// @param genderOut  Receives the list's gender. May be one of genders.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or a
    ///         gender past mloc_personFemale.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetListGender(const mlocDataSet* data,
                                                         const mlocLocale* locale,
                                                         const mlocPersonGender* genders,
                                                         size_t count, mlocPersonGender* genderOut);

#ifdef __cplusplus
}
#endif

#endif
