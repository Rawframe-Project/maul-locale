// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root collation unit (collation_unit.h).

#include "collation_unit.h"

#include "collation_encode.h"
#include "collation_tables.h"
#include "table_gen.h"
#include "trie_gen.h"

#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Builder
{
    CollationWords ces;
    CollationWords ce32s;
    CollationWords contexts;
    CollationWords listStarts;
    CollationWords scripts;
    CollationWords starts;
    uint32_t numericPrimary;
    // A bit a lead byte: those sort keys compress.
    uint8_t compressible[32];
    uint32_t* values;
    char* error;
    size_t errorSize;
} Builder;

bool CollationLeftOut(uint32_t codePoint)
{
    if ((codePoint >= 0xAC00 && codePoint <= 0xD7A3) ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF))
    {
        return true;
    }
    char text[4];
    size_t n = codePoint < 0x80 ? 1 : codePoint < 0x800 ? 2 : codePoint < 0x10000 ? 3 : 4;
    static const uint8_t lead[] = {0, 0, 0xC0, 0xE0, 0xF0};
    for (size_t i = n; i-- > 1;)
    {
        text[i] = (char)(0x80 | ((codePoint >> (6 * (n - 1 - i))) & 0x3F));
    }
    text[0] = (char)(lead[n] | (codePoint >> (6 * (n - 1))));
    muniQuickCheck answer = muni_quickCheckYes;
    return muniCheckNormalization(text, n, muni_nfd, &answer).status == muni_success &&
           answer != muni_quickCheckYes;
}

uint32_t CollationSampleText(uint32_t sample, uint32_t* textOut)
{
    if (sample < 0xAC00 || sample > 0xD7A3)
    {
        textOut[0] = sample;
        return 1;
    }
    uint32_t s = sample - 0xAC00;
    textOut[0] = 0x1100 + s / 588;
    textOut[1] = 0x1161 + s % 588 / 28;
    textOut[2] = 0x11A7 + s % 28;
    return s % 28 == 0 ? 2 : 3;
}

static uint32_t Encode(Builder* b, const uint64_t* elements, uint32_t count)
{
    return CollationEncode(&b->ces, &b->ce32s, elements, count);
}

// ICU's diffThreeBytePrimaries under one lead byte: how many steps of
// AddToPrimary lead from one primary to another. A second byte step is
// 254 third byte steps whether the lead byte compresses or not, so only
// the lead byte's own count, never needed here, depends on it.
static int64_t Distance(uint32_t p1, uint32_t p2)
{
    int64_t a = (int64_t)((p1 >> 8) & 0xFF) + 254 * (int64_t)((p1 >> 16) & 0xFF);
    int64_t b = (int64_t)((p2 >> 8) & 0xFF) + 254 * (int64_t)((p2 >> 16) & 0xFF);
    return b - a;
}

static bool IsLongPrimary(uint32_t ce32)
{
    return mlocCe32IsSpecial(ce32) && ce32 != collationImplicitCe32 &&
           mlocCe32Tag(ce32) == collationTagLongPrimary;
}

// Sets an offset over a range where it saves trie blocks, as ICU's
// maybeSetPrimaryRange does for its blocks: a step from 2 to 127, and
// three block boundaries crossed, or one or two with four code points
// on either side.
static void MaybeRange(Builder* b, const RootTable* t, uint32_t start, uint32_t end, int64_t step)
{
    uint32_t primary = b->values[start] & 0xFFFFFF00;
    uint32_t delta = (end >> 6) - (start >> 6);
    if (step < 2 || step > 0x7F ||
        !(delta >= 3 || (delta > 0 && (start & 63) <= 60 && (end & 63) >= 3)))
    {
        return;
    }
    bool compressible = t->compressible[primary >> 24] != 0;
    size_t index = CollationPush(&b->ces, (uint64_t)primary << 32 | (uint64_t)start << 8 |
                                              (compressible ? 0x80 : 0) | (uint64_t)step);
    for (uint32_t c = start; c <= end; c++)
    {
        b->values[c] = CollationSpecial(collationTagOffset, index, 0);
    }
}

