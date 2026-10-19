// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Selection messages for the PGS module (pgs.h).

#include "pgs.h"

#include "catalog_set.h"
#include "message_text.h"
#include "mf2_parts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* Copy(const char* bytes, size_t length)
{
    char* copy = malloc(length + 1);
    if (copy == nullptr)
    {
        exit(1);
    }
    memcpy(copy, bytes, length);
    copy[length] = '\0';
    return copy;
}

// One declaration line, ".input {$name :function options}", as a selector
// of a kind; 0 when it is another declaration or function.
static int Declaration(const char* line, size_t length, PgsSelector* out)
{
    if (length < 10 || memcmp(line, ".input {$", 9) != 0 || line[length - 1] != '}')
    {
        return 0;
    }
    size_t name = 9;
    size_t at = name;
    while (at < length && line[at] != ' ' && line[at] != '}')
    {
        at++;
    }
    const char* rest = line + at;
    size_t restLength = length - at;
    int number = restLength >= 8 && (memcmp(rest, " :number", 8) == 0 ||
                                     (restLength >= 9 && memcmp(rest, " :integer", 9) == 0));
    int string =
        restLength >= 8 && memcmp(rest, " :string", 8) == 0 && (restLength == 9 || rest[8] == ' ');
    if (!number && !string)
    {
        return 0;
    }
    const char* select = strstr(rest, " select=");
    PgsKind kind = string ? pgsSelect : pgsPlural;
    if (number && select != nullptr && select < line + length)
    {
        kind = strncmp(select + 8, "ordinal", 7) == 0 ? pgsOrdinal
               : strncmp(select + 8, "exact", 5) == 0 ? pgsSelect
                                                      : pgsPlural;
    }
    *out = (PgsSelector){kind, Copy(line + name, at - name)};
    return 1;
}

// The declaration named by a selector ("$name"), not yet taken; count
// when none. A name taken is null, so a selector named twice finds none.
static size_t Declared(const PgsSelector* declared, size_t count, const char* name, size_t n)
{
    size_t d = 0;
    while (d < count && (declared[d].name == nullptr || strlen(declared[d].name) != n ||
                         memcmp(declared[d].name, name, n) != 0))
    {
        d++;
    }
    return d;
}

// The .match line's selectors, each a declaration taken in its order.
static int Match(const char* line, size_t n, PgsSelector* declared, size_t count, PgsMessage* m)
{
    int ok = 1;
    for (size_t i = 7; ok && i < n;)
    {
        const char* space = memchr(line + i, ' ', n - i);
        size_t stop = space != nullptr ? (size_t)(space - line) : n;
        size_t d = Declared(declared, count, line + i + 1, stop - i - 1);
        ok = line[i] == '$' && d < count && m->selectorCount < pgsMaxSelectors;
        if (ok)
        {
            m->selectors[m->selectorCount++] = declared[d];
            declared[d].name = nullptr;
        }
        i = stop + 1;
    }
    return ok;
}

// The head: its .input lines become the selectors the .match line names,
// in its order; 0 when a declaration is another or names no selector.
static int Head(const char* head, size_t length, PgsMessage* m)
{
    PgsSelector declared[pgsMaxSelectors];
    size_t count = 0;
    int ok = 1;
    for (size_t at = 0; ok && at < length;)
    {
        const char* nl = memchr(head + at, '\n', length - at);
        size_t end = nl != nullptr ? (size_t)(nl - head) : length;
        const char* line = head + at;
        size_t n = end - at;
        if (n > 7 && memcmp(line, ".match ", 7) == 0)
        {
            ok = Match(line, n, declared, count, m);
        }
        else if (count < pgsMaxSelectors && Declaration(line, n, &declared[count]))
        {
            count++;
        }
        else
        {
            ok = 0;
        }
        at = end + 1;
    }
    for (size_t d = 0; d < count; d++)
    {
        // A declaration the match does not use is one PGS cannot carry.
        ok = ok && declared[d].name == nullptr;
        free(declared[d].name);
    }
    return ok && m->selectorCount > 0;
}

// A key as a case value.
static char* CaseOf(const char* key, size_t length)
{
    if (length == 1 && key[0] == '*')
    {
        return Copy("other", 5);
    }
    if (length < 2 || key[0] != '|')
    {
        return Copy(key, length);
    }
    char* value = Copy(key + 1, length - 2);
    size_t n = 0;
    for (size_t i = 1; i + 1 < length; i++)
    {
        i += key[i] == '\\';
        value[n++] = key[i];
    }
    value[n] = '\0';
    return value;
}

