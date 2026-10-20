// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin compare (collation_fast_compare.h), ICU4C 79's
// CollationFastLatin over UTF-8 with one reader for each side.

#include "collation_fast_compare.h"

#include "collation_fast_latin.h"
#include "collation_reorder.h"

#include "maul-unicode/properties.h"

enum
{
    commonSecPlusOffset = fastCommonSec + fastSecOffset,
    commonTerPlusOffset = fastCommonTer + fastTerOffset,
    caseAndTertiaryMask = fastCaseMask | fastTertiaryMask,
    // A text's two-byte lead bytes in the table: U+0080 to U+017F.
    latinMaxUtf8Lead = 0xC5,
};

static const uint32_t twoShortPrimariesMask =
    (uint32_t)fastShortPrimaryMask << 16 | fastShortPrimaryMask;
static const uint32_t twoLongPrimariesMask =
    (uint32_t)fastLongPrimaryMask << 16 | fastLongPrimaryMask;
static const uint32_t twoSecondariesMask = (uint32_t)fastSecondaryMask << 16 | fastSecondaryMask;
static const uint32_t twoCasesMask = (uint32_t)fastCaseMask << 16 | fastCaseMask;
static const uint32_t twoTertiariesMask = (uint32_t)fastTertiaryMask << 16 | fastTertiaryMask;
static const uint32_t twoSecOffsets = (uint32_t)fastSecOffset << 16 | fastSecOffset;
static const uint32_t twoCommonSecPlusOffset =
    (uint32_t)commonSecPlusOffset << 16 | commonSecPlusOffset;
static const uint32_t twoLowerCases = (uint32_t)fastLowerCase << 16 | fastLowerCase;
static const uint32_t twoTerOffsets = (uint32_t)fastTerOffset << 16 | fastTerOffset;
// U+FFFF's mini CE.
static const uint32_t maxMini = fastMaxShort | fastCommonSec | fastLowerCase | fastCommonTer;

typedef enum Level
{
    levelPrimary,
    levelSecondary,
    levelCase,
    levelTertiary,
    levelQuaternary,
} Level;

// One text as a level reads it: the next byte and the pair of mini CEs
// not yet compared, the current one in the low 16 bits.
typedef struct Side
{
    const uint8_t* s;
    size_t at;
    size_t length;
    uint32_t pair;
} Side;

static uint32_t Word(const uint8_t* words, uint32_t index)
{
    return mlocFastLatinWord(words, index);
}

// The mini CE of a character from a lead byte above ASCII: U+2000 to
// U+203F, U+FFFE and U+FFFF; a bail-out for any other or for bytes that
// are not one of them.
static uint32_t LookupThree(const uint8_t* words, uint32_t c, Side* x)
{
    if (x->at + 1 < x->length)
    {
        uint32_t t1 = x->s[x->at];
        uint32_t t2 = x->s[x->at + 1];
        x->at += 2;
        if (c == 0xE2 && t1 == 0x80 && t2 >= 0x80 && t2 <= 0xBF)
        {
            return Word(words, fastLatinLimit - 0x80 + t2);
        }
        if (c == 0xEF && t1 == 0xBF && (t2 == 0xBE || t2 == 0xBF))
        {
            return t2 == 0xBE ? fastMergeWeight : maxMini;
        }
    }
    return fastBailOut;
}

// The mini CE of a character the first level read, so known to be one
// the table holds.
static uint32_t LookupKnown(const uint8_t* words, uint32_t c, Side* x)
{
    if (c <= 0x7F)
    {
        return Word(words, c);
    }
    if (c <= latinMaxUtf8Lead)
    {
        return Word(words, ((c - 0xC2) << 6) + x->s[x->at++]);
    }
    uint32_t t2 = x->s[x->at + 1];
    x->at += 2;
    return c == 0xE2    ? Word(words, fastLatinLimit - 0x80 + t2)
           : t2 == 0xBE ? fastMergeWeight
                        : maxMini;
}