// Runs of code points from U+0180 whose long primaries rise by one step
// under one lead byte, as ICU's genuca finds them, become offsets.
static void OffsetRanges(Builder* b, const RootTable* t)
{
    uint32_t first = 0;
    uint32_t last = 0;
    int64_t step = -1;
    bool open = false;
    for (uint32_t c = 0x180; c <= trieMaxCodePoint + 1; c++)
    {
        uint32_t ce32 = c <= trieMaxCodePoint ? b->values[c] : collationImplicitCe32;
        bool longPrimary = IsLongPrimary(ce32);
        uint32_t p = ce32 & 0xFFFFFF00;
        uint32_t lastP = b->values[last] & 0xFFFFFF00;
        bool joins = false;
        if (open && longPrimary && c == last + 1 && p > lastP && (p >> 24) == (lastP >> 24))
        {
            int64_t d = Distance(lastP, p);
            joins = first == last ? d >= 2 : d == step;
            step = first == last && joins ? d : step;
        }
        if (joins)
        {
            last = c;
            continue;
        }
        if (open && first != last)
        {
            MaybeRange(b, t, first, last, step);
        }
        open = longPrimary;
        first = last = c;
        step = -1;
    }
}

static int Fail(Builder* b, const char* what, uint32_t codePoint)
{
    snprintf(b->error, b->errorSize, "U+%04X: %s", (unsigned)codePoint, what);
    return 0;
}

// Han: one offset element a range, its CE32 at every code point of it.
static void HanRanges(Builder* b, const RootTable* t)
{
    for (size_t i = 0; i < t->hanRangeCount; i++)
    {
        uint32_t start = t->hanRanges[2 * i];
        uint32_t end = t->hanRanges[2 * i + 1];
        uint64_t first = RootHanElement(t, start);
        size_t index =
            CollationPush(&b->ces, (first & 0xFFFFFFFF00000000ULL) | (uint64_t)start << 8 | 2);
        for (uint32_t c = start; c <= end; c++)
        {
            b->values[c] = CollationSpecial(collationTagOffset, index, 0);
        }
    }
}

typedef struct Entry
{
    uint32_t text[collationMaxContext];
    uint32_t length;
    uint32_t ce32;
} Entry;

static int ByText(const void* x, const void* y)
{
    const Entry* a = x;
    const Entry* b = y;
    for (uint32_t i = 0; i < a->length && i < b->length; i++)
    {
        if (a->text[i] != b->text[i])
        {
            return a->text[i] < b->text[i] ? -1 : 1;
        }
    }
    return a->length == b->length ? 0 : a->length < b->length ? -1 : 1;
}

// A context list for the code point: its default, then its entries in
// order; the trie value becomes its tag with the list's number.
static void Context(Builder* b, uint32_t codePoint, uint32_t tag, Entry* entries, size_t count)
{
    GenSort(entries, count, sizeof(Entry), ByText);
    size_t number = CollationPush(&b->listStarts, b->contexts.count);
    CollationPush(&b->contexts, b->values[codePoint]);
    CollationPush(&b->contexts, count);
    for (size_t i = 0; i < count; i++)
    {
        CollationPush(&b->contexts, entries[i].length);
        for (uint32_t k = 0; k < entries[i].length; k++)
        {
            CollationPush(&b->contexts, entries[i].text[k]);
        }
        CollationPush(&b->contexts, entries[i].ce32);
    }
    b->values[codePoint] = CollationSpecial(tag, number, 0);
}

// A mapping's place among those of one code point's contexts.
typedef struct Member
{
    uint32_t codePoint;
    uint32_t mapping;
} Member;

static int ByCodePoint(const void* x, const void* y)
{
    const Member* a = x;
    const Member* b = y;
    return a->codePoint != b->codePoint ? (a->codePoint < b->codePoint ? -1 : 1)
           : a->mapping != b->mapping   ? (a->mapping < b->mapping ? -1 : 1)
                                        : 0;
}

// The entry of one mapping in its code point's context list.
static bool MakeEntry(Builder* b, const RootMapping* m, bool prefixes, Entry* e)
{
    uint32_t length = prefixes ? m->prefixLength : m->textLength - 1;
    if (length > collationMaxContext || (prefixes && m->textLength != 1))
    {
        return false;
    }
    *e = (Entry){.length = length, .ce32 = Encode(b, m->elements, m->elementCount)};
    for (uint32_t k = 0; k < length; k++)
    {
        // A prefix is matched backwards from the code point.
        e->text[k] = prefixes ? m->prefix[length - 1 - k] : m->text[1 + k];
    }
    return true;
}