// A variant's keys from "k1 k2 ... ", quoted keys kept whole.
static int Keys(const char* text, size_t length, PgsMessage* m, PgsVariant* v)
{
    size_t count = 0;
    for (size_t i = 0; i < length;)
    {
        size_t stop = i;
        int quoted = 0;
        while (stop < length && (quoted || text[stop] != ' '))
        {
            quoted = text[stop] == '|' ? !quoted : quoted;
            stop += text[stop] == '\\' ? 2 : 1;
        }
        stop = stop < length ? stop : length;
        if (stop > i && count < pgsMaxSelectors)
        {
            v->keys[count] = Copy(text + i, stop - i);
            v->cases[count] = CaseOf(text + i, stop - i);
        }
        count += stop > i;
        i = stop + 1;
    }
    return count == m->selectorCount;
}

// A variant whose pattern starts at start: its keys run from its line's
// start to " {{"; the first variant's head is what comes before them.
static int Variant(const char* c, size_t start, size_t from, PgsMessage* out)
{
    size_t keysEnd = start - 3;
    size_t keysStart = keysEnd;
    while (keysStart > from && c[keysStart - 1] != '\n')
    {
        keysStart--;
    }
    int ok = 1;
    if (out->head == nullptr)
    {
        ok = Head(c, keysStart > 0 ? keysStart - 1 : 0, out);
        out->head = Copy(c, keysStart);
    }
    out->variants = CatalogGrow(out->variants, out->variantCount, sizeof(PgsVariant));
    PgsVariant* v = &out->variants[out->variantCount++];
    *v = (PgsVariant){0};
    return ok && Keys(c + keysStart, keysEnd - keysStart, out, v);
}

int PgsRead(const char* text, size_t length, PgsMessage* out)
{
    *out = (PgsMessage){0};
    size_t n = 0;
    char* c = CanonicalMessage(text, length, &n);
    Mf2Part* parts = nullptr;
    size_t count = 0;
    int ok = c != nullptr && c[0] == '.' && Mf2Split(c, n, &parts, &count);
    size_t from = 0;
    size_t begin = 0;
    for (size_t i = 0; ok && i < count; i++)
    {
        if (parts[i].kind == mf2PatternStart)
        {
            begin = parts[i].start;
            ok = Variant(c, begin, from, out);
        }
        else if (parts[i].kind == mf2PatternEnd && out->variantCount > 0)
        {
            out->variants[out->variantCount - 1].pattern = Copy(c + begin, parts[i].start - begin);
            from = parts[i].start + 2;
        }
    }
    ok = ok && out->variantCount > 0;
    free(parts);
    free(c);
    if (!ok)
    {
        PgsFree(out);
    }
    return ok;
}

void PgsFree(PgsMessage* m)
{
    for (size_t i = 0; i < m->selectorCount; i++)
    {
        free(m->selectors[i].name);
    }
    for (size_t v = 0; v < m->variantCount; v++)
    {
        for (size_t k = 0; k < pgsMaxSelectors; k++)
        {
            free(m->variants[v].keys[k]);
            free(m->variants[v].cases[k]);
        }
        free(m->variants[v].pattern);
    }
    free(m->variants);
    free(m->head);
    *m = (PgsMessage){0};
}

const PgsVariant* PgsFind(const PgsMessage* m, char* const* cases)
{
    for (int exact = 1; exact >= 0; exact--)
    {
        for (size_t v = 0; v < m->variantCount; v++)
        {
            const PgsVariant* variant = &m->variants[v];
            size_t k = 0;
            while (k < m->selectorCount && (strcmp(variant->cases[k], cases[k]) == 0 ||
                                            (!exact && strcmp(variant->keys[k], "*") == 0)))
            {
                k++;
            }
            if (k == m->selectorCount)
            {
                return variant;
            }
        }
    }
    return nullptr;
}

char* PgsWrite(const char* head, char* const* const* keys, char* const* patterns, size_t count,
               size_t selectorCount, size_t* lengthOut)
{
    size_t length = strlen(head);
    for (size_t v = 0; v < count; v++)
    {
        for (size_t k = 0; k < selectorCount; k++)
        {
            length += strlen(keys[v][k]) + 1;
        }
        length += strlen(patterns[v]) + 5;
    }
    char* text = malloc(length + 1);
    if (text == nullptr)
    {
        exit(1);
    }
    size_t at = (size_t)snprintf(text, length + 1, "%s", head);
    for (size_t v = 0; v < count; v++)
    {
        for (size_t k = 0; k < selectorCount; k++)
        {
            at += (size_t)snprintf(text + at, length + 1 - at, "%s ", keys[v][k]);
        }
        at += (size_t)snprintf(text + at, length + 1 - at, "{{%s}}%s", patterns[v],
                               v + 1 < count ? "\n" : "");
    }
    *lengthOut = at;
    return text;
}
