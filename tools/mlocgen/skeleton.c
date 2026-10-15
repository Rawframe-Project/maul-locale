// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date skeletons matched to patterns (skeleton.h), following ICU's
// DateTimePatternGenerator step by step, so that the patterns equal
// those CLDR's own generator and ICU give for the same data.

#include "skeleton.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    // The distance types of ICU's field table.
    typeNarrow = -0x101,
    typeShorter = -0x102,
    typeShort = -0x103,
    typeLong = -0x104,
    typeNumeric = 0x100,
    typeDelta = 0x10,
    extraField = 0x10000,
    missingField = 0x1000,
    // ICU's field masks.
    dateMask = (1 << skeletonDayPeriod) - 1,
    timeMask = (1 << skeletonFieldCount) - 1 - dateMask,
    fractionMask = 1 << skeletonFraction,
    secondAndFractionMask = (1 << skeletonSecond) | fractionMask,
    // Flags of a match.
    flagCapitalJ = 1,
    flagFixFraction = 2,
};

typedef struct FieldType
{
    char letter;
    uint8_t field;
    int16_t type;
    uint8_t minimum;
} FieldType;

// ICU's dtTypes: each letter's field, distance type and least length,
// rows of one letter in increasing length.
static const FieldType s_types[] = {
    {'G', 0, typeShort, 1},
    {'G', 0, typeLong, 4},
    {'G', 0, typeNarrow, 5},
    {'y', 1, typeNumeric, 1},
    {'Y', 1, typeNumeric + typeDelta, 1},
    {'u', 1, typeNumeric + 2 * typeDelta, 1},
    {'r', 1, typeNumeric + 3 * typeDelta, 1},
    {'U', 1, typeShort, 1},
    {'U', 1, typeLong, 4},
    {'U', 1, typeNarrow, 5},
    {'Q', 2, typeNumeric, 1},
    {'Q', 2, typeShort, 3},
    {'Q', 2, typeLong, 4},
    {'Q', 2, typeNarrow, 5},
    {'q', 2, typeNumeric + typeDelta, 1},
    {'q', 2, typeShort - typeDelta, 3},
    {'q', 2, typeLong - typeDelta, 4},
    {'q', 2, typeNarrow - typeDelta, 5},
    {'M', 3, typeNumeric, 1},
    {'M', 3, typeShort, 3},
    {'M', 3, typeLong, 4},
    {'M', 3, typeNarrow, 5},
    {'L', 3, typeNumeric + typeDelta, 1},
    {'L', 3, typeShort - typeDelta, 3},
    {'L', 3, typeLong - typeDelta, 4},
    {'L', 3, typeNarrow - typeDelta, 5},
    {'l', 3, typeNumeric + typeDelta, 1},
    {'w', 4, typeNumeric, 1},
    {'W', 5, typeNumeric, 1},
    {'E', 6, typeShort, 1},
    {'E', 6, typeLong, 4},
    {'E', 6, typeNarrow, 5},
    {'E', 6, typeShorter, 6},
    {'c', 6, typeNumeric + 2 * typeDelta, 1},
    {'c', 6, typeShort - 2 * typeDelta, 3},
    {'c', 6, typeLong - 2 * typeDelta, 4},
    {'c', 6, typeNarrow - 2 * typeDelta, 5},
    {'c', 6, typeShorter - 2 * typeDelta, 6},
    {'e', 6, typeNumeric + typeDelta, 1},
    {'e', 6, typeShort - typeDelta, 3},
    {'e', 6, typeLong - typeDelta, 4},
    {'e', 6, typeNarrow - typeDelta, 5},
    {'e', 6, typeShorter - typeDelta, 6},
    {'d', 9, typeNumeric, 1},
    {'d', 9, typeShort, 3},
    {'g', 9, typeNumeric + typeDelta, 1},
    {'D', 7, typeNumeric, 1},
    {'F', 8, typeNumeric, 1},
    {'a', 10, typeShort, 1},
    {'a', 10, typeLong, 4},
    {'a', 10, typeNarrow, 5},
    {'b', 10, typeShort - typeDelta, 1},
    {'b', 10, typeLong - typeDelta, 4},
    {'b', 10, typeNarrow - typeDelta, 5},
    {'B', 10, typeShort - 3 * typeDelta, 1},
    {'B', 10, typeLong - 3 * typeDelta, 4},
    {'B', 10, typeNarrow - 3 * typeDelta, 5},
    {'H', 11, typeNumeric + 10 * typeDelta, 1},
    {'k', 11, typeNumeric + 11 * typeDelta, 1},
    {'h', 11, typeNumeric, 1},
    {'K', 11, typeNumeric + typeDelta, 1},
    {'J', 11, typeNumeric + 5 * typeDelta, 1},
    {'j', 11, typeNumeric + 6 * typeDelta, 1},
    {'C', 11, typeNumeric + 7 * typeDelta, 1},
    {'m', 12, typeNumeric, 1},
    {'s', 13, typeNumeric, 1},
    {'A', 13, typeNumeric + typeDelta, 1},
    {'S', 14, typeNumeric, 1},
    {'v', 15, typeShort - 2 * typeDelta, 1},
    {'v', 15, typeLong - 2 * typeDelta, 4},
    {'z', 15, typeShort, 1},
    {'z', 15, typeLong, 4},
    {'Z', 15, typeNarrow - typeDelta, 1},
    {'Z', 15, typeLong - typeDelta, 4},
    {'Z', 15, typeShort - typeDelta, 5},
    {'O', 15, typeShort - typeDelta, 1},
    {'O', 15, typeLong - typeDelta, 4},
    {'V', 15, typeShort - typeDelta, 1},
    {'V', 15, typeLong - typeDelta, 2},
    {'V', 15, typeLong - 1 - typeDelta, 3},
    {'V', 15, typeLong - 2 - typeDelta, 4},
    {'X', 15, typeNarrow - typeDelta, 1},
    {'X', 15, typeShort - typeDelta, 2},
    {'X', 15, typeLong - typeDelta, 4},
    {'x', 15, typeNarrow - typeDelta, 1},
    {'x', 15, typeShort - typeDelta, 2},
    {'x', 15, typeLong - typeDelta, 4},
};