// The contexts starting with, or ending on, each code point, gathered
// from the mappings; 0 with a message for one the unit cannot hold.
static int Contexts(Builder* b, const RootTable* t, bool prefixes)
{
    Member* members = GenAllocate(t->count * sizeof(Member) + sizeof(Member));
    size_t count = 0;
    for (size_t i = 0; i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        bool mine = prefixes ? m->prefixLength > 0 : m->prefixLength == 0 && m->textLength > 1;
        if (mine && !CollationLeftOut(m->text[0]))
        {
            members[count++] = (Member){m->text[0], (uint32_t)i};
        }
    }
    GenSort(members, count, sizeof(Member), ByCodePoint);
    Entry* entries = GenAllocate(count * sizeof(Entry) + sizeof(Entry));
    int ok = 1;
    for (size_t start = 0; ok && start < count;)
    {
        uint32_t c = members[start].codePoint;
        size_t n = 0;
        for (; start + n < count && members[start + n].codePoint == c; n++)
        {
            if (!MakeEntry(b, &t->mappings[members[start + n].mapping], prefixes, &entries[n]))
            {
                ok = Fail(b, "a context the unit cannot hold", c);
                break;
            }
        }
        uint32_t held = b->values[c];
        if (ok && mlocCe32IsSpecial(held) &&
            (mlocCe32Tag(held) == collationTagPrefix ||
             mlocCe32Tag(held) == collationTagContraction) &&
            held != collationImplicitCe32)
        {
            ok = Fail(b, "a prefix and a contraction on one code point", c);
        }
        if (ok)
        {
            Context(b, c, prefixes ? collationTagPrefix : collationTagContraction, entries, n);
        }
        start += n;
    }
    free(entries);
    free(members);
    return ok;
}

// ICU's root data maps each script start's FDD1 line as a contraction of
// U+FDD1 (its sample in NFD, as the runtime reads it), and FDD1 FDD0 to
// the first unassigned primary (its base builder's init); U+FDD1 alone
// stays unassigned.
static void ScriptStartContractions(Builder* b, const RootTable* t)
{
    if (t->scriptStartCount == 0)
    {
        return;
    }
    Entry* entries = GenAllocate(t->scriptStartCount * sizeof(Entry));
    for (size_t i = 0; i < t->scriptStartCount; i++)
    {
        const RootMarker* m = &t->scriptStarts[i];
        uint64_t element = m->codePoint == 0xFDD0
                               ? (uint64_t)0xFE040200 << 32 | collationCommonSecondaryTertiary
                               : m->element;
        entries[i] = (Entry){.ce32 = Encode(b, &element, 1)};
        entries[i].length = CollationSampleText(m->codePoint, entries[i].text);
    }
    Context(b, 0xFDD1, collationTagContraction, entries, t->scriptStartCount);
    free(entries);
}

// A script start in the file's order: its first primary and the script
// or group it starts.
typedef struct Start
{
    uint32_t primary;
    uint32_t code;
    uint32_t order;
} Start;

static int ByPrimary(const void* x, const void* y)
{
    const Start* a = x;
    const Start* b = y;
    return a->primary != b->primary ? (a->primary < b->primary ? -1 : 1)
           : a->order != b->order   ? (a->order < b->order ? -1 : 1)
                                    : 0;
}

// Script entries are two words, the code first.
static int ByCode(const void* x, const void* y)
{
    const uint64_t* a = x;
    const uint64_t* b = y;
    return a[0] != b[0] ? (a[0] < b[0] ? -1 : 1) : 0;
}

// The script or group an FDD1 line's sample code point names, as ICU's
// genuca reads it: five samples name the groups, FDD0 the unassigned
// range, any other its script; 0 for a sample of no script.
static uint32_t SampleScript(uint32_t sample)
{
    static const uint32_t groups[][2] = {
        {0x00A0, collationGroupSpace},  {0x201C, collationGroupPunctuation},
        {0x263A, collationGroupSymbol}, {0x20AC, collationGroupCurrency},
        {0x0034, collationGroupDigit},  {0xFDD0, MUNI_SCRIPT('Z', 'z', 'z', 'z')},
    };
    for (size_t i = 0; i < sizeof(groups) / sizeof(groups[0]); i++)
    {
        if (groups[i][0] == sample)
        {
            return groups[i][1];
        }
    }
    uint32_t script = muniGetScript(sample);
    return script == MUNI_SCRIPT('Z', 'y', 'y', 'y') || script == MUNI_SCRIPT('Z', 'i', 'n', 'h') ||
                   script == MUNI_SCRIPT('Z', 'z', 'z', 'z')
               ? 0
               : script;
}

static void AddScript(Builder* b, uint32_t code, uint32_t index)
{
    CollationPush(&b->scripts, code);
    CollationPush(&b->scripts, index);
}

