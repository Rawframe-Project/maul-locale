// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's root collation (collation_root.h). The weights are read as ICU's
// genuca.cpp reads them (parseWeight, parseCE), and Han's primaries are
// made as its CollationBaseDataBuilder::initHanRanges makes them.

#include "collation_root.h"

#include "collation_tables.h"
#include "table_gen.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    // Second and third primary bytes a lead byte holds, neither
    // compressible.
    hanPerLeadByte = 254 * 254,
    commonSecondaryTertiary = 0x05000500,
};

typedef struct Reader
{
    RootTable* table;
    unsigned line;
    char* error;
    size_t errorSize;
    // The radical ranges' capacity, and whether the mapping being read
    // named a Han character's element.
    size_t radicalCapacity;
    bool namedHan;
} Reader;

static int Fail(Reader* r, const char* what)
{
    snprintf(r->error, r->errorSize, "line %u: %s", r->line, what);
    return 0;
}

static int Hex(char c)
{
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : -1;
}

static const char* Space(const char* s)
{
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    return s;
}

// A weight of up to most bytes ("0A 86"), left-aligned in 32 bits, ending
// at one of the separators; 0 bytes is a weight of 0. Inner 00 and 01
// bytes are refused, as ICU refuses them.
static int Weight(Reader* r, const char** at, const char* separators, int most, uint32_t* out)
{
    const char* s = *at;
    uint32_t weight = 0;
    int bytes = 0;
    while (Hex(s[0]) >= 0 && Hex(s[1]) >= 0)
    {
        int high = Hex(s[0]);
        int low = Hex(s[1]);
        if (bytes == most || (bytes > 0 && high == 0 && low <= 1))
        {
            return Fail(r, "a weight of too many bytes or with an inner 00 or 01");
        }
        weight = weight << 8 | (uint32_t)(high << 4 | low);
        bytes++;
        s += 2;
        if (*s != ' ')
        {
            break;
        }
        s++;
    }
    if (*s == '\0' || strchr(separators, *s) == nullptr)
    {
        return Fail(r, "a weight not followed by its separator");
    }
    for (; bytes < 4; bytes++)
    {
        weight <<= 8;
    }
    *at = s;
    *out = weight;
    return 1;
}

// One code point in hex.
static int CodePoint(Reader* r, const char** at, uint32_t* out)
{
    char* end = nullptr;
    unsigned long value = strtoul(*at, &end, 16);
    if (end == *at || value > 0x10FFFF)
    {
        return Fail(r, "not a code point");
    }
    *at = end;
    *out = (uint32_t)value;
    return 1;
}

// A code point's one element, as ICU's getSingleCE gives it: Han's, or
// that of a mapping of the code point alone to one element; 0 for none.
static uint64_t SingleElement(const RootTable* t, uint32_t codePoint)
{
    uint64_t han = RootHanElement(t, codePoint);
    if (han != 0)
    {
        return han;
    }
    for (size_t i = 0; i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        if (m->textLength == 1 && m->prefixLength == 0 && m->text[0] == codePoint)
        {
            return m->elementCount == 1 ? m->elements[0] : 0;
        }
    }
    return 0;
}

