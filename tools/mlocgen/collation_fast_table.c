// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin table (collation_fast_table.h): ICU4C 79's
// CollationFastLatinBuilder over elements the lookup gives.

#include "collation_fast_table.h"

#include "collation_builder.h"
#include "collation_fast_latin.h"
#include "table_gen.h"

#include "maul-unicode/properties.h"

#include <stdlib.h>
#include <string.h>

// ICU's Collation constants the builder reads.
static const uint64_t noCe = 0x101000100;
static const uint32_t noCePrimary = 1;
static const uint32_t commonSecondaryCe = 0x05000000;
static const uint32_t secondaryAndCaseMask = 0xFFFFC000;
static const uint32_t onlyTertiaryMask = 0x3F3F;
static const uint32_t commonWeight16 = 0x0500;
static const uint64_t caseMask = 0xC000;
static const uint64_t quaternaryMask = 0xC0;
static const uint64_t contractionFlag = 0x80000000;

enum
{
    // space, punct, symbol, currency
    specialGroups = 4,
    maxNfd = 8,
};

typedef struct Builder
{
    CollationMappings* m;
    uint64_t ce0;
    uint64_t ce1;
    uint64_t charCes[fastLatinChars][2];
    // Contraction entries: the next character's index, two elements.
    uint64_t* contractions;
    size_t contractionCount;
    size_t contractionCapacity;
    uint64_t* unique;
    size_t uniqueCount;
    size_t uniqueCapacity;
    uint16_t* miniCes;
    uint32_t lastSpecialPrimaries[specialGroups];
    uint32_t firstDigitPrimary;
    uint32_t firstLatinPrimary;
    uint32_t lastLatinPrimary;
    uint32_t firstShortPrimary;
    bool shortPrimaryOverflow;
    uint16_t* result;
    size_t resultCount;
    size_t resultCapacity;
    // Each fast character's NFD.
    uint32_t nfd[fastLatinChars][maxNfd];
    uint32_t nfdLength[fastLatinChars];
    // Whether a code point of its NFD is after a prefix in some mapping,
    // or starts a contraction.
    bool prefixed[fastLatinChars];
    bool contracts[fastLatinChars];
} Builder;

static void* Grow(void* items, size_t* capacity, size_t needed, size_t size)
{
    if (needed <= *capacity)
    {
        return items;
    }
    *capacity = needed * 2 + 64;
    void* grown = realloc(items, *capacity * size);
    if (grown == nullptr)
    {
        GenDie("fast Latin", "out of memory");
    }
    return grown;
}

static void Append(Builder* b, uint32_t word)
{
    b->result = Grow(b->result, &b->resultCapacity, b->resultCount + 1, sizeof *b->result);
    b->result[b->resultCount++] = (uint16_t)word;
}

static uint32_t CharAt(int32_t i)
{
    return i < fastLatinLimit ? (uint32_t)i : (uint32_t)(i - fastLatinLimit + fastPunctStart);
}

// A code point's NFD, which for a fast character is short.
static uint32_t Nfd(uint32_t c, uint32_t* out)
{
    uint32_t nfd[collationNfdMax];
    int32_t n = CollationNfd(&c, 1, nfd);
    if (n <= 0 || n > maxNfd)
    {
        return 0;
    }
    memcpy(out, nfd, (size_t)n * sizeof *out);
    return (uint32_t)n;
}

// The elements of a text, at most collationMaxExpansion; their count.
static uint32_t Elements(Builder* b, const uint32_t* text, uint32_t length, uint64_t* out)
{
    return CollationMappingsElements(b->m, text, length, 0, out, 0);
}

static bool InSameGroup(const Builder* b, uint32_t p, uint32_t q)
{
    if (p >= b->firstShortPrimary)
    {
        return q >= b->firstShortPrimary;
    }
    if (q >= b->firstShortPrimary)
    {
        return false;
    }
    uint32_t lastVariablePrimary = b->lastSpecialPrimaries[specialGroups - 1];
    if (p > lastVariablePrimary)
    {
        return q > lastVariablePrimary;
    }
    if (q > lastVariablePrimary)
    {
        return false;
    }
    for (int i = 0;; i++)
    {
        uint32_t lastPrimary = b->lastSpecialPrimaries[i];
        if (p <= lastPrimary)
        {
            return q <= lastPrimary;
        }
        if (q <= lastPrimary)
        {
            return false;
        }
    }
}