// ICU's addScriptStart: a start is the primary's top 16 bits, put back to
// its lead byte when it is that byte's lowest; scripts of one range share
// it; the trailing range's start closes them.
static int ScriptTable(Builder* b, const RootTable* t)
{
    size_t count = t->scriptStartCount + t->specialCount;
    Start* starts = GenAllocate(count * sizeof(Start) + sizeof(Start));
    size_t n = 0;
    for (size_t i = 0; i < t->scriptStartCount; i++)
    {
        const RootMarker* m = &t->scriptStarts[i];
        uint32_t code = SampleScript(m->codePoint);
        if (code == 0)
        {
            free(starts);
            return Fail(b, "a script start whose sample has no script", m->codePoint);
        }
        uint32_t primary = code == MUNI_SCRIPT('Z', 'z', 'z', 'z')
                               ? (uint32_t)collationUnassignedByte << 24 | 0x040200
                               : (uint32_t)(m->element >> 32);
        starts[n++] = (Start){primary, code, (uint32_t)i};
    }
    for (size_t i = 0; i < t->specialCount; i++)
    {
        const RootMarker* m = &t->specials[i];
        uint32_t primary = (uint32_t)(m->element >> 32);
        if (m->codePoint == 0x34)
        {
            b->numericPrimary = primary;
        }
        else if (m->codePoint == 0xFF21 || m->codePoint == 0xFF3A)
        {
            uint32_t code =
                m->codePoint == 0xFF21 ? collationReservedBeforeLatin : collationReservedAfterLatin;
            starts[n++] = (Start){primary, code, (uint32_t)(t->scriptStartCount + i)};
        }
    }
    GenSort(starts, n, sizeof(Start), ByPrimary);
    CollationPush(&b->starts, 0);
    for (size_t i = 0; i < n; i++)
    {
        uint32_t p = starts[i].primary >> 16;
        // As ICU's CollationBaseDataBuilder, the unassigned implicit byte
        // counts as compressible here, so its start (FE 04) is the byte's.
        bool compressible = t->compressible[p >> 8] || p >> 8 == collationUnassignedByte;
        uint32_t lowest = compressible ? 4 : 2;
        p = (p & 0xFF) == lowest ? p & 0xFF00 : p;
        size_t last = b->starts.count - 1;
        size_t index = b->starts.items[last] == p ? last : CollationPush(&b->starts, p);
        uint32_t code = starts[i].code;
        AddScript(b, code, (uint32_t)index);
        if (code == MUNI_SCRIPT('H', 'i', 'r', 'a'))
        {
            AddScript(b, MUNI_SCRIPT('H', 'r', 'k', 't'), (uint32_t)index);
        }
        else if (code == MUNI_SCRIPT('H', 'a', 'n', 'i'))
        {
            AddScript(b, MUNI_SCRIPT('H', 'a', 'n', 's'), (uint32_t)index);
            AddScript(b, MUNI_SCRIPT('H', 'a', 'n', 't'), (uint32_t)index);
        }
        if (code == MUNI_SCRIPT('Z', 'z', 'z', 'z'))
        {
            CollationPush(&b->starts, 0xFF00);
        }
    }
    free(starts);
    if (b->scripts.count > 2)
    {
        GenSort(b->scripts.items, b->scripts.count / 2, 2 * sizeof(uint64_t), ByCode);
    }
    for (size_t i = 2; i < b->scripts.count; i += 2)
    {
        if (b->scripts.items[i] == b->scripts.items[i - 2])
        {
            return Fail(b, "two script starts for one script", (uint32_t)b->scripts.items[i]);
        }
    }
    // The trailing range closes the starts, as the unassigned range's
    // start adds it in CLDR's root; a root without one still ends there.
    if (b->starts.items[b->starts.count - 1] != 0xFF00)
    {
        CollationPush(&b->starts, 0xFF00);
    }
    return 1;
}