// An element: [p, s, t], or [U+XXXX] with an optional tertiary, or
// secondary and tertiary, replacing that code point's own.
static int Element(Reader* r, const char** at, uint64_t* out)
{
    const char* s = *at + 1;
    uint32_t w = 0;
    if (s[0] == 'U' && s[1] == '+')
    {
        s += 2;
        uint32_t c = 0;
        if (!CodePoint(r, &s, &c))
        {
            return 0;
        }
        uint64_t element = SingleElement(r->table, c);
        r->namedHan = r->namedHan || RootHanElement(r->table, c) != 0;
        if (element == 0)
        {
            return Fail(r, "an element naming a code point without one element of its own");
        }
        if (*s == ',')
        {
            s = Space(s + 1);
            if (!Weight(r, &s, ",]", 2, &w))
            {
                return 0;
            }
            if (*s == ']')
            {
                element = (element & 0xFFFFFFFFFFFF0000ULL) | w >> 16;
            }
            else
            {
                element = (element & 0xFFFFFFFF00000000ULL) | w;
                s = Space(s + 1);
                if (!Weight(r, &s, "]", 2, &w))
                {
                    return 0;
                }
                element |= w >> 16;
            }
        }
        if (*s != ']')
        {
            return Fail(r, "an element naming a code point not closed");
        }
        *at = s + 1;
        *out = element;
        return 1;
    }
    uint32_t p = 0;
    uint32_t secondary = 0;
    uint32_t tertiary = 0;
    if (!Weight(r, &s, ",", 4, &p))
    {
        return 0;
    }
    s = Space(s + 1);
    if (!Weight(r, &s, ",", 2, &secondary))
    {
        return 0;
    }
    s = Space(s + 1);
    if (!Weight(r, &s, "]", 2, &tertiary))
    {
        return 0;
    }
    *at = s + 1;
    *out = (uint64_t)p << 32 | secondary | tertiary >> 16;
    return 1;
}

// Code points up to a stop character.
static int Text(Reader* r, const char** at, char stop, uint32_t* out, uint32_t* length)
{
    const char* s = Space(*at);
    *length = 0;
    while (*s != stop && *s != '|' && *s != '\0')
    {
        if (*length == rootMaxText)
        {
            return Fail(r, "a string of too many code points");
        }
        if (!CodePoint(r, &s, &out[(*length)++]))
        {
            return 0;
        }
        s = Space(s);
    }
    *at = s;
    return *length > 0 ? 1 : Fail(r, "no code points");
}

// Room for one more item: 16 at first, then twice the count each time
// the count reaches a power of two from 16.
static void* Grow(void* items, size_t count, size_t size)
{
    if (count != 0 && (count < 16 || (count & (count - 1)) != 0))
    {
        return items;
    }
    void* grown = realloc(items, (count == 0 ? 16 : count * 2) * size);
    if (grown == nullptr)
    {
        GenDie("collation root", "out of memory");
    }
    return grown;
}

// "PREFIX | TEXT; [elements]" or "TEXT; [elements]".
static int Mapping(Reader* r, const char* line)
{
    RootMapping m = {0};
    const char* s = line;
    r->namedHan = false;
    if (!Text(r, &s, ';', m.text, &m.textLength))
    {
        return 0;
    }
    if (*s == '|')
    {
        memcpy(m.prefix, m.text, sizeof(m.text));
        m.prefixLength = m.textLength;
        s++;
        if (!Text(r, &s, ';', m.text, &m.textLength))
        {
            return 0;
        }
    }
    if (*s != ';')
    {
        return Fail(r, "no ';' after the code points");
    }
    s = Space(s + 1);
    while (*s == '[')
    {
        if (m.elementCount == rootMaxElements)
        {
            return Fail(r, "too many elements");
        }
        if (!Element(r, &s, &m.elements[m.elementCount++]))
        {
            return 0;
        }
    }
    if (m.elementCount == 0)
    {
        return Fail(r, "a mapping without elements");
    }
    m.hanReference = r->namedHan;
    RootTable* t = r->table;
    uint32_t first = m.text[0];
    if (m.prefixLength == 0 && m.textLength == 2 && (first == 0xFDD1 || first == 0xFDD0))
    {
        RootMarker** list = first == 0xFDD1 ? &t->scriptStarts : &t->specials;
        size_t* count = first == 0xFDD1 ? &t->scriptStartCount : &t->specialCount;
        *list = Grow(*list, *count, sizeof(RootMarker));
        (*list)[(*count)++] = (RootMarker){m.text[1], m.elements[0]};
        return 1;
    }
    if (m.prefixLength == 0 && m.textLength == 1 && (first == 0xFFFD || first == 0xFFFF))
    {
        // The runtime's fixed weights.
        return 1;
    }
    t->mappings = Grow(t->mappings, t->count, sizeof(RootMapping));
    t->mappings[t->count++] = m;
    return 1;
}