// ICU's getCEsFromCE32 after its expansion cases: whether elements fit,
// left in ce0 and ce1.
static bool Fits(Builder* b, const uint64_t* elements, uint32_t count)
{
    if (count > 2)
    {
        return false;
    }
    b->ce0 = count > 0 ? elements[0] : 0;
    b->ce1 = count > 1 ? elements[1] : 0;
    uint64_t ce0 = b->ce0;
    uint64_t ce1 = b->ce1;
    if (ce0 == 0)
    {
        return ce1 == 0;
    }
    uint32_t p0 = (uint32_t)(ce0 >> 32);
    if (p0 == 0 || p0 > b->lastLatinPrimary)
    {
        return false;
    }
    uint32_t lower0 = (uint32_t)ce0;
    if (p0 < b->firstShortPrimary && (lower0 & secondaryAndCaseMask) != commonSecondaryCe)
    {
        return false;
    }
    if ((lower0 & onlyTertiaryMask) < commonWeight16)
    {
        return false;
    }
    if (ce1 != 0)
    {
        uint32_t p1 = (uint32_t)(ce1 >> 32);
        if (p1 == 0 ? p0 < b->firstShortPrimary : !InSameGroup(b, p0, p1))
        {
            return false;
        }
        uint32_t lower1 = (uint32_t)ce1;
        if ((lower1 >> 16) == 0)
        {
            return false;
        }
        if (p1 != 0 && p1 < b->firstShortPrimary &&
            (lower1 & secondaryAndCaseMask) != commonSecondaryCe)
        {
            return false;
        }
        if ((lower1 & onlyTertiaryMask) < commonWeight16)
        {
            return false;
        }
    }
    return ((ce0 | ce1) & quaternaryMask) == 0;
}

static int CompareUnsigned(uint64_t a, uint64_t b)
{
    return a < b ? -1 : a > b;
}

// ICU's binarySearch: the index, or ~ the insertion point.
static ptrdiff_t Search(const uint64_t* list, size_t limit, uint64_t ce)
{
    size_t start = 0;
    while (start < limit)
    {
        size_t i = start + (limit - start) / 2;
        int c = CompareUnsigned(ce, list[i]);
        if (c == 0)
        {
            return (ptrdiff_t)i;
        }
        if (c < 0)
        {
            limit = i;
        }
        else
        {
            start = i + 1;
        }
    }
    return ~(ptrdiff_t)start;
}

static void AddUnique(Builder* b, uint64_t ce)
{
    if (ce == 0 || (uint32_t)(ce >> 32) == noCePrimary)
    {
        return;
    }
    ce &= ~caseMask;
    ptrdiff_t i = Search(b->unique, b->uniqueCount, ce);
    if (i >= 0)
    {
        return;
    }
    size_t at = (size_t)~i;
    b->unique = Grow(b->unique, &b->uniqueCapacity, b->uniqueCount + 1, sizeof *b->unique);
    memmove(b->unique + at + 1, b->unique + at, (b->uniqueCount - at) * sizeof *b->unique);
    b->unique[at] = ce;
    b->uniqueCount++;
}

static void AddEntry(Builder* b, uint32_t x, uint64_t ce0, uint64_t ce1)
{
    b->contractions =
        Grow(b->contractions, &b->contractionCapacity, b->contractionCount + 3, sizeof(uint64_t));
    b->contractions[b->contractionCount++] = x;
    b->contractions[b->contractionCount++] = ce0;
    b->contractions[b->contractionCount++] = ce1;
    AddUnique(b, ce0);
    AddUnique(b, ce1);
}