static uint8_t* Write(const Builder* b, size_t* sizeOut)
{
    size_t trieSize = 0;
    uint8_t* trie = TrieBuild(b->values, &trieSize);
    size_t padded = (trieSize + 7) / 8 * 8;
    size_t size = collationRootHead + padded + b->ces.count * 8 + b->ce32s.count * 4 +
                  (b->listStarts.count + 1) * 4 + b->contexts.count * 4 + b->scripts.count * 4 +
                  b->starts.count * 4 + sizeof(b->compressible);
    uint8_t* out = GenAllocate(size);
    GenPut(out + 4, b->ce32s.count, 4);
    GenPut(out + 8, b->ces.count, 4);
    GenPut(out + 12, b->contexts.count, 4);
    GenPut(out + 16, b->listStarts.count, 4);
    GenPut(out + 20, b->scripts.count / 2, 4);
    GenPut(out + 24, b->starts.count, 4);
    GenPut(out + 28, b->numericPrimary, 4);
    memcpy(out + collationRootHead, trie, trieSize);
    uint8_t* p = out + collationRootHead + padded;
    for (size_t i = 0; i < b->ces.count; i++, p += 8)
    {
        GenPut(p, b->ces.items[i], 8);
    }
    for (size_t i = 0; i < b->ce32s.count; i++, p += 4)
    {
        GenPut(p, b->ce32s.items[i], 4);
    }
    for (size_t i = 0; i < b->listStarts.count; i++, p += 4)
    {
        GenPut(p, b->listStarts.items[i], 4);
    }
    GenPut(p, b->contexts.count, 4);
    p += 4;
    for (size_t i = 0; i < b->contexts.count; i++, p += 4)
    {
        GenPut(p, b->contexts.items[i], 4);
    }
    for (size_t i = 0; i < b->scripts.count; i++, p += 4)
    {
        GenPut(p, b->scripts.items[i], 4);
    }
    for (size_t i = 0; i < b->starts.count; i++, p += 4)
    {
        GenPut(p, b->starts.items[i], 4);
    }
    memcpy(p, b->compressible, sizeof(b->compressible));
    free(trie);
    *sizeOut = size;
    return out;
}

uint8_t* CollationRootUnit(const RootTable* table, size_t* sizeOut, char* error, size_t errorSize)
{
    Builder b = {.error = error, .errorSize = errorSize};
    b.values = GenAllocate((size_t)trieCodePoints * 4);
    for (uint32_t c = 0; c <= trieMaxCodePoint; c++)
    {
        b.values[c] = collationImplicitCe32;
    }
    HanRanges(&b, table);
    b.values[0xFFFD] = collationFffdCe32;
    b.values[0xFFFF] = collationFfffCe32;
    for (size_t i = 0; i < table->count; i++)
    {
        const RootMapping* m = &table->mappings[i];
        if (m->elementCount > collationMaxExpansion)
        {
            free(b.values);
            Fail(&b, "more elements than an expansion holds", m->text[0]);
            return nullptr;
        }
        if (m->prefixLength == 0 && m->textLength == 1 && !CollationLeftOut(m->text[0]))
        {
            b.values[m->text[0]] = Encode(&b, m->elements, m->elementCount);
        }
    }
    OffsetRanges(&b, table);
    // FractionalUCA's COMPRESS top bytes, and the unassigned byte, which
    // ICU's base data builder adds.
    for (uint32_t byte = 0; byte < 256; byte++)
    {
        bool compressible = table->compressible[byte] || byte == collationUnassignedByte;
        b.compressible[byte / 8] |= (uint8_t)(compressible ? 1U << (byte % 8) : 0);
    }
    uint8_t* out = nullptr;
    if (Contexts(&b, table, false) && Contexts(&b, table, true) && ScriptTable(&b, table))
    {
        ScriptStartContractions(&b, table);
        out = Write(&b, sizeOut);
        GenPut(out, (uint64_t)table->version[0] << 16 | table->version[1] << 8 | table->version[2],
               4);
    }
    free(b.values);
    free(b.ces.items);
    free(b.ce32s.items);
    free(b.contexts.items);
    free(b.listStarts.items);
    free(b.scripts.items);
    free(b.starts.items);
    return out;
}

uint8_t* CollationHanUnit(const RootTable* table, size_t* sizeOut)
{
    size_t size = 16 + table->radicalCount * 8;
    uint8_t* out = GenAllocate(size);
    GenPut(out, table->radicalCount, 4);
    GenPut(out + 4, table->radicalCodePoints, 4);
    for (size_t i = 0; i < table->radicalCount; i++)
    {
        const uint32_t* range = table->radicals + 3 * i;
        GenPut(out + 16 + i * 8, range[0] | (range[1] - range[0]) << 21, 4);
        GenPut(out + 20 + i * 8, range[2], 4);
    }
    CollationSeal(out, size, collationHanHashAt);
    *sizeOut = size;
    return out;
}

void CollationSeal(uint8_t* unit, size_t size, uint32_t at)
{
    GenPut(unit + at, mlocCollationUnitHash((DataSpan){unit, (uint32_t)size}, at), 8);
}
