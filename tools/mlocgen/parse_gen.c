// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The parse sets (parse_gen.h). Root's parseLenients are read from its
// chains, as ICU's ParseDataSink reads its root bundle: the general and
// number scopes, each set filed by the symbol it stands for; the fixed
// sets are ICU's (static_unicode_sets.cpp), their properties Maul
// Unicode's.

#include "parse_gen.h"

#include "kinds.h"
#include "parse_tables.h"
#include "table_gen.h"

#include "maul-unicode/case.h"
#include "maul-unicode/properties.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxRanges = 512,
    maxStrings = 8,
    maxFolds = 512,
    maxCodePoint = 0x10FFFF,
};

typedef struct Set
{
    uint32_t first[maxRanges];
    uint32_t last[maxRanges];
    int count;
    char strings[maxStrings][16];
    int stringCount;
} Set;

static Set s_sets[parseClassCount];
// Root's apostrophes, which ICU adds to the other grouping separators.
static Set s_apostrophes;

static void AddRange(Set* set, uint32_t first, uint32_t last)
{
    if (set->count == maxRanges || first > last || last > maxCodePoint)
    {
        GenDie("a parse set's range out of bounds", "");
    }
    set->first[set->count] = first;
    set->last[set->count] = last;
    set->count++;
}

static void AddString(Set* set, const char* text, size_t length)
{
    if (set->stringCount == maxStrings || length >= sizeof(set->strings[0]))
    {
        GenDie("a parse set's string out of bounds", text);
    }
    memcpy(set->strings[set->stringCount], text, length);
    set->strings[set->stringCount][length] = '\0';
    set->stringCount++;
}

static void AddAll(Set* set, const Set* other)
{
    for (int i = 0; i < other->count; i++)
    {
        AddRange(set, other->first[i], other->last[i]);
    }
    for (int i = 0; i < other->stringCount; i++)
    {
        AddString(set, other->strings[i], strlen(other->strings[i]));
    }
}

static int CompareRanges(const void* a, const void* b)
{
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return x < y ? -1 : x > y;
}

// Sorts a set's ranges and merges those that touch.
static void Normalize(Set* set)
{
    uint64_t packed[maxRanges];
    for (int i = 0; i < set->count; i++)
    {
        packed[i] = (uint64_t)set->first[i] << 32 | set->last[i];
    }
    qsort(packed, (size_t)set->count, sizeof(packed[0]), CompareRanges);
    int out = 0;
    for (int i = 0; i < set->count; i++)
    {
        uint32_t first = (uint32_t)(packed[i] >> 32);
        uint32_t last = (uint32_t)packed[i];
        if (out > 0 && first <= set->last[out - 1] + 1)
        {
            set->last[out - 1] = last > set->last[out - 1] ? last : set->last[out - 1];
            continue;
        }
        set->first[out] = first;
        set->last[out] = last;
        out++;
    }
    set->count = out;
}

// The code point at text, its length into lengthOut; dies on bad UTF-8.
static uint32_t Decode(const char* text, int* lengthOut)
{
    const unsigned char* t = (const unsigned char*)text;
    int length = GenCharLength(t);
    if (length == 0)
    {
        GenDie("a parse set that is not UTF-8", text);
    }
    static const unsigned char s_masks[5] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t c = t[0] & s_masks[length];
    for (int i = 1; i < length; i++)
    {
        c = c << 6 | (t[i] & 0x3F);
    }
    *lengthOut = length;
    return c;
}

// Reads a UnicodeSet pattern of literal characters, escapes, ranges and
// strings ("[\-－﹣ ‐‑]", "[₹ {Rp}]"), spaces ignored.
static void ReadPattern(const char* pattern, Set* set)
{
    size_t length = strlen(pattern);
    if (length < 2 || pattern[0] != '[' || pattern[length - 1] != ']')
    {
        GenDie("a parse set that is not a bracketed set", pattern);
    }
    int64_t previous = -1;
    for (size_t at = 1; at < length - 1;)
    {
        char c = pattern[at];
        if (c == ' ')
        {
            at++;
            continue;
        }
        if (c == '{')
        {
            const char* end = strchr(pattern + at, '}');
            if (end == nullptr)
            {
                GenDie("a parse set's string without its end", pattern);
            }
            AddString(set, pattern + at + 1, (size_t)(end - pattern) - at - 1);
            at = (size_t)(end - pattern) + 1;
            previous = -1;
            continue;
        }
        if (c == '-' && previous >= 0 && at + 2 < length)
        {
            int n = 0;
            uint32_t last = Decode(pattern + at + 1, &n);
            set->last[set->count - 1] = last;
            if (last < (uint32_t)previous)
            {
                GenDie("a parse set's range backwards", pattern);
            }
            at += 1 + (size_t)n;
            previous = -1;
            continue;
        }
        at += c == '\\' ? 1 : 0;
        int n = 0;
        uint32_t cp = Decode(pattern + at, &n);
        AddRange(set, cp, cp);
        previous = cp;
        at += (size_t)n;
    }
}

typedef struct Lenient
{
    const char* sample;
    const char* level;
    Set* set;
} Lenient;

