// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit table's validator and reads (unit_data.h). The validator
// bounds every offset, length and number, so the reads trust a table
// that passed it.

#include "unit_data.h"

#include "bytes.h"
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
    };
    t.simples = b + unitsHeaderSize;
    t.prefixes = t.simples + (size_t)t.simpleCount * unitSimpleSize;
    t.aliases = t.prefixes + (size_t)t.prefixCount * unitPrefixSize;
    t.bases = t.aliases + (size_t)t.aliasCount * unitAliasSize;
    t.beaufort = t.bases + (size_t)t.baseCount * unitBaseSize;
    t.text = t.beaufort + (size_t)t.beaufortCount * 2;
    t.numbers = t.text + mlocLoad32(b + 12);
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
            s[simpleFlags] > (unitOffset | unitBeaufort) || !ValidFactor(t, numbersSize, s))
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

static bool ValidBases(const UnitTable* t, uint32_t textSize)
{
    for (uint32_t i = 0; i < t->baseCount; i++)
    {
        const uint8_t* b = t->bases + (size_t)i * unitBaseSize;
        if (!ValidText(t, textSize, b, b[4]) || b[5] != 0 || b[6] != 0 || b[7] != 0)
        {
            return false;
        }
        for (uint32_t d = t->dimensionCount; d < unitMaxDimensions; d++)
        {
            if (b[8 + d] != 0)
            {
                return false;
            }
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
    UnitTable t = mlocReadUnitTable(payload);
    uint32_t textSize = mlocLoad32(payload.bytes + 12);
    uint32_t numbersSize = mlocLoad32(payload.bytes + 16);
    size_t size = unitsHeaderSize + (size_t)t.simpleCount * unitSimpleSize +
                  (size_t)t.prefixCount * unitPrefixSize + (size_t)t.aliasCount * unitAliasSize +
                  (size_t)t.baseCount * unitBaseSize + (size_t)t.beaufortCount * 2 + textSize +
                  numbersSize;
    return size == payload.size && t.dimensionCount <= unitMaxDimensions &&
           ValidSimples(&t, textSize, numbersSize) && ValidPrefixes(&t, textSize) &&
           ValidAliases(&t, textSize) && ValidBases(&t, textSize) && ValidBeaufort(&t);
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

const int8_t* mlocBaseDimensions(const UnitTable* t, uint32_t base)
{
    return (const int8_t*)(t->bases + (size_t)base * unitBaseSize + 8);
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