// [Unified_Ideograph 4E00..9FFF FA0E ...]: the Han ranges, then their
// first primary.
static int HanRanges(Reader* r, const char* s)
{
    RootTable* t = r->table;
    uint32_t count = 0;
    s = Space(s);
    while (*s != ']' && *s != '\0')
    {
        uint32_t start = 0;
        uint32_t end = 0;
        if (t->hanRangeCount == rootMaxHanRanges || !CodePoint(r, &s, &start))
        {
            return t->hanRangeCount == rootMaxHanRanges ? Fail(r, "too many Han ranges") : 0;
        }
        end = start;
        if (s[0] == '.' && s[1] == '.')
        {
            s += 2;
            if (!CodePoint(r, &s, &end) || end < start)
            {
                return Fail(r, "a Han range that ends before it starts");
            }
        }
        t->hanRanges[2 * t->hanRangeCount] = start;
        t->hanRanges[2 * t->hanRangeCount + 1] = end;
        t->hanRangeCount++;
        count += end - start + 1;
        s = Space(s);
    }
    uint32_t han = count * rootHanStep + rootHanStep + 2;
    uint32_t leadBytes = (han + hanPerLeadByte - 1) / hanPerLeadByte;
    t->firstHanPrimary = (collationUnassignedByte - leadBytes) << 24 | 0x20200;
    return 1;
}

// One UTF-8 character of a [radical] line, or 0 at its end or on a
// malformed one.
static uint32_t Utf8(const char** at)
{
    const unsigned char* s = (const unsigned char*)*at;
    int length = GenCharLength(s);
    if (length == 0 || *s == ']')
    {
        return 0;
    }
    uint32_t c = length == 1 ? s[0] : s[0] & (0x7Fu >> length);
    for (int i = 1; i < length; i++)
    {
        c = c << 6 | (s[i] & 0x3Fu);
    }
    *at += length;
    return c;
}

static int AddRadical(Reader* r, uint32_t start, uint32_t end)
{
    RootTable* t = r->table;
    if (t->radicalCount == r->radicalCapacity)
    {
        r->radicalCapacity = r->radicalCapacity * 2 + 1024;
        uint32_t* grown = realloc(t->radicals, r->radicalCapacity * 3 * sizeof *grown);
        if (grown == nullptr)
        {
            return Fail(r, "out of memory");
        }
        t->radicals = grown;
    }
    uint32_t* range = t->radicals + t->radicalCount++ * 3;
    range[0] = start;
    range[1] = end;
    range[2] = t->radicalCodePoints;
    t->radicalCodePoints += end - start + 1;
    return 1;
}

// [radical N=...:characters and ranges]: the ranges in radical-stroke
// order, after the radical's own characters before the colon.
static int Radical(Reader* r, const char* s)
{
    const char* colon = strchr(s, ':');
    if (colon == nullptr)
    {
        return Fail(r, "a radical line without its characters");
    }
    s = colon + 1;
    for (uint32_t c = Utf8(&s); c != 0; c = Utf8(&s))
    {
        uint32_t end = c;
        if (*s == '-')
        {
            s++;
            end = Utf8(&s);
            if (end < c)
            {
                return Fail(r, "a radical range that ends before it starts");
            }
        }
        if (!AddRadical(r, c, end))
        {
            return 0;
        }
    }
    return *s == ']' ? 1 : Fail(r, "a radical line not ended by ']'");
}

static int ByStart(const void* x, const void* y)
{
    uint32_t a = *(const uint32_t*)x;
    uint32_t b = *(const uint32_t*)y;
    return a < b ? -1 : a > b;
}