// The index of the character at the reader in the table, after which
// *next is; -1 for U+FFFE and U+FFFF, which no contraction holds;
// fastLatinChars + 1 for one the table does not hold.
static int32_t NextIndex(const Side* x, size_t* next)
{
    size_t i = x->at;
    uint32_t c = x->s[i++];
    if (c <= 0x7F)
    {
        *next = i;
        return (int32_t)c;
    }
    if (c >= 0xC2 && c <= latinMaxUtf8Lead && i != x->length && x->s[i] >= 0x80 && x->s[i] <= 0xBF)
    {
        *next = i + 1;
        return (int32_t)(((c - 0xC2) << 6) + x->s[i]);
    }
    if (i + 1 < x->length)
    {
        uint32_t t1 = x->s[i];
        uint32_t t2 = x->s[i + 1];
        *next = i + 2;
        if (c == 0xE2 && t1 == 0x80 && t2 >= 0x80 && t2 <= 0xBF)
        {
            return (int32_t)(fastLatinLimit - 0x80 + t2);
        }
        if (c == 0xEF && t1 == 0xBF && (t2 == 0xBE || t2 == 0xBF))
        {
            return -1;
        }
    }
    return fastLatinChars + 1;
}

// A character's pair of mini CEs from its word: an expansion's two, a
// contraction's result for the character after it (which it then
// takes) or its default; a bail-out where the list says so or the next
// character is not in the table.
static uint32_t NextPair(const uint8_t* words, uint32_t mini, Side* x)
{
    if (mini >= fastMinLong || mini < fastContraction)
    {
        return mini;
    }
    uint32_t index = fastLatinChars + (mini & fastIndexMask);
    if (mini >= fastExpansion)
    {
        return Word(words, index + 1) << 16 | Word(words, index);
    }
    if (x->at != x->length)
    {
        size_t next = 0;
        int32_t c2 = NextIndex(x, &next);
        if (c2 > fastLatinChars)
        {
            return fastBailOut;
        }
        // The list rises by its characters; the default comes first.
        uint32_t i = index;
        uint32_t head = Word(words, i);
        int32_t c = 0;
        do
        {
            i += head >> fastContractionLengthShift;
            head = Word(words, i);
            c = (int32_t)(head & fastContractionCharMask);
        } while (c < c2);
        if (c == c2)
        {
            index = i;
            x->at = next;
        }
    }
    uint32_t length = Word(words, index) >> fastContractionLengthShift;
    if (length == 1)
    {
        return fastBailOut;
    }
    uint32_t first = Word(words, index + 1);
    return length == 2 ? first : Word(words, index + 2) << 16 | first;
}

static uint32_t Primaries(uint32_t variableTop, uint32_t pair)
{
    uint32_t mini = pair & 0xFFFF;
    if (mini >= fastMinShort)
    {
        return pair & twoShortPrimariesMask;
    }
    if (mini > variableTop)
    {
        return pair & twoLongPrimariesMask;
    }
    return mini >= fastMinLong ? 0 : pair;
}

static uint32_t SecondariesOfShort(uint32_t mini)
{
    uint32_t s = mini & fastSecondaryMask;
    return s < fastMinSecHigh ? s + fastSecOffset : (s + fastSecOffset) << 16 | commonSecPlusOffset;
}

static uint32_t Secondaries(uint32_t variableTop, uint32_t pair)
{
    uint32_t mini = pair & 0xFFFF;
    if (pair <= 0xFFFF)
    {
        return mini >= fastMinShort  ? SecondariesOfShort(mini)
               : mini > variableTop  ? commonSecPlusOffset
               : mini >= fastMinLong ? 0
                                     : pair;
    }
    return mini >= fastMinShort ? (pair & twoSecondariesMask) + twoSecOffsets
           : mini > variableTop ? twoCommonSecPlusOffset
                                : 0;
}

