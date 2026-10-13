// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit table's validator and reads (unit_data.h). The validator
// bounds every offset, length and number, so the reads trust a table
// that passed it.

#include "unit_data.h"

#include "bytes.h"
#include "locale_tables.h"
#include "unit_tables.h"

#include <string.h>

UnitTable mlocReadUnitTable(DataSpan unit)
{
    const uint8_t* b = unit.bytes;
    UnitTable t = {
        .simpleCount = mlocLoad16(b),
        .prefixCount = mlocLoad16(b + 2),
        .aliasCount = mlocLoad16(b + 4),
        .baseCount = mlocLoad16(b + 6),
        .beaufortCount = mlocLoad16(b + 8),
        .dimensionCount = mlocLoad16(b + 10),
        .quantityCount = mlocLoad16(b + 12),
    };
    t.simples = b + unitsHeaderSize;
    t.prefixes = t.simples + (size_t)t.simpleCount * unitSimpleSize;
    t.aliases = t.prefixes + (size_t)t.prefixCount * unitPrefixSize;
    t.bases = t.aliases + (size_t)t.aliasCount * unitAliasSize;
    t.dimensions = t.bases + (size_t)t.baseCount * unitBaseSize;
    t.quantities = t.dimensions + (size_t)t.dimensionCount * unitDimensionSize;
    t.beaufort = t.quantities + (size_t)t.quantityCount * unitQuantitySize;
    t.text = t.beaufort + (size_t)t.beaufortCount * 2;
    t.numbers = t.text + mlocLoad32(b + 16);
    return t;
}