enum
{
    typeCount = sizeof(s_types) / sizeof(s_types[0]),
};

// The canonical single fields ICU adds first: GyQMwWEDFdaHmsSv.
static const char s_canonical[] = "GyQMwWEDFdaHmsSv";

static bool IsLetter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// The length of the token at p: a run of one letter, or one byte.
static size_t TokenLength(const char* p)
{
    size_t length = 1;
    while (IsLetter(p[0]) && p[length] == p[0])
    {
        length += 1;
    }
    return length;
}

// ICU's getCanonicalIndex, not strict: the row of a run of one letter,
// or -1 for a letter the table lacks.
static int CanonicalIndex(const char* p, size_t length)
{
    int best = -1;
    for (int i = 0; i < typeCount; i++)
    {
        if (s_types[i].letter != p[0])
        {
            continue;
        }
        best = i;
        if (i + 1 == typeCount || s_types[i + 1].letter != p[0] || s_types[i + 1].minimum > length)
        {
            return i;
        }
    }
    return best;
}

// The end of a quoted literal starting at p (on its quote): past the
// closing quote, a doubled quote inside kept.
static const char* QuoteEnd(const char* p)
{
    for (p++; *p != '\0'; p++)
    {
        if (*p == '\'')
        {
            if (p[1] != '\'')
            {
                return p + 1;
            }
            p++;
        }
    }
    return p;
}

static void Populate(SkeletonFields* f, int field, char letter, size_t length)
{
    f->chars[field] = letter;
    f->lengths[field] = (uint8_t)length;
}

static int FirstRow(int field)
{
    int i = 0;
    while (s_types[i].field != field)
    {
        i++;
    }
    return i;
}