// Primary and case level: a primary ignorable's case is ignored;
// otherwise a secondary ignorable's (which the table has none of).
static uint32_t Cases(uint32_t variableTop, bool primaryStrength, uint32_t pair)
{
    uint32_t mini = pair & 0xFFFF;
    if (pair <= 0xFFFF)
    {
        if (mini >= fastMinShort)
        {
            // A high secondary is a second element, a secondary one.
            bool second = !primaryStrength && (mini & fastSecondaryMask) >= fastMinSecHigh;
            return (mini & fastCaseMask) | (second ? (uint32_t)fastLowerCase << 16 : 0);
        }
        return mini > variableTop ? fastLowerCase : mini >= fastMinLong ? 0 : pair;
    }
    if (mini >= fastMinShort)
    {
        bool secondIgnorable = (pair & ((uint32_t)fastShortPrimaryMask << 16)) == 0;
        return pair & (primaryStrength && secondIgnorable ? fastCaseMask : twoCasesMask);
    }
    return mini > variableTop ? twoLowerCases : 0;
}

static uint32_t Tertiaries(uint32_t variableTop, bool withCase, uint32_t pair)
{
    uint32_t mini = pair & 0xFFFF;
    if (pair <= 0xFFFF)
    {
        if (mini >= fastMinShort)
        {
            bool second = (mini & fastSecondaryMask) >= fastMinSecHigh;
            uint32_t mask = withCase ? caseAndTertiaryMask : fastTertiaryMask;
            uint32_t secondWeight =
                withCase ? fastLowerCase | commonTerPlusOffset : commonTerPlusOffset;
            return ((mini & mask) + fastTerOffset) | (second ? secondWeight << 16 : 0);
        }
        if (mini > variableTop)
        {
            return ((mini & fastTertiaryMask) + fastTerOffset) | (withCase ? fastLowerCase : 0);
        }
        return mini >= fastMinLong ? 0 : pair;
    }
    if (mini >= fastMinShort)
    {
        return (pair & (withCase ? twoCasesMask | twoTertiariesMask : twoTertiariesMask)) +
               twoTerOffsets;
    }
    if (mini > variableTop)
    {
        return ((pair & twoTertiariesMask) + twoTerOffsets) | (withCase ? twoLowerCases : 0);
    }
    return 0;
}

// A variable element's primary, else the highest primary for one that
// is not ignorable.
static uint32_t Quaternaries(uint32_t variableTop, uint32_t pair)
{
    uint32_t mini = pair & 0xFFFF;
    if (pair <= 0xFFFF)
    {
        if (mini >= fastMinShort)
        {
            return (mini & fastSecondaryMask) >= fastMinSecHigh ? twoShortPrimariesMask
                                                                : fastShortPrimaryMask;
        }
        // U+FFFE's merge weight is no variable: above every primary, as
        // the general compare ranks it (ICU4C's fast compare leaves it
        // lowest, where its sort keys do not).
        return mini > variableTop || mini == fastMergeWeight ? fastShortPrimaryMask
               : mini >= fastMinLong                         ? pair & fastLongPrimaryMask
                                                             : pair;
    }
    return mini > variableTop ? twoShortPrimariesMask : pair & twoLongPrimariesMask;
}

// Whether a pair of primaries holds a digit's under a reordering that
// moves digits.
static bool HoldsDigit(const FastLatinOptions* o, uint32_t pair)
{
    uint32_t low = pair & 0xFFFF;
    uint32_t high = pair >> 16;
    return (low >= o->digitLow && low <= o->digitHigh) ||
           (high >= o->digitLow && high <= o->digitHigh);
}

