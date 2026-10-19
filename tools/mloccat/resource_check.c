// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalog source checks (resource_check.h).

#include "resource_check.h"

#include "maul-locale/locale.h"
#include "maul-locale/message.h"

#include <stdlib.h>
#include <string.h>

enum
{
    // The longest key, in bytes.
    keyRoom = 1024,
};

typedef struct Checker
{
    const char* path;
    FILE* report;
    int ok;
} Checker;

static void Problem(Checker* c, size_t line, const char* reason, const char* detail)
{
    fprintf(c->report, "%s:%zu: %s%s%s\n", c->path, line, reason, detail[0] != '\0' ? ": " : "",
            detail);
    c->ok = 0;
}

static int TextIs(const ResText* t, const char* s)
{
    return t->length == strlen(s) && memcmp(t->bytes, s, t->length) == 0;
}

int ResourceKey(const ResId* section, const ResId* entry, char* out, size_t capacity)
{
    size_t length = 0;
    const ResId* ids[2] = {section, entry};
    for (size_t k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < ids[k]->count; i++)
        {
            const ResText* part = &ids[k]->parts[i];
            if (memchr(part->bytes, '.', part->length) != nullptr ||
                memchr(part->bytes, '\0', part->length) != nullptr ||
                length + part->length + 2 > capacity)
            {
                return 0;
            }
            if (length > 0)
            {
                out[length++] = '.';
            }
            memcpy(out + length, part->bytes, part->length);
            length += part->length;
        }
    }
    out[length] = '\0';
    return 1;
}

// The properties the tool reads, and where each may stand.
typedef enum Where
{
    whereResource = 1,
    whereSection = 2,
    whereEntry = 4,
} Where;

typedef struct Property
{
    const char* key;
    unsigned where;
} Property;

static const Property s_properties[] = {
    {"locale", whereResource},
    {"param", whereEntry},
    {"translate", whereSection | whereEntry},
    {"do-not-translate", whereSection | whereEntry},
    {"obsolete", whereEntry},
    {"allow-empty", whereEntry},
    {"mloc-max-length", whereEntry},
    {"mloc-source", whereEntry},
};

static int AllDigits(const ResText* t, int hex)
{
    if (t->length == 0)
    {
        return 0;
    }
    for (size_t i = 0; i < t->length; i++)
    {
        char d = t->bytes[i];
        int digit = (d >= '0' && d <= '9') || (hex && ((d >= 'a' && d <= 'f')));
        if (!digit)
        {
            return 0;
        }
    }
    return 1;
}

// Whether a known property's value has its form; unknown ones pass.
static const char* ValueProblem(const ResMeta* m)
{
    const ResText* v = &m->value;
    if (TextIs(&m->key, "translate"))
    {
        return v->length == 0 || TextIs(v, "yes") || TextIs(v, "no") ? nullptr
                                                                     : "@translate takes yes or no";
    }
    if (TextIs(&m->key, "param"))
    {
        return v->length > 1 && v->bytes[0] == '$' ? nullptr
                                                   : "@param begins with the variable, $name";
    }
    if (TextIs(&m->key, "mloc-max-length"))
    {
        return AllDigits(v, 0) && v->length <= 9 && v->bytes[0] != '0'
                   ? nullptr
                   : "@mloc-max-length takes a count from 1";
    }
    if (TextIs(&m->key, "mloc-source"))
    {
        return AllDigits(v, 1) && v->length == 16 ? nullptr
                                                  : "@mloc-source takes 16 small hex digits";
    }
    int bare = TextIs(&m->key, "obsolete") || TextIs(&m->key, "allow-empty") ||
               TextIs(&m->key, "do-not-translate");
    return bare && v->length > 0 ? "this property takes no value" : nullptr;
}

static void CheckMeta(Checker* c, const ResMeta* meta, size_t count, Where where, size_t line)
{
    for (size_t i = 0; i < count; i++)
    {
        for (size_t p = 0; p < sizeof(s_properties) / sizeof(s_properties[0]); p++)
        {
            if (!TextIs(&meta[i].key, s_properties[p].key))
            {
                continue;
            }
            if ((s_properties[p].where & where) == 0)
            {
                Problem(c, line, "a property where it describes nothing", s_properties[p].key);
            }
            const char* problem = ValueProblem(&meta[i]);
            if (problem != nullptr)
            {
                Problem(c, line, problem, "");
            }
        }
    }
}

static const ResMeta* FindMeta(const ResMeta* meta, size_t count, const char* key)
{
    for (size_t i = 0; i < count; i++)
    {
        if (TextIs(&meta[i].key, key))
        {
            return &meta[i];
        }
    }
    return nullptr;
}