// ICU's adjustments: seconds forced between minutes and fractions; a
// day period added to a 12-hour hour, dropped from a 24-hour one.
static void FixFields(Skeleton* s)
{
    if (s->original.lengths[skeletonMinute] != 0 && s->original.lengths[skeletonFraction] != 0 &&
        s->original.lengths[skeletonSecond] == 0)
    {
        const FieldType* row = &s_types[FirstRow(skeletonSecond)];
        Populate(&s->original, skeletonSecond, row->letter, row->minimum);
        Populate(&s->base, skeletonSecond, row->letter, row->minimum);
        s->types[skeletonSecond] = (int16_t)(row->type > 0 ? row->type + 1 : row->type);
    }
    char hour = s->original.chars[skeletonHour];
    if (s->original.lengths[skeletonHour] == 0)
    {
        return;
    }
    if (hour == 'h' || hour == 'K')
    {
        if (s->original.lengths[skeletonDayPeriod] == 0)
        {
            const FieldType* row = &s_types[FirstRow(skeletonDayPeriod)];
            Populate(&s->original, skeletonDayPeriod, row->letter, row->minimum);
            Populate(&s->base, skeletonDayPeriod, row->letter, row->minimum);
            s->types[skeletonDayPeriod] = row->type;
        }
    }
    else
    {
        Populate(&s->original, skeletonDayPeriod, 0, 0);
        Populate(&s->base, skeletonDayPeriod, 0, 0);
        s->types[skeletonDayPeriod] = 0;
    }
}

void SkeletonParse(const char* text, Skeleton* out)
{
    memset(out, 0, sizeof(*out));
    for (const char* p = text; *p != '\0';)
    {
        if (*p == '\'')
        {
            p = QuoteEnd(p);
            continue;
        }
        size_t length = TokenLength(p);
        int index = IsLetter(*p) ? CanonicalIndex(p, length) : -1;
        if (index >= 0)
        {
            const FieldType* row = &s_types[index];
            Populate(&out->original, row->field, *p, length);
            Populate(&out->base, row->field, row->letter, row->minimum);
            out->types[row->field] = (int16_t)(row->type > 0 ? row->type + (int)length : row->type);
        }
        p += length;
    }
    FixFields(out);
}

static int HeadIndex(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A';
    }
    return c >= 'a' && c <= 'z' ? 26 + c - 'a' : -1;
}

static char FirstChar(const SkeletonFields* f)
{
    for (int i = 0; i < skeletonFieldCount; i++)
    {
        if (f->lengths[i] != 0)
        {
            return f->chars[i];
        }
    }
    return 0;
}