// The first level's next pair of primaries, reading and checking each
// character; false to bail out.
static bool NextPrimaries(const FastLatinOptions* o, Side* x)
{
    while (x->pair == 0)
    {
        if (x->at == x->length)
        {
            x->pair = fastEndOfString;
            return true;
        }
        uint32_t c = x->s[x->at++];
        uint32_t mini = 0;
        if (c <= 0x7F)
        {
            if (c - '0' <= 9 && o->digitsBail)
            {
                return false;
            }
            mini = Word(o->words, c);
        }
        else if (c >= 0xC2 && c <= latinMaxUtf8Lead && x->at != x->length && x->s[x->at] >= 0x80 &&
                 x->s[x->at] <= 0xBF)
        {
            mini = Word(o->words, ((c - 0xC2) << 6) + x->s[x->at++]);
        }
        else
        {
            mini = LookupThree(o->words, c, x);
        }
        if (mini >= fastMinShort)
        {
            x->pair = mini & fastShortPrimaryMask;
        }
        else if (mini > o->variableTop)
        {
            x->pair = mini & fastLongPrimaryMask;
        }
        else
        {
            mini = NextPair(o->words, mini, x);
            if (mini == fastBailOut)
            {
                return false;
            }
            x->pair = Primaries(o->variableTop, mini);
        }
    }
    return !HoldsDigit(o, x->pair);
}

// A later level's next pair of weights.
static void NextWeights(const FastLatinOptions* o, Level level, Side* x)
{
    while (x->pair == 0)
    {
        if (x->at == x->length)
        {
            x->pair = fastEndOfString;
            return;
        }
        uint32_t c = x->s[x->at++];
        uint32_t mini = LookupKnown(o->words, c, x);
        if (mini < fastMinLong)
        {
            mini = NextPair(o->words, mini, x);
        }
        switch (level)
        {
        case levelSecondary:
            x->pair = Secondaries(o->variableTop, mini);
            break;
        case levelCase:
            x->pair = Cases(o->variableTop, o->strength == collationPrimary, mini);
            break;
        case levelTertiary:
            x->pair = Tertiaries(o->variableTop, o->tertiaryCase, mini);
            break;
        default:
            x->pair = Quaternaries(o->variableTop, mini);
            break;
        }
    }
}

// The order of two differing weights at a level, or fastLatinBail.
static int Order(const FastLatinOptions* o, Level level, uint32_t a, uint32_t b)
{
    if (level == levelSecondary && o->backwards)
    {
        return fastLatinBail;
    }
    if (level == levelCase && o->upperFirst)
    {
        return a < b ? 1 : -1;
    }
    if (level == levelTertiary && o->tertiaryUpperFirst)
    {
        a ^= a > fastMergeWeight ? fastCaseMask : 0;
        b ^= b > fastMergeWeight ? fastCaseMask : 0;
    }
    return a < b ? -1 : 1;
}

// One level over both texts: -1, 0, 1 or fastLatinBail.
static int CompareLevel(const FastLatinOptions* o, Level level, Side x, Side y)
{
    for (;;)
    {
        if (level == levelPrimary)
        {
            if (!NextPrimaries(o, &x) || !NextPrimaries(o, &y))
            {
                return fastLatinBail;
            }
        }
        else
        {
            NextWeights(o, level, &x);
            NextWeights(o, level, &y);
        }
        if (x.pair == y.pair)
        {
            if (x.pair == fastEndOfString)
            {
                return 0;
            }
            x.pair = 0;
            y.pair = 0;
            continue;
        }
        uint32_t a = x.pair & 0xFFFF;
        uint32_t b = y.pair & 0xFFFF;
        if (a != b)
        {
            return Order(o, level, a, b);
        }
        if (x.pair == fastEndOfString)
        {
            return 0;
        }
        x.pair >>= 16;
        y.pair >>= 16;
    }
}

int mlocFastLatinCompare(const FastLatinOptions* options, const uint8_t* a, size_t aLength,
                         const uint8_t* b, size_t bLength)
{
    Side x = {a, 0, aLength, 0};
    Side y = {b, 0, bLength, 0};
    int order = CompareLevel(options, levelPrimary, x, y);
    if (order == 0 && options->strength >= collationSecondary)
    {
        order = CompareLevel(options, levelSecondary, x, y);
    }
    if (order == 0 && options->caseLevel)
    {
        order = CompareLevel(options, levelCase, x, y);
    }
    if (order == 0 && options->strength >= collationTertiary)
    {
        order = CompareLevel(options, levelTertiary, x, y);
    }
    if (order == 0 && options->strength >= collationQuaternary)
    {
        order = CompareLevel(options, levelQuaternary, x, y);
    }
    return order;
}