static void Contexts(Builder* b, int32_t i)
{
    for (uint32_t k = 0; k < b->nfdLength[i]; k++)
    {
        bool prefixed = false;
        bool contracts = false;
        CollationMappingsContexts(b->m, b->nfd[i][k], &prefixed, &contracts);
        b->prefixed[i] = b->prefixed[i] || prefixed;
        b->contracts[i] = b->contracts[i] || contracts;
    }
}

// A contraction character's list: its default, then an entry for each
// fast character whose pair does not read as the two apart; none for a
// character after a prefix, which bails out itself (a longer prefix
// before it may change what it reads as).
static void ContractionList(Builder* b, int32_t i)
{
    size_t index = b->contractionCount;
    uint64_t alone[collationMaxExpansion];
    uint32_t aloneCount = Elements(b, b->nfd[i], b->nfdLength[i], alone);
    bool fits = Fits(b, alone, aloneCount);
    AddEntry(b, fastContractionCharMask, fits ? b->ce0 : noCe, fits ? b->ce1 : 0);
    for (int32_t x = 0; x < fastLatinChars; x++)
    {
        if (b->prefixed[x])
        {
            continue;
        }
        uint32_t pair[2 * maxNfd];
        uint32_t length = b->nfdLength[i];
        memcpy(pair, b->nfd[i], length * sizeof *pair);
        memcpy(pair + length, b->nfd[x], b->nfdLength[x] * sizeof *pair);
        length += b->nfdLength[x];
        uint64_t together[collationMaxExpansion];
        uint64_t apart[2 * collationMaxExpansion];
        uint32_t n = Elements(b, pair, length, together);
        uint32_t a = aloneCount;
        memcpy(apart, alone, a * sizeof *apart);
        uint32_t other = Elements(b, b->nfd[x], b->nfdLength[x], apart + a);
        bool same = n == a + other && memcmp(together, apart, n * sizeof *apart) == 0;
        if (CollationMappingsExtends(b->m, pair, length))
        {
            AddEntry(b, (uint32_t)x, noCe, 0);
        }
        else if (!same)
        {
            bool pairFits = Fits(b, together, n);
            AddEntry(b, (uint32_t)x, pairFits ? b->ce0 : noCe, pairFits ? b->ce1 : 0);
        }
    }
    if (b->contractionCount == index + 3 && b->nfdLength[i] > 1)
    {
        // A precomposed letter after which nothing reads otherwise: the
        // letter alone, as ICU's closure gives it its own elements. A
        // letter that is its own NFD keeps its list, as in ICU, where
        // its mapping is the contraction's.
        b->contractionCount = index;
        b->ce0 = !fits ? noCe : aloneCount > 0 ? alone[0] : 0;
        b->ce1 = fits && aloneCount > 1 ? alone[1] : 0;
        return;
    }
    b->ce0 = (uint64_t)noCePrimary << 32 | contractionFlag | index;
    b->ce1 = 0;
}

static bool IsContractionCharCe(uint64_t ce)
{
    return (uint32_t)(ce >> 32) == noCePrimary && ce != noCe;
}

static void GetCes(Builder* b)
{
    for (int32_t i = 0; i < fastLatinChars; i++)
    {
        uint64_t elements[collationMaxExpansion];
        bool ok = !b->prefixed[i];
        if (ok && b->contracts[i])
        {
            ContractionList(b, i);
        }
        else if (ok)
        {
            ok = Fits(b, elements, Elements(b, b->nfd[i], b->nfdLength[i], elements));
        }
        if (ok)
        {
            b->charCes[i][0] = b->ce0;
            b->charCes[i][1] = b->ce1;
            AddUnique(b, b->ce0);
            AddUnique(b, b->ce1);
        }
        else
        {
            b->charCes[i][0] = b->ce0 = noCe;
            b->charCes[i][1] = b->ce1 = 0;
        }
        if (i == 0 && !IsContractionCharCe(b->ce0))
        {
            // U+0000 always reads as a contraction.
            AddEntry(b, fastContractionCharMask, b->ce0, b->ce1);
            b->charCes[0][0] = (uint64_t)noCePrimary << 32 | contractionFlag;
            b->charCes[0][1] = 0;
        }
    }
    // The last list's end.
    b->contractions =
        Grow(b->contractions, &b->contractionCapacity, b->contractionCount + 1, sizeof(uint64_t));
    b->contractions[b->contractionCount++] = fastContractionCharMask;
}