// An identifier's text: an offset and a length within the text, of the
// characters identifiers use.
static bool ValidText(const UnitTable* t, uint32_t textSize, const uint8_t* at, uint32_t length)
{
    uint32_t offset = mlocLoad32(at);
    if (length == 0 || offset > textSize || length > textSize - offset)
    {
        return false;
    }
    for (uint32_t i = 0; i < length; i++)
    {
        uint8_t c = t->text[offset + i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
        {
            return false;
        }
    }
    return true;
}

// Compares an identifier with a record's text.
static int CompareText(const UnitTable* t, const uint8_t* at, uint32_t length, const char* id,
                       size_t idLength)
{
    size_t n = length < idLength ? length : idLength;
    int c = memcmp(t->text + mlocLoad32(at), id, n);
    return c != 0 ? c : length < idLength ? -1 : length > idLength ? 1 : 0;
}

// Whether records' texts strictly ascend: each against the one before.
static bool Ascending(const UnitTable* t, const uint8_t* record, const uint8_t* previous,
                      uint32_t lengthAt)
{
    return previous == nullptr ||
           CompareText(t, previous, previous[lengthAt], (const char*)t->text + mlocLoad32(record),
                       record[lengthAt]) < 0;
}

// Reads a number at *at, advancing past it; false when it does not lie
// within the numbers or is not in its shortest form.
static bool ReadNumber(const uint8_t* numbers, uint32_t size, uint32_t* at, Wide* out)
{
    if (*at >= size)
    {
        return false;
    }
    uint32_t length = numbers[*at];
    if (length > unitMaxNumberBytes || length > size - *at - 1 ||
        (length > 0 && numbers[*at + length] == 0))
    {
        return false;
    }
    Wide w = {0};
    for (uint32_t i = 0; i < length; i++)
    {
        w.limb[i / 4] |= (uint32_t)numbers[*at + 1 + i] << (8 * (i % 4));
    }
    w.count = (length + 3) / 4;
    *at += 1 + length;
    if (out != nullptr)
    {
        *out = w;
    }
    return true;
}

static bool ValidFactor(const UnitTable* t, uint32_t numbersSize, const uint8_t* s)
{
    uint32_t at = mlocLoad32(s + simpleFactor);
    Wide n;
    Wide d;
    if (s[simpleBase] == unitNoBase)
    {
        return s[simpleFlags] == 0 && at == 0;
    }
    if (!ReadNumber(t->numbers, numbersSize, &at, &n) ||
        !ReadNumber(t->numbers, numbersSize, &at, &d) || mlocWideIsZero(&n) || mlocWideIsZero(&d))
    {
        return false;
    }
    if ((s[simpleFlags] & unitOffset) != 0 &&
        (!ReadNumber(t->numbers, numbersSize, &at, &n) ||
         !ReadNumber(t->numbers, numbersSize, &at, &d) || mlocWideIsZero(&d)))
    {
        return false;
    }
    return true;
}

static bool ValidSimples(const UnitTable* t, uint32_t textSize, uint32_t numbersSize)
{
    const uint8_t* previous = nullptr;
    for (uint32_t i = 0; i < t->simpleCount; i++)
    {
        const uint8_t* s = t->simples + (size_t)i * unitSimpleSize;
        if (!ValidText(t, textSize, s + simpleText, s[simpleLength]) ||
            !Ascending(t, s, previous, simpleLength) ||
            (s[simpleBase] != unitNoBase && s[simpleBase] >= t->baseCount) ||
            (s[simpleFlags] != 0 && s[simpleFlags] != unitOffset &&
             s[simpleFlags] != unitBeaufort) ||
            mlocLoad16(s + simpleSystems) > unitSystemAll || mlocLoad16(s + 14) != 0 ||
            !ValidFactor(t, numbersSize, s))
        {
            return false;
        }
        previous = s;
    }
    return true;
}

static bool ValidPrefixes(const UnitTable* t, uint32_t textSize)
{
    const uint8_t* previous = nullptr;
    for (uint32_t i = 0; i < t->prefixCount; i++)
    {
        const uint8_t* p = t->prefixes + (size_t)i * unitPrefixSize;
        int8_t power10 = (int8_t)p[5];
        if (!ValidText(t, textSize, p, p[4]) || !Ascending(t, p, previous, 4) ||
            (power10 == 0) == (p[6] == 0) || power10 < -30 || power10 > 30 || p[6] > 10 ||
            p[7] != 0)
        {
            return false;
        }
        previous = p;
    }
    return true;
}

static bool ValidAliases(const UnitTable* t, uint32_t textSize)
{
    const uint8_t* previous = nullptr;
    for (uint32_t i = 0; i < t->aliasCount; i++)
    {
        const uint8_t* a = t->aliases + (size_t)i * unitAliasSize;
        if (!ValidText(t, textSize, a, a[8]) || !ValidText(t, textSize, a + 4, a[9]) ||
            !Ascending(t, a, previous, 8) || a[10] != 0 || a[11] != 0)
        {
            return false;
        }
        previous = a;
    }
    return true;
}

// Whether exponents past the dimension count are 0.
static bool ValidExponents(const UnitTable* t, const uint8_t* exponents)
{
    for (uint32_t d = t->dimensionCount; d < unitMaxDimensions; d++)
    {
        if (exponents[d] != 0)
        {
            return false;
        }
    }
    return true;
}

static bool ValidBases(const UnitTable* t, uint32_t textSize)
{
    for (uint32_t i = 0; i < t->baseCount; i++)
    {
        const uint8_t* b = t->bases + (size_t)i * unitBaseSize;
        if (!ValidText(t, textSize, b, b[4]) || b[5] != 0 || b[6] != 0 || b[7] != 0 ||
            !ValidExponents(t, b + baseNumerator) || !ValidExponents(t, b + baseDenominator))
        {
            return false;
        }
    }
    return true;
}

static bool ValidDimensions(const UnitTable* t, uint32_t textSize)
{
    for (uint32_t i = 0; i < t->dimensionCount; i++)
    {
        const uint8_t* d = t->dimensions + (size_t)i * unitDimensionSize;
        if (!ValidText(t, textSize, d, d[4]) || d[6] != 0 || d[7] != 0)
        {
            return false;
        }
    }
    return true;
}

static bool ValidQuantities(const UnitTable* t, uint32_t textSize)
{
    for (uint32_t i = 0; i < t->quantityCount; i++)
    {
        const uint8_t* q = t->quantities + (size_t)i * unitQuantitySize;
        if (!ValidText(t, textSize, q, q[8]) || !ValidText(t, textSize, q + 4, q[9]) ||
            q[10] != 0 || q[11] != 0 || !ValidExponents(t, q + quantityNumerator) ||
            !ValidExponents(t, q + quantityDenominator))
        {
            return false;
        }
    }
    return true;
}

static bool ValidBeaufort(const UnitTable* t)
{
    for (uint32_t i = 1; i < t->beaufortCount; i++)
    {
        if (mlocLoad16(t->beaufort + 2 * i) <= mlocLoad16(t->beaufort + 2 * (i - 1)))
        {
            return false;
        }
    }
    return t->beaufortCount >= 2;
}

bool mlocValidUnits(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < unitsHeaderSize)
    {
        return false;
    }
    // The sizes in 64 bits, so that no count or size wraps on a 32-bit
    // target, and checked before any part's address is taken.
    const uint8_t* h = payload.bytes;
    uint32_t textSize = mlocLoad32(h + 16);
    uint32_t numbersSize = mlocLoad32(h + 20);
    uint64_t size = unitsHeaderSize + (uint64_t)mlocLoad16(h) * unitSimpleSize +
                    (uint64_t)mlocLoad16(h + 2) * unitPrefixSize +
                    (uint64_t)mlocLoad16(h + 4) * unitAliasSize +
                    (uint64_t)mlocLoad16(h + 6) * unitBaseSize + (uint64_t)mlocLoad16(h + 8) * 2 +
                    (uint64_t)mlocLoad16(h + 10) * unitDimensionSize +
                    (uint64_t)mlocLoad16(h + 12) * unitQuantitySize + textSize + numbersSize;
    if (size != payload.size || mlocLoad16(h + 14) != 0)
    {
        return false;
    }
    UnitTable t = mlocReadUnitTable(payload);
    return t.dimensionCount <= unitMaxDimensions && ValidSimples(&t, textSize, numbersSize) &&
           ValidPrefixes(&t, textSize) && ValidAliases(&t, textSize) && ValidBases(&t, textSize) &&
           ValidDimensions(&t, textSize) && ValidQuantities(&t, textSize) && ValidBeaufort(&t);
}

