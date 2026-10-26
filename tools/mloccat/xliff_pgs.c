// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Selection messages as PGS units (xliff_pgs.h).

#include "xliff_pgs.h"

#include "catalog_set.h"
#include "print.h"

#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/locale.h"
#include "maul-locale/plural.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const s_categories[] = {"zero", "one", "two", "few", "many", "other"};

enum
{
    categoryCount = 6,
    // Integer examples before the list ends in an ellipsis.
    exampleItems = 6,
};

static char* Copy(const char* text)
{
    size_t length = strlen(text);
    char* copy = malloc(length + 1);
    if (copy == nullptr)
    {
        exit(1);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static int CategoryOf(const char* value)
{
    for (int c = 0; c < categoryCount; c++)
    {
        if (strcmp(value, s_categories[c]) == 0)
        {
            return c;
        }
    }
    return -1;
}

// A number literal as MF2 writes one: digits with an optional sign,
// fraction and exponent.
static int IsNumber(const char* value)
{
    char* end = nullptr;
    (void)strtod(value, &end);
    return *value != '\0' && *end == '\0' && (*value == '-' || (*value >= '0' && *value <= '9'));
}

// A case value PGS can carry: not empty and without a space.
static int IsToken(const char* value)
{
    if (*value == '\0')
    {
        return 0;
    }
    for (const char* s = value; *s != '\0'; s++)
    {
        if ((unsigned char)*s <= ' ')
        {
            return 0;
        }
    }
    return 1;
}

static int Has(char* const* values, size_t count, const char* value)
{
    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(values[i], value) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int ByNumber(const void* a, const void* b)
{
    double x = strtod(*(const char* const*)a, nullptr);
    double y = strtod(*(const char* const*)b, nullptr);
    return x < y ? -1 : x > y;
}

static int ByBytes(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static int Rules(const char* locale, PgsKind kind, mlocPluralRules* rules)
{
    static mlocData s_builtin;
    static mlocDataSet s_data;
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    mlocLocale parsed;
    mlocPluralType type = kind == pgsOrdinal ? mloc_pluralOrdinal : mloc_pluralCardinal;
    if (mlocParseLocale(locale, strlen(locale), &parsed).status != mloc_success)
    {
        return 0;
    }
    mlocResult r = mlocInitPluralRules(rules, &s_data, &parsed, type);
    return r == mloc_success || r == mloc_fellBack;
}

static void Push(char*** values, size_t* count, char* value)
{
    *values = (char**)CatalogGrow((void*)*values, *count, sizeof(char*));
    (*values)[(*count)++] = value;
}

// A selector's case values; 0 when a source key is none.
static int Values(XliffPgsUnit* u, size_t column, const mlocPluralRules* rules)
{
    const PgsMessage* m = &u->source;
    PgsKind kind = m->selectors[column].kind;
    char*** values = &u->values[column];
    size_t* count = &u->valueCount[column];
    for (size_t v = 0; v < m->variantCount; v++)
    {
        const char* value = m->variants[v].cases[column];
        int numeric = kind != pgsSelect && IsNumber(value);
        int ok = IsToken(value) &&
                 (kind == pgsSelect
                      ? strcmp(value, "other") != 0 || strcmp(m->variants[v].keys[column], "*") == 0
                      : numeric || CategoryOf(value) >= 0);
        if (!ok)
        {
            return 0;
        }
        if ((kind == pgsSelect ? strcmp(value, "other") != 0 : numeric) &&
            !Has(*values, *count, value))
        {
            Push(values, count, Copy(value));
        }
    }
    if (*count > 1)
    {
        qsort((void*)*values, *count, sizeof(char*), kind == pgsSelect ? ByBytes : ByNumber);
    }
    uint32_t categories = 1U << mloc_pluralOther;
    if (kind != pgsSelect && mlocGetPluralCategories(rules, &categories) != mloc_success)
    {
        return 0;
    }
    for (int c = 0; c < categoryCount; c++)
    {
        if (categories & 1U << c)
        {
            Push(values, count, Copy(s_categories[c]));
        }
    }
    return 1;
}

// Appends "a" or "a~b" for a run, and its separator.
static void Item(char* text, size_t room, size_t* items, const char* first, const char* last)
{
    size_t at = strlen(text);
    snprintf(text + at, room - at, strcmp(first, last) == 0 ? "%s%s" : "%s%s~%s",
             *items > 0 ? ", " : "", first, last);
    (*items)++;
}

// The integers 0 to 1,000 in a category as runs, then the powers of ten
// to a million in it, and for cardinals the decimals 0.0 to 2.5: a note
// of CLDR's form ("0, 2~16, 102, 1002").
static char* Examples(const mlocPluralRules* rules, int cardinal, mlocPluralCategory want)
{
    char text[256] = "";
    size_t items = 0;
    int64_t start = -1;
    for (int64_t n = 0; n <= 1001 && items < exampleItems; n++)
    {
        mlocPluralCategory c = mloc_pluralOther;
        int in = n <= 1000 && mlocSelectPluralInteger(rules, n, &c) == mloc_success && c == want;
        if (in && start < 0)
        {
            start = n;
        }
        else if (!in && start >= 0)
        {
            char a[24];
            char b[24];
            snprintf(a, sizeof a, "%lld", (long long)start);
            snprintf(b, sizeof b, "%lld", (long long)(n - 1));
            Item(text, sizeof text, &items, a, b);
            start = -1;
        }
    }
    for (int64_t n = 10000; n <= 1000000 && items < exampleItems; n *= 10)
    {
        mlocPluralCategory c = mloc_pluralOther;
        if (mlocSelectPluralInteger(rules, n, &c) == mloc_success && c == want)
        {
            char a[24];
            snprintf(a, sizeof a, "%lld", (long long)n);
            Item(text, sizeof text, &items, a, a);
        }
    }
    if (items == exampleItems)
    {
        Item(text, sizeof text, &items, "…", "…");
    }
    char first[8] = "";
    char last[8] = "";
    for (uint64_t k = 0; cardinal && k <= 26; k++)
    {
        mlocPluralOperands o = {k / 10, k % 10, k % 10, 1, k % 10 != 0, 0};
        mlocPluralCategory c = mloc_pluralOther;
        int in = k <= 25 && mlocSelectPluralOperands(rules, &o, &c) == mloc_success && c == want;
        if (in)
        {
            if (first[0] == '\0')
            {
                snprintf(first, sizeof first, "%u.%u", (unsigned)(k / 10), (unsigned)(k % 10));
            }
            snprintf(last, sizeof last, "%u.%u", (unsigned)(k / 10), (unsigned)(k % 10));
        }
        else if (first[0] != '\0')
        {
            Item(text, sizeof text, &items, first, last);
            first[0] = '\0';
        }
    }
    return items > 0 ? Copy(text) : nullptr;
}

// The note for a segment: each plural selector's examples for its
// category, named by its variable when there is more than one.
static char* Note(const XliffPgsUnit* u, const XliffPgsSegment* s, const mlocPluralRules* rules)
{
    size_t plurals = 0;
    for (size_t k = 0; k < u->source.selectorCount; k++)
    {
        plurals += u->source.selectors[k].kind != pgsSelect;
    }
    char* note = nullptr;
    size_t length = 0;
    for (size_t k = 0; k < u->source.selectorCount; k++)
    {
        const PgsSelector* sel = &u->source.selectors[k];
        int category = sel->kind != pgsSelect ? CategoryOf(s->cases[k]) : -1;
        char* examples = category >= 0 ? Examples(&rules[k], sel->kind == pgsPlural,
                                                  (mlocPluralCategory)category)
                                       : nullptr;
        if (examples != nullptr)
        {
            size_t add =
                strlen(examples) + (length > 0 ? 2 : 0) + (plurals > 1 ? strlen(sel->name) + 3 : 0);
            char* grown = realloc(note, length + add + 1);
            if (grown == nullptr)
            {
                exit(1);
            }
            note = grown;
            length +=
                (size_t)snprintf(note + length, add + 1, plurals > 1 ? "%s$%s: %s" : "%s%.0s%s",
                                 length > 0 ? "; " : "", sel->name, examples);
            free(examples);
        }
    }
    return note;
}

static int SameSelectors(const PgsMessage* a, const PgsMessage* b)
{
    int same = a->selectorCount == b->selectorCount;
    for (size_t k = 0; same && k < a->selectorCount; k++)
    {
        same = a->selectors[k].kind == b->selectors[k].kind &&
               strcmp(a->selectors[k].name, b->selectors[k].name) == 0;
    }
    return same;
}

// The segments: every tuple of values, the first selector's slowest.
static int Segments(XliffPgsUnit* u, const mlocPluralRules* rules)
{
    size_t total = 1;
    for (size_t k = 0; k < u->source.selectorCount; k++)
    {
        total *= u->valueCount[k];
        if (total > xliffPgsMaxSegments)
        {
            return 0;
        }
    }
    u->segments = calloc(total, sizeof(XliffPgsSegment));
    if (u->segments == nullptr)
    {
        exit(1);
    }
    for (size_t i = 0; i < total; i++)
    {
        XliffPgsSegment* s = &u->segments[u->segmentCount++];
        size_t rest = i;
        for (size_t k = u->source.selectorCount; k-- > 0;)
        {
            s->cases[k] = u->values[k][rest % u->valueCount[k]];
            rest /= u->valueCount[k];
        }
        s->source = PgsFind(&u->source, (char* const*)s->cases);
        s->target =
            u->target.variantCount > 0 ? PgsFind(&u->target, (char* const*)s->cases) : nullptr;
        s->examples = Note(u, s, rules);
    }
    return 1;
}

int XliffPgsPlan(XliffPgsUnit* out, const ResText* source, const ResText* target,
                 const char* locale)
{
    *out = (XliffPgsUnit){0};
    int ok = PgsRead(source->bytes, source->length, &out->source);
    if (ok && target != nullptr)
    {
        ok = PgsRead(target->bytes, target->length, &out->target) &&
             SameSelectors(&out->source, &out->target);
    }
    mlocPluralRules rules[pgsMaxSelectors];
    for (size_t k = 0; ok && k < out->source.selectorCount; k++)
    {
        PgsKind kind = out->source.selectors[k].kind;
        ok = (kind == pgsSelect || Rules(locale, kind, &rules[k])) && Values(out, k, &rules[k]);
    }
    ok = ok && Segments(out, rules);
    size_t length = 0;
    for (size_t k = 0; ok && k < out->source.selectorCount; k++)
    {
        const PgsSelector* sel = &out->source.selectors[k];
        const char* word = sel->kind == pgsPlural    ? "plural"
                           : sel->kind == pgsOrdinal ? "ordinal"
                                                     : "select";
        size_t add = strlen(word) + strlen(sel->name) + 2;
        char* grown = realloc(out->switchText, length + add + 1);
        if (grown == nullptr)
        {
            exit(1);
        }
        out->switchText = grown;
        // add counts the word, the name, ":" and the space before.
        if (!TextPrint(out->switchText, length + add + 1, &length, "%s%s:%s", k > 0 ? " " : "",
                       word, sel->name))
        {
            exit(1);
        }
    }
    if (!ok)
    {
        XliffPgsFree(out);
    }
    return ok;
}

void XliffPgsFree(XliffPgsUnit* unit)
{
    for (size_t i = 0; i < unit->segmentCount; i++)
    {
        free(unit->segments[i].examples);
    }
    for (size_t k = 0; k < pgsMaxSelectors; k++)
    {
        for (size_t i = 0; i < unit->valueCount[k]; i++)
        {
            free(unit->values[k][i]);
        }
        free((void*)unit->values[k]);
    }
    free(unit->segments);
    free(unit->switchText);
    PgsFree(&unit->source);
    PgsFree(&unit->target);
    *unit = (XliffPgsUnit){0};
}

// An ASCII name MF2 reads unquoted: a letter or '_', then those, digits,
// '-' and '.'.
static int IsPlainName(const char* value)
{
    int ok = (*value >= 'a' && *value <= 'z') || (*value >= 'A' && *value <= 'Z') || *value == '_';
    for (const char* s = value; ok && *s != '\0'; s++)
    {
        ok = (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
             *s == '_' || *s == '-' || *s == '.';
    }
    return ok;
}

// MF2's number literal: an optional '-', 0 or digits not starting with 0,
// then optionally '.' and digits, then optionally an exponent.
static int IsNumberLiteral(const char* s)
{
    s += *s == '-';
    if (*s == '0')
    {
        s++;
    }
    else if (*s >= '1' && *s <= '9')
    {
        s += strspn(s, "0123456789");
    }
    else
    {
        return 0;
    }
    if (*s == '.')
    {
        size_t n = strspn(s + 1, "0123456789");
        s += n > 0 ? n + 1 : 0;
    }
    if (*s == 'e' || *s == 'E')
    {
        s += 1 + (s[1] == '-' || s[1] == '+');
        size_t n = strspn(s, "0123456789");
        if (n == 0)
        {
            return 0;
        }
        s += n;
    }
    return *s == '\0';
}

// A case value as a key: '*' for other, the value itself when MF2 reads
// it unquoted, else a quoted literal.
static char* KeyOf(const char* value)
{
    if (strcmp(value, "other") == 0)
    {
        return Copy("*");
    }
    if (IsPlainName(value) || IsNumberLiteral(value))
    {
        return Copy(value);
    }
    size_t length = strlen(value);
    char* key = malloc(2 * length + 3);
    if (key == nullptr)
    {
        exit(1);
    }
    size_t n = 0;
    key[n++] = '|';
    for (size_t i = 0; i < length; i++)
    {
        if (value[i] == '\\' || value[i] == '|')
        {
            key[n++] = '\\';
        }
        key[n++] = value[i];
    }
    key[n++] = '|';
    key[n] = '\0';
    return key;
}

char* XliffPgsFold(const PgsMessage* source, char* const* const* cases, char* const* patterns,
                   size_t count, size_t* lengthOut)
{
    size_t selectors = source->selectorCount;
    char*** keys = (char***)calloc(count + 1, sizeof(char**));
    if (keys == nullptr)
    {
        exit(1);
    }
    for (size_t v = 0; v < count; v++)
    {
        keys[v] = (char**)calloc(selectors, sizeof(char*));
        if (keys[v] == nullptr)
        {
            exit(1);
        }
        for (size_t k = 0; k < selectors; k++)
        {
            keys[v][k] = KeyOf(cases[v][k]);
        }
    }
    char* text =
        PgsWrite(source->head, (char* const* const*)keys, patterns, count, selectors, lengthOut);
    for (size_t v = 0; v < count; v++)
    {
        for (size_t k = 0; k < selectors; k++)
        {
            free(keys[v][k]);
        }
        free((void*)keys[v]);
    }
    free((void*)keys);
    return text;
}