// A group's or script's first primary after the reordering; 0 when the
// root has none.
static uint32_t ReorderedStart(const CollationRoot* root, const CollationReorder* reorder,
                               uint32_t code)
{
    uint32_t index = mlocCollationScriptIndex(root, code);
    uint32_t start = index == 0 ? 0 : mlocCollationStart(root, index) << 16;
    return start == 0 ? 0 : mlocCollationReorderPrimary(reorder, start);
}

// ICU4C's check of a reordering: false when it moves the groups before
// Latin out of their order; *digitsOut when it moves digits apart.
static bool ReorderServed(const CollationRoot* root, const CollationReorder* reorder,
                          bool* digitsOut)
{
    uint32_t previous = 0;
    uint32_t beforeDigit = 0;
    uint32_t digit = 0;
    for (uint32_t group = collationGroupSpace; group <= collationGroupDigit; group++)
    {
        uint32_t start = ReorderedStart(root, reorder, group);
        if (group == collationGroupDigit)
        {
            beforeDigit = previous;
            digit = start;
        }
        else if (start != 0)
        {
            if (start < previous)
            {
                return false;
            }
            previous = start;
        }
    }
    uint32_t latin = ReorderedStart(root, reorder, MUNI_SCRIPT('L', 'a', 't', 'n'));
    if (latin < previous)
    {
        return false;
    }
    // Digits stay between the group before them and Latin, which follows
    // them in the root.
    *digitsOut = !(beforeDigit < digit && digit < latin);
    return true;
}

// The mini primaries from 0's to 9's, which every digit's primary in the
// table is among; false when a digit's word is not a plain primary.
static bool DigitRange(const uint8_t* table, uint32_t* lowOut, uint32_t* highOut)
{
    uint32_t primaries[2];
    for (int i = 0; i < 2; i++)
    {
        uint32_t word = mlocFastLatinWord(table, fastLatinHeader + (i == 0 ? '0' : '9'));
        if (word < fastMinLong)
        {
            return false;
        }
        primaries[i] = word & (word >= fastMinShort ? fastShortPrimaryMask : fastLongPrimaryMask);
    }
    *lowOut = primaries[0];
    *highOut = primaries[1];
    return true;
}

bool mlocFastLatinOptions(const CollationRoot* root, const CollationSettings* settings,
                          const uint8_t* table, uint32_t count, FastLatinOptions* out)
{
    if (table == nullptr || count < fastLatinHeader + fastLatinChars ||
        settings->strength >= collationIdentical)
    {
        return false;
    }
    uint32_t variableTop = fastMinLong - 1;
    if (settings->shifted)
    {
        uint32_t group = 0;
        while (group < fastLatinHeader - 1 &&
               mlocCollationLastPrimary(root, collationGroupSpace + group) != settings->variableTop)
        {
            group++;
        }
        if (group == fastLatinHeader - 1)
        {
            return false;
        }
        variableTop = mlocFastLatinWord(table, 1 + group) | fastTertiaryMask;
    }
    bool digitsReordered = false;
    if (settings->reorder != nullptr && !ReorderServed(root, settings->reorder, &digitsReordered))
    {
        return false;
    }
    uint32_t digitLow = 1;
    uint32_t digitHigh = 0;
    if (digitsReordered && !DigitRange(table, &digitLow, &digitHigh))
    {
        return false;
    }
    bool upper = settings->caseFirst == collationUpperFirst;
    *out = (FastLatinOptions){
        .words = table + (size_t)fastLatinHeader * 2,
        .variableTop = variableTop,
        .strength = settings->strength,
        .backwards = settings->backwards,
        .caseLevel = settings->caseLevel,
        .upperFirst = upper,
        .tertiaryCase = settings->caseFirst != collationCaseFirstOff && !settings->caseLevel,
        .tertiaryUpperFirst = upper && !settings->caseLevel,
        .digitsBail = settings->numeric || digitsReordered,
        .digitLow = digitLow,
        .digitHigh = digitHigh,
    };
    return true;
}