// The index of the record whose text equals an identifier, or -1.
static int32_t FindText(const UnitTable* t, const uint8_t* records, uint32_t count,
                        uint32_t recordSize, uint32_t lengthAt, const char* id, size_t length)
{
    uint32_t low = 0;
    uint32_t high = count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* r = records + (size_t)middle * recordSize;
        int c = CompareText(t, r, r[lengthAt], id, length);
        if (c == 0)
        {
            return (int32_t)middle;
        }
        if (c < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return -1;
}

int32_t mlocFindSimpleUnit(const UnitTable* t, const char* id, size_t length)
{
    return FindText(t, t->simples, t->simpleCount, unitSimpleSize, simpleLength, id, length);
}

int32_t mlocFindUnitPrefix(const UnitTable* t, const char* token, size_t length)
{
    int32_t found = -1;
    uint32_t best = 0;
    for (uint32_t i = 0; i < t->prefixCount; i++)
    {
        const uint8_t* p = t->prefixes + (size_t)i * unitPrefixSize;
        if (p[4] < length && p[4] > best && memcmp(t->text + mlocLoad32(p), token, p[4]) == 0)
        {
            found = (int32_t)i;
            best = p[4];
        }
    }
    return found;
}

size_t mlocFindUnitAlias(const UnitTable* t, const char* id, size_t length,
                         const char** replacementOut)
{
    int32_t i = FindText(t, t->aliases, t->aliasCount, unitAliasSize, 8, id, length);
    if (i < 0)
    {
        return 0;
    }
    const uint8_t* a = t->aliases + (size_t)i * unitAliasSize;
    *replacementOut = (const char*)t->text + mlocLoad32(a + 4);
    return a[9];
}

const uint8_t* mlocSimpleUnit(const UnitTable* t, uint32_t index)
{
    return t->simples + (size_t)index * unitSimpleSize;
}

const uint8_t* mlocUnitPrefix(const UnitTable* t, uint32_t index)
{
    return t->prefixes + (size_t)index * unitPrefixSize;
}

const uint8_t* mlocBaseUnit(const UnitTable* t, uint32_t base)
{
    return t->bases + (size_t)base * unitBaseSize;
}

void mlocReadUnitFactor(const UnitTable* t, uint32_t simple, UnitFactor* out)
{
    const uint8_t* s = mlocSimpleUnit(t, simple);
    uint32_t at = mlocLoad32(s + simpleFactor);
    // The validator bounded the numbers; the size only has to pass them.
    (void)ReadNumber(t->numbers, UINT32_MAX, &at, &out->numerator);
    (void)ReadNumber(t->numbers, UINT32_MAX, &at, &out->denominator);
    out->offsetNumerator = mlocWideOf(0);
    out->offsetDenominator = mlocWideOf(1);
    if ((s[simpleFlags] & unitOffset) != 0)
    {
        (void)ReadNumber(t->numbers, UINT32_MAX, &at, &out->offsetNumerator);
        (void)ReadNumber(t->numbers, UINT32_MAX, &at, &out->offsetDenominator);
    }
}

UnitPrefsTable mlocReadUnitPrefs(DataSpan unit)
{
    const uint8_t* b = unit.bytes;
    UnitPrefsTable t = {
        .setCount = mlocLoad16(b),
        .runCount = mlocLoad16(b + 2),
        .entryCount = mlocLoad16(b + 4),
    };
    t.sets = b + unitPrefsHeaderSize;
    t.runs = t.sets + (size_t)t.setCount * unitSetSize;
    t.entries = t.runs + (size_t)t.runCount * unitRunSize;
    t.text = t.entries + (size_t)t.entryCount * unitEntrySize;
    return t;
}

// A preference text: within the text, of identifier characters.
static bool ValidPrefText(const UnitPrefsTable* t, uint32_t textSize, const uint8_t* at,
                          uint32_t length)
{
    UnitTable view = {.text = t->text};
    return ValidText(&view, textSize, at, length);
}

static int CompareSets(const UnitPrefsTable* t, const uint8_t* set, const char* category,
                       size_t categoryLength, const char* usage, size_t usageLength)
{
    UnitTable view = {.text = t->text};
    int c = CompareText(&view, set, set[8], category, categoryLength);
    return c != 0 || usage == nullptr ? c : CompareText(&view, set + 4, set[9], usage, usageLength);
}

static bool ValidSets(const UnitPrefsTable* t, uint32_t textSize)
{
    uint32_t nextRun = 0;
    for (uint32_t i = 0; i < t->setCount; i++)
    {
        const uint8_t* s = t->sets + (size_t)i * unitSetSize;
        const uint8_t* p = s - unitSetSize;
        uint32_t first = mlocLoad16(s + 10);
        uint32_t count = mlocLoad16(s + 12);
        if (!ValidPrefText(t, textSize, s, s[8]) || !ValidPrefText(t, textSize, s + 4, s[9]) ||
            (i > 0 && CompareSets(t, p, (const char*)t->text + mlocLoad32(s), s[8],
                                  (const char*)t->text + mlocLoad32(s + 4), s[9]) >= 0) ||
            first != nextRun || count == 0 || mlocLoad16(s + 14) != 0)
        {
            return false;
        }
        nextRun = first + count;
        bool world = false;
        for (uint32_t r = first; r < nextRun && r < t->runCount; r++)
        {
            uint32_t region = mlocLoad16(t->runs + (size_t)r * unitRunSize);
            world |= region == mlocPackRegion("001", 3);
            if (!mlocValidPackedRegion(region) ||
                (r > first && region <= mlocLoad16(t->runs + (size_t)(r - 1) * unitRunSize)))
            {
                return false;
            }
        }
        if (!world)
        {
            return false;
        }
    }
    return nextRun == t->runCount;
}

static bool ValidIncrement(uint32_t n)
{
    return n == 0 || n == 1 || n == 2 || n == 5 || n == 10 || n == 20 || n == 25 || n == 50 ||
           n == 100;
}

static bool ValidRuns(const UnitPrefsTable* t, uint32_t textSize)
{
    for (uint32_t r = 0; r < t->runCount; r++)
    {
        uint32_t first = mlocLoad16(t->runs + (size_t)r * unitRunSize + 2);
        uint32_t end = r + 1 < t->runCount ? mlocLoad16(t->runs + (size_t)(r + 1) * unitRunSize + 2)
                                           : t->entryCount;
        if ((r == 0 && first != 0) || first >= end || end > t->entryCount)
        {
            return false;
        }
        for (uint32_t k = first; k < end; k++)
        {
            const uint8_t* e = t->entries + (size_t)k * unitEntrySize;
            if (!ValidPrefText(t, textSize, e, e[4]) || e[5] > unitMaxPlaces ||
                !ValidIncrement(mlocLoad16(e + 6)) || (k + 1 == end && mlocLoad32(e + 8) != 0))
            {
                return false;
            }
        }
    }
    return true;
}

bool mlocValidUnitPreferences(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < unitPrefsHeaderSize)
    {
        return false;
    }
    const uint8_t* h = payload.bytes;
    uint32_t textSize = mlocLoad32(h + 8);
    uint64_t size = unitPrefsHeaderSize + (uint64_t)mlocLoad16(h) * unitSetSize +
                    (uint64_t)mlocLoad16(h + 2) * unitRunSize +
                    (uint64_t)mlocLoad16(h + 4) * unitEntrySize + textSize;
    if (size != payload.size || mlocLoad16(h + 6) != 0)
    {
        return false;
    }
    UnitPrefsTable t = mlocReadUnitPrefs(payload);
    return ValidSets(&t, textSize) && ValidRuns(&t, textSize);
}