// [radical end]: the ranges sorted by start, disjoint; the unihan root's
// first Han primary from their count.
static int RadicalsEnd(Reader* r)
{
    RootTable* t = r->table;
    if (t->radicalCount > 0)
    {
        qsort(t->radicals, t->radicalCount, 3 * sizeof *t->radicals, ByStart);
    }
    for (size_t i = 1; i < t->radicalCount; i++)
    {
        if (t->radicals[3 * i] <= t->radicals[3 * i - 2])
        {
            return Fail(r, "radical ranges that overlap");
        }
    }
    if (t->radicalOrder)
    {
        uint32_t han = t->radicalCodePoints * rootHanStep + rootHanStep + 2;
        uint32_t leadBytes = (han + hanPerLeadByte - 1) / hanPerLeadByte;
        t->firstHanPrimary = (collationUnassignedByte - leadBytes) << 24 | 0x20200;
    }
    return 1;
}

// [top_byte XX Script] or [top_byte XX Script COMPRESS].
static int TopByte(Reader* r, const char* s)
{
    s = Space(s);
    if (Hex(s[0]) < 0 || Hex(s[1]) < 0)
    {
        return Fail(r, "a top byte that is not two hex digits");
    }
    unsigned byte = (unsigned)(Hex(s[0]) << 4 | Hex(s[1]));
    const char* end = strchr(s, ']');
    const char* compress = strstr(s, "COMPRESS");
    r->table->compressible[byte] = compress != nullptr && (end == nullptr || compress < end);
    return 1;
}

// [fixed ... byte XX], the name already matched.
static int FixedByte(Reader* r, const char* s, uint8_t* out)
{
    s = Space(s);
    if (Hex(s[0]) < 0 || Hex(s[1]) < 0 || Space(s + 2)[0] != ']')
    {
        return Fail(r, "a fixed byte that is not two hex digits");
    }
    *out = (uint8_t)(Hex(s[0]) << 4 | Hex(s[1]));
    return 1;
}

static int Option(Reader* r, const char* line)
{
    static const char lastCommon[] = "[fixed last secondary common byte";
    static const char ignorableSecondary[] = "[fixed first ignorable secondary byte";
    static const char ignorableTertiary[] = "[fixed first ignorable tertiary byte";
    if (strncmp(line, lastCommon, sizeof lastCommon - 1) == 0)
    {
        return FixedByte(r, line + sizeof lastCommon - 1, &r->table->lastCommonSecondaryByte);
    }
    if (strncmp(line, ignorableSecondary, sizeof ignorableSecondary - 1) == 0)
    {
        return FixedByte(r, line + sizeof ignorableSecondary - 1,
                         &r->table->firstIgnorableSecondaryByte);
    }
    if (strncmp(line, ignorableTertiary, sizeof ignorableTertiary - 1) == 0)
    {
        return FixedByte(r, line + sizeof ignorableTertiary - 1,
                         &r->table->firstIgnorableTertiaryByte);
    }
    if (strncmp(line, "[UCA version = ", 15) == 0)
    {
        uint32_t* v = r->table->version;
        return sscanf(line + 15, "%" SCNu32 ".%" SCNu32 ".%" SCNu32, &v[0], &v[1], &v[2]) == 3
                   ? 1
                   : Fail(r, "a version not of three numbers");
    }
    if (strncmp(line, "[Unified_Ideograph", 18) == 0)
    {
        return HanRanges(r, line + 18);
    }
    if (strncmp(line, "[top_byte", 9) == 0)
    {
        return TopByte(r, line + 9);
    }
    if (strcmp(line, "[radical end]") == 0)
    {
        return RadicalsEnd(r);
    }
    if (strncmp(line, "[radical ", 9) == 0)
    {
        return Radical(r, line + 9);
    }
    // The boundaries, categories, reordering tokens and other
    // fixed bytes are not part of the root's elements.
    return 1;
}