// Files one of root's chains when it is a parse set of the general or
// number scope.
static void TakeChain(const char* chain, void* value, void* context)
{
    (void)context;
    static const char s_prefix[] = "//ldml/characters/parseLenients[@level=\"";
    if (strncmp(chain, s_prefix, sizeof(s_prefix) - 1) != 0 || strstr(chain, "\"date\"") != nullptr)
    {
        return;
    }
    const Lenient s_lenients[] = {
        {".", "lenient", &s_sets[parsePeriod]},
        {".", "stricter", &s_sets[parseStrictPeriod]},
        {",", "lenient", &s_sets[parseComma]},
        {",", "stricter", &s_sets[parseStrictComma]},
        {"-", "lenient", &s_sets[parseMinus]},
        {"+", "lenient", &s_sets[parsePlus]},
        {"%", "lenient", &s_sets[parsePercent]},
        {"\xE2\x80\xB0", "lenient", &s_sets[parsePerMille]},
        {"$", "lenient", &s_sets[parseDollar]},
        {"\xC2\xA3", "lenient", &s_sets[parsePound]},
        {"\xC2\xA5", "lenient", &s_sets[parseYen]},
        {"\xE2\x82\xA9", "lenient", &s_sets[parseWon]},
        {"\xE2\x82\xB9", "lenient", &s_sets[parseRupee]},
        {"\xE2\x80\x99", "lenient", &s_apostrophes},
    };
    const char* level = chain + sizeof(s_prefix) - 1;
    const char* sample = strstr(chain, "[@sample=\"");
    if (sample == nullptr)
    {
        GenDie("a parse set without its sample", chain);
    }
    sample += 10;
    for (size_t i = 0; i < sizeof(s_lenients) / sizeof(s_lenients[0]); i++)
    {
        size_t n = strlen(s_lenients[i].sample);
        if (strncmp(level, s_lenients[i].level, strlen(s_lenients[i].level)) == 0 &&
            strncmp(sample, s_lenients[i].sample, n) == 0 && strcmp(sample + n, "\"]") == 0)
        {
            ReadPattern(((const Leaf*)value)->text, s_lenients[i].set);
            return;
        }
    }
    GenDie("a parse set ICU does not read", chain);
}

// ICU's fixed sets and the decimal digits.
static void AddFixedSets(void)
{
    static const uint32_t s_bidi[][2] = {
        {0x061C, 0x061C}, {0x200E, 0x200F}, {0x202A, 0x202E}, {0x2066, 0x2069}};
    static const uint32_t s_selectors[][2] = {
        {0x180B, 0x180D}, {0x180F, 0x180F}, {0xFE00, 0xFE0F}, {0xE0100, 0xE01EF}};
    static const uint32_t s_grouping[][2] = {{0x066C, 0x066C}, {0x2018, 0x2018}, {0x20, 0x20},
                                             {0xA0, 0xA0},     {0x2000, 0x200A}, {0x202F, 0x202F},
                                             {0x205F, 0x205F}, {0x3000, 0x3000}};
    static const uint32_t s_approximately[] = {0x223C, 0x7E, 0x2248, 0x2243, 0x7D04};
    for (size_t i = 0; i < 4; i++)
    {
        AddRange(&s_sets[parseStrictIgnorables], s_bidi[i][0], s_bidi[i][1]);
        AddRange(&s_sets[parseIgnorables], s_bidi[i][0], s_bidi[i][1]);
        AddRange(&s_sets[parseIgnorables], s_selectors[i][0], s_selectors[i][1]);
    }
    AddRange(&s_sets[parseIgnorables], 0x09, 0x09);
    for (size_t i = 0; i < sizeof(s_grouping) / sizeof(s_grouping[0]); i++)
    {
        AddRange(&s_sets[parseOtherGrouping], s_grouping[i][0], s_grouping[i][1]);
    }
    AddAll(&s_sets[parseOtherGrouping], &s_apostrophes);
    AddRange(&s_sets[parseInfinity], 0x221E, 0x221E);
    for (size_t i = 0; i < sizeof(s_approximately) / sizeof(s_approximately[0]); i++)
    {
        AddRange(&s_sets[parseApproximately], s_approximately[i], s_approximately[i]);
    }
    int64_t run = -1;
    for (uint32_t cp = 0; cp <= maxCodePoint + 1; cp++)
    {
        muniGeneralCategory gc = cp <= maxCodePoint ? muniGetGeneralCategory(cp) : 0;
        if (cp <= maxCodePoint && gc == muni_gcZs)
        {
            AddRange(&s_sets[parseIgnorables], cp, cp);
        }
        bool digit = cp <= maxCodePoint && gc == muni_gcNd;
        if (digit && run < 0)
        {
            run = cp;
        }
        if (digit && muniGetDecimalDigitValue(cp) != (int32_t)((cp - (uint32_t)run) % 10))
        {
            GenDie("a run of decimal digits not from 0 to 9", "");
        }
        if (!digit && run >= 0)
        {
            AddRange(&s_sets[parseDigits], (uint32_t)run, cp - 1);
            run = -1;
        }
    }
}