int32_t mlocFindUnitPrefSet(const UnitPrefsTable* t, const char* category, size_t categoryLength,
                            const char* usage, size_t usageLength)
{
    uint32_t low = 0;
    uint32_t high = t->setCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        int c = CompareSets(t, t->sets + (size_t)middle * unitSetSize, category, categoryLength,
                            usage, usageLength);
        if (c < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    bool found = low < t->setCount && CompareSets(t, t->sets + (size_t)low * unitSetSize, category,
                                                  categoryLength, usage, usageLength) == 0;
    return found ? (int32_t)low : -1;
}

uint32_t mlocFindUnitPrefRun(const UnitPrefsTable* t, uint32_t set, uint32_t region)
{
    const uint8_t* s = t->sets + (size_t)set * unitSetSize;
    uint32_t first = mlocLoad16(s + 10);
    uint32_t end = first + mlocLoad16(s + 12);
    uint32_t world = first;
    for (uint32_t r = first; r < end; r++)
    {
        uint32_t at = mlocLoad16(t->runs + (size_t)r * unitRunSize);
        if (at == region)
        {
            return r;
        }
        world = at == mlocPackRegion("001", 3) ? r : world;
    }
    return world;
}

uint32_t mlocUnitPrefEntries(const UnitPrefsTable* t, uint32_t run, const uint8_t** firstOut)
{
    uint32_t first = mlocLoad16(t->runs + (size_t)run * unitRunSize + 2);
    uint32_t end = run + 1 < t->runCount ? mlocLoad16(t->runs + (size_t)(run + 1) * unitRunSize + 2)
                                         : t->entryCount;
    *firstOut = t->entries + (size_t)first * unitEntrySize;
    return end - first;
}