static uint16_t MiniCe(const Builder* b, uint64_t ce)
{
    ptrdiff_t i = Search(b->unique, b->uniqueCount, ce & ~caseMask);
    return i < 0 ? fastBailOut : b->miniCes[i];
}

// The next mini primary, or false when there is no room.
static bool NextPrimary(Builder* b, uint32_t p, uint32_t* pri)
{
    if (p < b->firstShortPrimary)
    {
        if (*pri == 0)
        {
            *pri = fastMinLong;
        }
        else if (*pri < fastMaxLong)
        {
            *pri += fastLongInc;
        }
        else
        {
            return false;
        }
        return true;
    }
    if (*pri < fastMinShort)
    {
        *pri = fastMinShort;
    }
    else if (*pri < fastMaxShort - fastShortInc)
    {
        *pri += fastShortInc;
    }
    else
    {
        b->shortPrimaryOverflow = true;
        return false;
    }
    return true;
}

// The next mini secondary after s, or false when there is no room.
static bool NextSecondary(uint32_t pri, uint32_t s, uint32_t* sec)
{
    if (pri == 0)
    {
        if (*sec == 0)
        {
            *sec = fastMinSecHigh;
        }
        else if (*sec < fastMaxSecHigh)
        {
            *sec += fastSecInc;
        }
        else
        {
            return false;
        }
    }
    else if (s < commonWeight16)
    {
        if (*sec == fastCommonSec)
        {
            *sec = fastMinSecBefore;
        }
        else if (*sec < fastMaxSecBefore)
        {
            *sec += fastSecInc;
        }
        else
        {
            return false;
        }
    }
    else if (s == commonWeight16)
    {
        *sec = fastCommonSec;
    }
    else if (*sec < fastMinSecAfter)
    {
        *sec = fastMinSecAfter;
    }
    else if (*sec < fastMaxSecAfter)
    {
        *sec += fastSecInc;
    }
    else
    {
        return false;
    }
    return true;
}

// The head's top mini primary of each group that ends below p.
static void CloseGroups(Builder* b, uint32_t p, uint32_t pri, int* group,
                        uint32_t* lastGroupPrimary)
{
    while (p > *lastGroupPrimary)
    {
        b->result[1 + *group] = (uint16_t)pri;
        *lastGroupPrimary = ++*group < specialGroups ? b->lastSpecialPrimaries[*group] : 0xFFFFFFFF;
    }
}

static void EncodeUnique(Builder* b)
{
    free(b->miniCes);
    b->miniCes = GenAllocate((b->uniqueCount + 1) * sizeof *b->miniCes);
    int group = 0;
    uint32_t lastGroupPrimary = b->lastSpecialPrimaries[0];
    uint32_t prevPrimary = 0;
    uint32_t prevSecondary = 0;
    uint32_t pri = 0;
    uint32_t sec = 0;
    uint32_t ter = fastCommonTer;
    for (size_t i = 0; i < b->uniqueCount; i++)
    {
        uint64_t ce = b->unique[i];
        uint32_t p = (uint32_t)(ce >> 32);
        if (p != prevPrimary)
        {
            CloseGroups(b, p, pri, &group, &lastGroupPrimary);
            if (!NextPrimary(b, p, &pri))
            {
                b->miniCes[i] = fastBailOut;
                continue;
            }
            prevPrimary = p;
            prevSecondary = commonWeight16;
            sec = fastCommonSec;
            ter = fastCommonTer;
        }
        uint32_t lower = (uint32_t)ce;
        uint32_t s = lower >> 16;
        if (s != prevSecondary)
        {
            if (!NextSecondary(pri, s, &sec))
            {
                b->miniCes[i] = fastBailOut;
                continue;
            }
            prevSecondary = s;
            ter = fastCommonTer;
        }
        if ((lower & onlyTertiaryMask) > commonWeight16)
        {
            if (ter >= fastMaxTerAfter)
            {
                b->miniCes[i] = fastBailOut;
                continue;
            }
            ter++;
        }
        b->miniCes[i] =
            (uint16_t)(fastMinLong <= pri && pri <= fastMaxLong ? pri | ter : pri | sec | ter);
    }
}