static int Line(Reader* r, char* line)
{
    char* hash = strchr(line, '#');
    if (hash != nullptr)
    {
        *hash = '\0';
    }
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ' ||
                     line[n - 1] == '\t'))
    {
        line[--n] = '\0';
    }
    const char* s = Space(line);
    // The BOM a UTF-8 file may start with.
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
    {
        s += 3;
    }
    return *s == '\0' ? 1 : *s == '[' ? Option(r, s) : Mapping(r, s);
}

static int Read(const char* path, RootTable* out, bool radicalOrder, char* error, size_t errorSize)
{
    *out = (RootTable){.radicalOrder = radicalOrder};
    Reader r = {out, 0, error, errorSize, 0, false};
    FILE* f = fopen(path, "rb");
    if (f == nullptr)
    {
        snprintf(error, errorSize, "%s: cannot read", path);
        return 0;
    }
    static char line[65536];
    int ok = 1;
    while (ok && fgets(line, sizeof(line), f) != nullptr)
    {
        r.line++;
        ok = Line(&r, line);
    }
    fclose(f);
    if (ok &&
        (out->hanRangeCount == 0 || out->count == 0 || (radicalOrder && out->radicalCount == 0)))
    {
        snprintf(error, errorSize, "%s: no [Unified_Ideograph] line, no mappings or no radicals",
                 path);
        ok = 0;
    }
    if (!ok)
    {
        RootFree(out);
    }
    return ok;
}

int RootRead(const char* path, RootTable* out, char* error, size_t errorSize)
{
    return Read(path, out, false, error, errorSize);
}

int RootReadUnihan(const char* path, RootTable* out, char* error, size_t errorSize)
{
    return Read(path, out, true, error, errorSize);
}

void RootFree(RootTable* table)
{
    free(table->mappings);
    free(table->scriptStarts);
    free(table->specials);
    free(table->radicals);
    *table = (RootTable){0};
}

// ICU's incThreeBytePrimaryByOffset for a primary that does not compress.
static uint32_t AddToPrimary(uint32_t primary, uint32_t offset)
{
    offset += ((primary >> 8) & 0xFF) - 2;
    uint32_t third = offset % 254 + 2;
    offset /= 254;
    offset += ((primary >> 16) & 0xFF) - 2;
    uint32_t second = offset % 254 + 2;
    offset /= 254;
    return ((primary & 0xFF000000) + (offset << 24)) | second << 16 | third << 8;
}

// The radical range holding a code point, or null.
static const uint32_t* RadicalOf(const RootTable* table, uint32_t codePoint)
{
    size_t lo = 0;
    size_t hi = table->radicalCount;
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        const uint32_t* range = table->radicals + 3 * mid;
        if (codePoint < range[0])
        {
            hi = mid;
        }
        else if (codePoint > range[1])
        {
            lo = mid + 1;
        }
        else
        {
            return range;
        }
    }
    return nullptr;
}

uint64_t RootHanElement(const RootTable* table, uint32_t codePoint)
{
    if (table->radicalOrder)
    {
        const uint32_t* range = RadicalOf(table, codePoint);
        return range == nullptr
                   ? 0
                   : (uint64_t)AddToPrimary(table->firstHanPrimary,
                                            (range[2] + codePoint - range[0]) * rootHanStep)
                             << 32 |
                         commonSecondaryTertiary;
    }
    uint32_t index = 0;
    for (size_t i = 0; i < table->hanRangeCount; i++)
    {
        uint32_t start = table->hanRanges[2 * i];
        uint32_t end = table->hanRanges[2 * i + 1];
        if (codePoint >= start && codePoint <= end)
        {
            uint32_t p =
                AddToPrimary(table->firstHanPrimary, (index + codePoint - start) * rootHanStep);
            return (uint64_t)p << 32 | commonSecondaryTertiary;
        }
        index += end - start + 1;
    }
    return 0;
}