typedef struct Fold
{
    uint32_t first;
    uint32_t count;
    uint32_t stride;
    int32_t delta;
} Fold;

static Fold s_folds[maxFolds];
static int s_foldCount;

// What ICU's parser folds a code point to: u_foldCase with the Turkic
// mappings, which its StringSegment asks for (I to dotless i, dotted I to
// i).
static uint32_t FoldOf(uint32_t cp)
{
    return cp == 0x49 ? 0x131 : cp == 0x130 ? 0x69 : muniFoldCase(cp);
}

// The folds as runs: code points 1 or 2 apart that fold by the same
// difference.
static void BuildFolds(void)
{
    s_foldCount = 0;
    Fold* run = nullptr;
    for (uint32_t cp = 0; cp <= maxCodePoint; cp++)
    {
        uint32_t folded = FoldOf(cp);
        if (folded == cp)
        {
            continue;
        }
        int32_t delta = (int32_t)folded - (int32_t)cp;
        uint32_t last = run == nullptr ? 0 : run->first + (run->count - 1) * run->stride;
        uint32_t gap = cp - last;
        bool extends = run != nullptr && run->delta == delta && run->count < 0xFFFF &&
                       (run->count == 1 ? gap <= 2 : gap == run->stride);
        if (extends)
        {
            run->stride = run->count == 1 ? gap : run->stride;
            run->count++;
            continue;
        }
        if (s_foldCount == maxFolds)
        {
            GenDie("too many case folding runs", "");
        }
        run = &s_folds[s_foldCount++];
        *run = (Fold){.first = cp, .count = 1, .stride = 1, .delta = delta};
    }
}

void GenerateParseSets(Cldr* cldr, DataFile* file)
{
    memset(s_sets, 0, sizeof(s_sets));
    memset(&s_apostrophes, 0, sizeof(s_apostrophes));
    const LdmlFile* root = CldrFile(cldr, "root", componentMain);
    if (root == nullptr)
    {
        GenDie("no root file for the parse sets", "");
    }
    MapEach((Map*)&root->leaves, TakeChain, nullptr);
    AddFixedSets();
    int ranges = 0;
    int strings = 0;
    size_t text = 0;
    for (int c = 0; c < parseClassCount; c++)
    {
        Normalize(&s_sets[c]);
        if (s_sets[c].count == 0 && c != parseInfinity)
        {
            GenDie("an empty parse set", "");
        }
        ranges += s_sets[c].count;
        strings += s_sets[c].stringCount;
        for (int i = 0; i < s_sets[c].stringCount; i++)
        {
            text += strlen(s_sets[c].strings[i]);
        }
    }
    BuildFolds();
    size_t size = parseHeaderSize + parseClassCount * parseClassSize +
                  (size_t)ranges * parseRangeSize + (size_t)s_foldCount * parseFoldSize +
                  2 * ((size_t)strings + 1) + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, parseClassCount, 2);
    GenPut(unit + 2, (uint64_t)ranges, 2);
    GenPut(unit + 4, (uint64_t)strings, 2);
    GenPut(unit + 6, text, 2);
    GenPut(unit + 8, (uint64_t)s_foldCount, 2);
    uint8_t* classes = unit + parseHeaderSize;
    uint8_t* rangeAt = classes + parseClassCount * parseClassSize;
    uint8_t* foldAt = rangeAt + (size_t)ranges * parseRangeSize;
    for (int i = 0; i < s_foldCount; i++)
    {
        uint8_t* f = foldAt + (size_t)i * parseFoldSize;
        GenPut(f, s_folds[i].first, 4);
        GenPut(f + 4, s_folds[i].count, 2);
        GenPut(f + 6, s_folds[i].stride, 1);
        GenPut(f + 8, (uint32_t)s_folds[i].delta, 4);
    }
    uint8_t* offsets = foldAt + (size_t)s_foldCount * parseFoldSize;
    uint8_t* textAt = offsets + 2 * ((size_t)strings + 1);
    int range = 0;
    int string = 0;
    size_t at = 0;
    for (int c = 0; c < parseClassCount; c++)
    {
        const Set* set = &s_sets[c];
        GenPut(classes + c * parseClassSize, (uint64_t)range, 2);
        GenPut(classes + c * parseClassSize + 2, (uint64_t)set->count, 2);
        GenPut(classes + c * parseClassSize + 4, (uint64_t)string, 2);
        GenPut(classes + c * parseClassSize + 6, (uint64_t)set->stringCount, 2);
        for (int i = 0; i < set->count; i++, range++)
        {
            GenPut(rangeAt + (size_t)range * parseRangeSize, set->first[i], 4);
            GenPut(rangeAt + (size_t)range * parseRangeSize + 4, set->last[i], 4);
        }
        for (int i = 0; i < set->stringCount; i++, string++)
        {
            size_t n = strlen(set->strings[i]);
            GenPut(offsets + 2 * (size_t)string, at, 2);
            memcpy(textAt + at, set->strings[i], n);
            at += n;
        }
    }
    GenPut(offsets + 2 * (size_t)strings, at, 2);
    DataFileAdd(file, kindParseSets, nullptr, unit, size);
    free(unit);
}