// ICU's encodeTwoCEs: one mini CE in the low 16 bits, or two.
static uint32_t EncodeTwo(const Builder* b, uint64_t first, uint64_t second)
{
    if (first == 0)
    {
        return 0;
    }
    if (first == noCe)
    {
        return fastBailOut;
    }
    uint32_t mini = MiniCe(b, first);
    if (mini == fastBailOut)
    {
        return mini;
    }
    if (mini >= fastMinShort)
    {
        mini |= (((uint32_t)first & (uint32_t)caseMask) >> (14 - 3)) + fastLowerCase;
    }
    if (second == 0)
    {
        return mini;
    }
    uint32_t mini1 = MiniCe(b, second);
    if (mini1 == fastBailOut)
    {
        return mini1;
    }
    uint32_t case1 = (uint32_t)second & (uint32_t)caseMask;
    if (mini >= fastMinShort && (mini & fastSecondaryMask) == fastCommonSec)
    {
        uint32_t sec1 = mini1 & fastSecondaryMask;
        uint32_t ter1 = mini1 & fastTertiaryMask;
        if (sec1 >= fastMinSecHigh && case1 == 0 && ter1 == fastCommonTer)
        {
            return (mini & ~(uint32_t)fastSecondaryMask) | sec1;
        }
    }
    if (mini1 <= fastSecondaryMask || fastMinShort <= mini1)
    {
        mini1 |= (case1 >> (14 - 3)) + fastLowerCase;
    }
    return mini << 16 | mini1;
}

static void EncodeChars(Builder* b)
{
    size_t start = b->resultCount;
    for (int32_t i = 0; i < fastLatinChars; i++)
    {
        Append(b, 0);
    }
    size_t base = b->resultCount;
    for (int32_t i = 0; i < fastLatinChars; i++)
    {
        uint64_t ce = b->charCes[i][0];
        if (IsContractionCharCe(ce))
        {
            continue;
        }
        uint32_t mini = EncodeTwo(b, ce, b->charCes[i][1]);
        if (mini > 0xFFFF)
        {
            size_t index = b->resultCount - base;
            if (index > fastIndexMask)
            {
                mini = fastBailOut;
            }
            else
            {
                Append(b, mini >> 16);
                Append(b, mini & 0xFFFF);
                mini = fastExpansion | (uint32_t)index;
            }
        }
        b->result[start + (size_t)i] = (uint16_t)mini;
    }
}

static void EncodeContractions(Builder* b)
{
    size_t base = fastLatinHeader + fastLatinChars;
    size_t first = b->resultCount;
    for (int32_t i = 0; i < fastLatinChars; i++)
    {
        uint64_t ce = b->charCes[i][0];
        if (!IsContractionCharCe(ce))
        {
            continue;
        }
        size_t index = b->resultCount - base;
        if (index > fastIndexMask)
        {
            b->result[fastLatinHeader + (size_t)i] = fastBailOut;
            continue;
        }
        bool firstTriple = true;
        for (size_t k = (size_t)(ce & 0x7FFFFFFF);; k += 3)
        {
            uint32_t x = (uint32_t)b->contractions[k];
            if (x == fastContractionCharMask && !firstTriple)
            {
                break;
            }
            uint32_t mini = EncodeTwo(b, b->contractions[k + 1], b->contractions[k + 2]);
            if (mini == fastBailOut)
            {
                Append(b, x | 1u << fastContractionLengthShift);
            }
            else if (mini <= 0xFFFF)
            {
                Append(b, x | 2u << fastContractionLengthShift);
                Append(b, mini);
            }
            else
            {
                Append(b, x | 3u << fastContractionLengthShift);
                Append(b, mini >> 16);
                Append(b, mini & 0xFFFF);
            }
            firstTriple = false;
        }
        b->result[fastLatinHeader + (size_t)i] = (uint16_t)(fastContraction | index);
    }
    if (b->resultCount > first)
    {
        Append(b, fastContractionCharMask);
    }
}