static bool SameFields(const SkeletonFields* a, const SkeletonFields* b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

static char* Copy(const char* text)
{
    size_t length = strlen(text);
    char* copy = malloc(length + 1);
    if (copy == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

// The entry of a base, or -1: the first with the same base fields.
static int FindBase(const PatternGenerator* g, const SkeletonFields* base)
{
    int head = HeadIndex(FirstChar(base));
    for (int e = head < 0 ? -1 : g->heads[head]; e >= 0; e = g->entries[e].next)
    {
        if (SameFields(&g->entries[e].skeleton.base, base))
        {
            return e;
        }
    }
    return -1;
}

// The entry of a skeleton, or -1: the first with the same fields.
static int FindSkeleton(const PatternGenerator* g, const Skeleton* s)
{
    int head = HeadIndex(FirstChar(&s->base));
    for (int e = head < 0 ? -1 : g->heads[head]; e >= 0; e = g->entries[e].next)
    {
        if (SameFields(&g->entries[e].skeleton.original, &s->original))
        {
            return e;
        }
    }
    return -1;
}

// ICU's PatternMap::add: a duplicate (same base, same types) takes the
// new pattern; otherwise the entry goes to the end of its letter's list.
static void Put(PatternGenerator* g, const Skeleton* s, const char* pattern, bool specified)
{
    int head = HeadIndex(FirstChar(&s->base));
    if (head < 0)
    {
        return;
    }
    int last = -1;
    for (int e = g->heads[head]; e >= 0; last = e, e = g->entries[e].next)
    {
        PatternEntry* entry = &g->entries[e];
        if (SameFields(&entry->skeleton.base, &s->base) &&
            memcmp(entry->skeleton.types, s->types, sizeof(s->types)) == 0)
        {
            free(entry->pattern);
            entry->pattern = Copy(pattern);
            entry->specified = specified;
            return;
        }
    }
    if (g->count == g->capacity)
    {
        g->capacity = g->capacity == 0 ? 256 : g->capacity * 2;
        PatternEntry* grown = realloc(g->entries, (size_t)g->capacity * sizeof(PatternEntry));
        if (grown == nullptr)
        {
            fputs("mlocgen: out of memory\n", stderr);
            exit(1);
        }
        g->entries = grown;
    }
    g->entries[g->count] = (PatternEntry){
        .skeleton = *s, .pattern = Copy(pattern), .specified = specified, .next = -1};
    if (last < 0)
    {
        g->heads[head] = g->count;
    }
    else
    {
        g->entries[last].next = g->count;
    }
    g->count += 1;
}

static bool KeySeen(PatternGenerator* g, const char* key)
{
    for (int i = 0; i < g->keyCount; i++)
    {
        if (strcmp(g->keys[i], key) == 0)
        {
            return true;
        }
    }
    char** grown = realloc(g->keys, (size_t)(g->keyCount + 1) * sizeof(char*));
    if (grown == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    g->keys = grown;
    g->keys[g->keyCount++] = Copy(key);
    return false;
}

// ICU's addPatternWithOptionalSkeleton, override false for a pattern of
// its own skeleton and true for an availableFormats entry.
void PatternGeneratorAdd(PatternGenerator* g, const char* pattern, const char* skeleton)
{
    if (skeleton != nullptr && KeySeen(g, skeleton))
    {
        return;
    }
    bool override = skeleton != nullptr;
    Skeleton s;
    SkeletonParse(skeleton != nullptr ? skeleton : pattern, &s);
    int duplicate = FindBase(g, &s.base);
    if (duplicate >= 0 && !g->entries[duplicate].specified && !override)
    {
        return;
    }
    duplicate = FindSkeleton(g, &s);
    if (duplicate >= 0 && (!override || g->entries[duplicate].specified))
    {
        return;
    }
    Put(g, &s, pattern, override);
}

void PatternGeneratorInit(PatternGenerator* g)
{
    memset(g, 0, sizeof(*g));
    for (int i = 0; i < 52; i++)
    {
        g->heads[i] = -1;
    }
    g->hourChar = 'h';
    g->preferredHour = 'h';
    g->preferredPeriod = 'a';
    for (const char* c = s_canonical; *c != '\0'; c++)
    {
        char item[2] = {*c, '\0'};
        PatternGeneratorAdd(g, item, nullptr);
    }
}

void PatternGeneratorFree(PatternGenerator* g)
{
    for (int i = 0; i < g->count; i++)
    {
        free(g->entries[i].pattern);
    }
    for (int i = 0; i < g->keyCount; i++)
    {
        free(g->keys[i]);
    }
    for (int i = 0; i < skeletonFieldCount; i++)
    {
        free(g->appendItems[i]);
        free(g->fieldNames[i]);
    }
    for (int i = 0; i < 4; i++)
    {
        free(g->glue[i]);
    }
    free(g->decimal);
    free(g->entries);
    free(g->keys);
    memset(g, 0, sizeof(*g));
}

// A growing string.
typedef struct Text
{
    char* bytes;
    size_t length;
    size_t capacity;
} Text;

static void Append(Text* t, const char* bytes, size_t length)
{
    if (t->length + length + 1 > t->capacity)
    {
        t->capacity = (t->length + length + 1) * 2;
        char* grown = realloc(t->bytes, t->capacity);
        if (grown == nullptr)
        {
            fputs("mlocgen: out of memory\n", stderr);
            exit(1);
        }
        t->bytes = grown;
    }
    memcpy(t->bytes + t->length, bytes, length);
    t->length += length;
    t->bytes[t->length] = '\0';
}

static void AppendRun(Text* t, char letter, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        Append(t, &letter, 1);
    }
}

typedef struct Match
{
    // The request, after j, J and C were replaced.
    Skeleton request;
    int flags;
    int missing;
    int extra;
} Match;

// ICU's getDistance over the fields of a mask.
static int Distance(const Match* m, int mask, const Skeleton* trial, int* missingOut, int* extraOut)
{
    int result = 0;
    *missingOut = 0;
    *extraOut = 0;
    for (int i = 0; i < skeletonFieldCount; i++)
    {
        int mine = (mask & (1 << i)) == 0 ? 0 : m->request.types[i];
        int other = trial->types[i];
        if (mine == other)
        {
            continue;
        }
        if (mine == 0)
        {
            result += extraField;
            *extraOut |= 1 << i;
        }
        else if (other == 0)
        {
            result += missingField;
            *missingOut |= 1 << i;
        }
        else
        {
            result += abs(mine - other);
        }
    }
    return result;
}

// A two-digit year matches only a request for one.
static bool YearCompatible(const Skeleton* request, const Skeleton* trial)
{
    if (request->types[1] <= 0 || trial->types[1] <= 0)
    {
        return true;
    }
    return trial->original.lengths[1] != 2 || request->original.lengths[1] == 2;
}

// ICU's getBestRaw: the entry nearest the request over a mask, ties to
// the larger missing mask, in the map's order. Returns the entry whose
// pattern is used (the first with the best entry's fields).
static int BestRaw(const PatternGenerator* g, Match* m, int mask)
{
    int bestDistance = INT32_MAX;
    int bestMissing = -1;
    int best = -1;
    for (int head = 0; head < 52; head++)
    {
        for (int e = g->heads[head]; e >= 0; e = g->entries[e].next)
        {
            const Skeleton* trial = &g->entries[e].skeleton;
            int missing = 0;
            int extra = 0;
            if (!YearCompatible(&m->request, trial))
            {
                continue;
            }
            int distance = Distance(m, mask, trial, &missing, &extra);
            if (distance < bestDistance || (distance == bestDistance && bestMissing < missing))
            {
                bestDistance = distance;
                bestMissing = missing;
                best = FindSkeleton(g, trial);
                m->missing = missing;
                m->extra = extra;
                if (distance == 0)
                {
                    return best;
                }
            }
        }
    }
    return best;
}

// An hour letter turned to the locale's cycle, as ICU adjusts it.
static char HourLetter(const PatternGenerator* g, const Match* m, char request, char found)
{
    char c = found;
    char own = g->hourChar;
    if ((m->flags & flagCapitalJ) != 0 || request == own)
    {
        c = own;
    }
    else if (request == 'h' && own == 'K')
    {
        c = 'K';
    }
    else if (request == 'H' && own == 'k')
    {
        c = 'k';
    }
    else if (request == 'k' && own == 'H')
    {
        c = 'H';
    }
    else if (request == 'K' && own == 'h')
    {
        c = 'h';
    }
    return c;
}

// One pattern field adjusted to the request (ICU's adjustFieldTypes).
static void AdjustField(const PatternGenerator* g, const Match* m, const char* p, size_t length,
                        const FieldType* row, const Skeleton* specified, Text* out)
{
    int field = row->field;
    char request = m->request.original.chars[field];
    size_t requestLength = m->request.original.lengths[field];
    if (request == 'E' && requestLength < 3)
    {
        requestLength = 3;
    }
    size_t adjusted = requestLength;
    if (field == skeletonHour || field == skeletonMinute || field == skeletonSecond)
    {
        adjusted = length;
    }
    else if (specified != nullptr && request != 'c' && request != 'e')
    {
        bool patternNumeric = row->type > 0;
        bool requestNumeric = m->request.types[field] > 0;
        if (specified->original.lengths[field] == requestLength || patternNumeric != requestNumeric)
        {
            adjusted = length;
        }
    }
    char c = (field != skeletonHour && field != 3 && field != 6 && (field != 1 || request == 'Y'))
                 ? request
                 : p[0];
    if (c == 'E' && adjusted < 3)
    {
        c = 'e';
    }
    if (field == skeletonHour && g->hourChar != 0)
    {
        c = HourLetter(g, m, request, c);
    }
    AppendRun(out, c, adjusted);
}

static char* Adjust(const PatternGenerator* g, const Match* m, const char* pattern,
                    const Skeleton* specified, int flags)
{
    Text out = {0};
    Append(&out, "", 0);
    for (const char* p = pattern; *p != '\0';)
    {
        if (*p == '\'')
        {
            const char* end = QuoteEnd(p);
            Append(&out, p, (size_t)(end - p));
            p = end;
            continue;
        }
        size_t length = TokenLength(p);
        int index = IsLetter(*p) ? CanonicalIndex(p, length) : -1;
        const FieldType* row = index < 0 ? nullptr : &s_types[index];
        if (row != nullptr && (flags & flagFixFraction) != 0 && row->field == skeletonSecond)
        {
            const char* decimal = g->decimal != nullptr ? g->decimal : ".";
            Append(&out, p, length);
            Append(&out, decimal, strlen(decimal));
            AppendRun(&out, m->request.original.chars[skeletonFraction],
                      m->request.original.lengths[skeletonFraction]);
        }
        else if (row != nullptr && m->request.types[row->field] != 0)
        {
            AdjustField(g, m, p, length, row, specified, &out);
        }
        else
        {
            Append(&out, p, length);
        }
        p += length;
    }
    return out.bytes;
}

// ICU's SimpleFormatter: {0} to {9} replaced; a doubled apostrophe is
// one, and an apostrophe before a brace quotes up to the next one.
static char* Format(const char* pattern, const char* const* arguments, int count)
{
    Text out = {0};
    Append(&out, "", 0);
    bool quoted = false;
    for (const char* p = pattern; *p != '\0'; p++)
    {
        if (*p == '\'' && p[1] == '\'')
        {
            Append(&out, p, 1);
            p++;
        }
        else if (*p == '\'' && (quoted || p[1] == '{' || p[1] == '}'))
        {
            quoted = !quoted;
        }
        else if (!quoted && p[0] == '{' && p[1] >= '0' && p[1] < '0' + count && p[2] == '}')
        {
            const char* argument = arguments[p[1] - '0'];
            Append(&out, argument, strlen(argument));
            p += 2;
        }
        else
        {
            Append(&out, p, 1);
        }
    }
    return out.bytes;
}

// The highest field of a mask, at most the zone; 0 for none.
static int TopBit(int mask)
{
    int top = -1;
    for (; mask != 0; mask >>= 1)
    {
        top++;
    }
    return top < 0 ? 0 : (top > skeletonZone ? skeletonZone : top);
}

static const Skeleton* Specified(const PatternGenerator* g, int entry)
{
    return entry >= 0 && g->entries[entry].specified ? &g->entries[entry].skeleton : nullptr;
}

// Appends a missing field's pattern by its appendItem.
static char* AppendMissing(const PatternGenerator* g, char* result, const char* found, int field)
{
    const char* item = g->appendItems[field];
    if (item == nullptr || item[0] == '\0')
    {
        return result;
    }
    Text name = {0};
    Append(&name, "'", 1);
    const char* fieldName = g->fieldNames[field] != nullptr ? g->fieldNames[field] : "";
    Append(&name, fieldName, strlen(fieldName));
    Append(&name, "'", 1);
    const char* arguments[3] = {result, found, name.bytes};
    char* formatted = Format(item, arguments, 3);
    free(name.bytes);
    free(result);
    return formatted;
}

// ICU's getBestAppending: the best entry over the fields of a mask, then
// each field still missing appended.
static char* BestAppending(const PatternGenerator* g, Match* m, int fields)
{
    if (fields == 0)
    {
        return Copy("");
    }
    int entry = BestRaw(g, m, fields);
    const Skeleton* specified = Specified(g, entry);
    char* result = Adjust(g, m, g->entries[entry].pattern, specified, m->flags);
    int last = 0;
    while (m->missing != 0 && last != m->missing)
    {
        if ((m->missing & secondAndFractionMask) == fractionMask &&
            (fields & secondAndFractionMask) == secondAndFractionMask)
        {
            char* fixed = Adjust(g, m, result, specified, m->flags | flagFixFraction);
            free(result);
            result = fixed;
            m->missing &= ~fractionMask;
            continue;
        }
        int starting = m->missing;
        entry = BestRaw(g, m, m->missing);
        specified = Specified(g, entry);
        char* found = Adjust(g, m, g->entries[entry].pattern, specified, m->flags);
        result = AppendMissing(g, result, found, TopBit(starting & ~m->missing));
        free(found);
        last = m->missing;
    }
    return result;
}

// ICU's mapSkeletonMetacharacters: j to the locale's hour letter, C to
// its preferred hour format with that format's day period, J to H; a
// run's length sets the hour's and the day period's.
static void MapMetacharacters(const PatternGenerator* g, const char* skeleton, Text* out,
                              int* flags)
{
    bool quoted = false;
    for (const char* p = skeleton; *p != '\0'; p++)
    {
        if (*p == '\'')
        {
            quoted = !quoted;
            continue;
        }
        if (quoted)
        {
            continue;
        }
        if (*p == 'j' || *p == 'C')
        {
            int extra = 0;
            for (; p[1] == p[0]; p++)
            {
                extra++;
            }
            int hourLength = 1 + (extra & 1);
            int periodLength = extra < 2 ? 1 : 3 + (extra >> 1);
            char hour = *p == 'j' ? g->hourChar : g->preferredHour;
            char period = *p == 'j' ? 'a' : g->preferredPeriod;
            if (hour == 'H' || hour == 'k' || period == 0)
            {
                periodLength = 0;
            }
            AppendRun(out, period, (size_t)periodLength);
            AppendRun(out, hour, (size_t)hourLength);
        }
        else if (*p == 'J')
        {
            Append(out, "H", 1);
            *flags |= flagCapitalJ;
        }
        else
        {
            Append(out, p, 1);
        }
    }
}

// The glue length ICU picks from the request's month and weekday: full,
// long, medium or short.
static int GlueLength(const Skeleton* request)
{
    int month = request->base.lengths[3];
    if (month == 4)
    {
        return request->base.lengths[6] > 0 ? 0 : 1;
    }
    return month == 3 ? 2 : 3;
}

char* PatternGeneratorBest(PatternGenerator* g, const char* skeleton)
{
    Match m = {0};
    Text mapped = {0};
    Append(&mapped, "", 0);
    MapMetacharacters(g, skeleton, &mapped, &m.flags);
    SkeletonParse(mapped.bytes, &m.request);
    free(mapped.bytes);
    int entry = BestRaw(g, &m, -1);
    if (m.missing == 0 && m.extra == 0)
    {
        return Adjust(g, &m, g->entries[entry].pattern, Specified(g, entry), m.flags);
    }
    int needed = 0;
    for (int i = 0; i < skeletonFieldCount; i++)
    {
        needed |= m.request.types[i] != 0 ? 1 << i : 0;
    }
    char* date = BestAppending(g, &m, needed & dateMask);
    char* time = BestAppending(g, &m, needed & timeMask);
    if (date[0] == '\0' || time[0] == '\0')
    {
        char* one = date[0] == '\0' ? time : date;
        free(one == date ? time : date);
        return one;
    }
    const char* glue = g->glue[GlueLength(&m.request)];
    const char* arguments[2] = {time, date};
    char* result = Format(glue != nullptr ? glue : "{1} {0}", arguments, 2);
    free(date);
    free(time);
    return result;
}