static void CheckLocale(Checker* c, const Resource* r)
{
    const ResMeta* locale = FindMeta(r->meta, r->metaCount, "locale");
    mlocLocale parsed;
    if (locale == nullptr)
    {
        Problem(c, 1, "no @locale in the frontmatter", "");
    }
    else if (mlocParseLocale(locale->value.bytes, locale->value.length, &parsed).status !=
             mloc_success)
    {
        Problem(c, 1, "@locale is not a locale identifier", locale->value.bytes);
    }
}

// An entry's message: given unless obsolete, empty only when allowed,
// and compiling.
static void CheckMessage(Checker* c, const ResItem* item, const char* key)
{
    if (FindMeta(item->meta, item->metaCount, "obsolete") != nullptr)
    {
        return;
    }
    if (item->value.length == 0)
    {
        if (FindMeta(item->meta, item->metaCount, "allow-empty") == nullptr)
        {
            Problem(c, item->line, "an empty message without @allow-empty", key);
        }
        return;
    }
    size_t size = 0;
    mlocMessageError kind = mloc_messageErrorNone;
    mlocTextResult r =
        mlocCompileMessage(item->value.bytes, item->value.length, nullptr, 0, &size, &kind);
    if (r.status == mloc_errorSyntax)
    {
        char detail[keyRoom + 64];
        snprintf(detail, sizeof(detail), "%s, at byte %zu of its message", key, r.offset);
        Problem(c, item->line, "a message that does not compile", detail);
    }
}

// Every key with its line, sorted at the end to find those given twice.
typedef struct SeenKey
{
    char* key;
    size_t line;
} SeenKey;

typedef struct Seen
{
    SeenKey* keys;
    size_t count;
} Seen;

static void AddSeen(Seen* seen, const char* key, size_t line)
{
    SeenKey* keys = realloc(seen->keys, (seen->count + 1) * sizeof(*keys));
    size_t length = strlen(key) + 1;
    char* copy = malloc(length);
    if (keys == nullptr || copy == nullptr)
    {
        fputs("mloccat: out of memory\n", stderr);
        exit(1);
    }
    memcpy(copy, key, length);
    seen->keys = keys;
    seen->keys[seen->count++] = (SeenKey){copy, line};
}

static int CompareSeen(const void* a, const void* b)
{
    const SeenKey* x = a;
    const SeenKey* y = b;
    int order = strcmp(x->key, y->key);
    return order != 0 ? order : (x->line > y->line) - (x->line < y->line);
}

static void CheckTwice(Checker* c, Seen* seen)
{
    if (seen->count < 2)
    {
        return;
    }
    qsort(seen->keys, seen->count, sizeof(seen->keys[0]), CompareSeen);
    for (size_t i = 1; i < seen->count; i++)
    {
        if (strcmp(seen->keys[i].key, seen->keys[i - 1].key) == 0)
        {
            char detail[keyRoom + 32];
            snprintf(detail, sizeof(detail), "%s, first on line %zu", seen->keys[i].key,
                     seen->keys[i - 1].line);
            Problem(c, seen->keys[i].line, "a key given twice", detail);
        }
    }
}

static void CheckEntry(Checker* c, const ResSection* s, const ResItem* item, Seen* seen)
{
    char key[keyRoom];
    CheckMeta(c, item->meta, item->metaCount, whereEntry, item->line);
    if (!ResourceKey(&s->id, &item->id, key, sizeof(key)))
    {
        Problem(c, item->line, "a key with a dot inside a part, a NUL, or past 1,023 bytes", "");
        return;
    }
    AddSeen(seen, key, item->line);
    CheckMessage(c, item, key);
}

int ResourceCheck(const Resource* resource, const char* path, FILE* report)
{
    Checker c = {path, report, 1};
    CheckLocale(&c, resource);
    CheckMeta(&c, resource->meta, resource->metaCount, whereResource, 1);
    Seen seen = {0};
    for (size_t s = 0; s < resource->sectionCount; s++)
    {
        const ResSection* section = &resource->sections[s];
        CheckMeta(&c, section->meta, section->metaCount, whereSection, section->line);
        for (size_t i = 0; i < section->itemCount; i++)
        {
            if (section->items[i].kind == resEntry)
            {
                CheckEntry(&c, section, &section->items[i], &seen);
            }
        }
    }
    CheckTwice(&c, &seen);
    for (size_t i = 0; i < seen.count; i++)
    {
        free(seen.keys[i].key);
    }
    free(seen.keys);
    return c.ok;
}