static void Reset(Builder* b)
{
    b->contractionCount = 0;
    b->uniqueCount = 0;
    b->shortPrimaryOverflow = false;
    b->resultCount = fastLatinHeader;
}

static uint32_t FirstPrimary(const CollationRoot* root, uint32_t code)
{
    uint32_t index = mlocCollationScriptIndex(root, code);
    return index == 0 ? 0 : mlocCollationStart(root, index) << 16;
}

uint16_t* CollationFastLatinTable(CollationMappings* mappings, const CollationRoot* root,
                                  size_t* countOut)
{
    Builder* b = GenAllocate(sizeof(Builder));
    b->m = mappings;
    Append(b, fastLatinVersion << 8 | fastLatinHeader);
    bool ok = true;
    for (int i = 0; i < specialGroups; i++)
    {
        b->lastSpecialPrimaries[i] =
            mlocCollationLastPrimary(root, collationGroupSpace + (uint32_t)i);
        ok = ok && b->lastSpecialPrimaries[i] != 0;
        Append(b, 0);
    }
    b->firstDigitPrimary = FirstPrimary(root, collationGroupDigit);
    b->firstLatinPrimary = FirstPrimary(root, MUNI_SCRIPT('L', 'a', 't', 'n'));
    b->lastLatinPrimary = mlocCollationLastPrimary(root, MUNI_SCRIPT('L', 'a', 't', 'n'));
    ok = ok && b->firstDigitPrimary != 0 && b->firstLatinPrimary != 0;
    for (int32_t i = 0; ok && i < fastLatinChars; i++)
    {
        b->nfdLength[i] = Nfd(CharAt(i), b->nfd[i]);
        ok = b->nfdLength[i] > 0;
        Contexts(b, i);
    }
    if (ok)
    {
        b->firstShortPrimary = b->firstDigitPrimary;
        GetCes(b);
        EncodeUnique(b);
        if (b->shortPrimaryOverflow)
        {
            // Digits take long mini primaries, leaving short ones to letters.
            b->firstShortPrimary = b->firstLatinPrimary;
            Reset(b);
            GetCes(b);
            EncodeUnique(b);
        }
        ok = !b->shortPrimaryOverflow;
    }
    if (ok)
    {
        EncodeChars(b);
        EncodeContractions(b);
    }
    uint16_t* out = ok ? b->result : nullptr;
    *countOut = ok ? b->resultCount : 0;
    if (!ok)
    {
        free(b->result);
    }
    free(b->contractions);
    free(b->unique);
    free(b->miniCes);
    free(b);
    return out;
}

uint8_t* CollationFastLatinAppend(uint8_t* unit, size_t* size, const uint16_t* words, size_t count)
{
    size_t wordCount = words == nullptr ? 0 : count;
    uint8_t* grown = realloc(unit, *size + 4 + wordCount * 2);
    if (grown == nullptr)
    {
        GenDie("fast Latin", "out of memory");
    }
    GenPut(grown + *size, count, 4);
    for (size_t i = 0; i < wordCount; i++)
    {
        GenPut(grown + *size + 4 + i * 2, words[i], 2);
    }
    *size += 4 + wordCount * 2;
    return grown;
}

uint8_t* CollationFastLatinSection(uint8_t* unit, size_t* size, CollationMappings* mappings,
                                   const CollationRoot* root, const uint16_t* rootWords,
                                   size_t rootCount, const char* type)
{
    if (strncmp(type, "search", 6) == 0)
    {
        return CollationFastLatinAppend(unit, size, nullptr, 0);
    }
    size_t count = 0;
    uint16_t* words = CollationFastLatinTable(mappings, root, &count);
    bool same = words != nullptr && rootWords != nullptr && count == rootCount &&
                memcmp(words, rootWords, count * sizeof *words) == 0;
    uint8_t* out = same ? CollationFastLatinAppend(unit, size, nullptr, fastLatinRootTable)
                        : CollationFastLatinAppend(unit, size, words, count);
    free(words);
    return out;
}
