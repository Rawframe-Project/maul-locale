// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// XLIFF 2.2 import (exchange_xliff.h). Elements are matched by their
// local names, so a tool's own prefix for the XLIFF namespace reads too.
// A segment's content becomes MF2 again: text with '\', '{' and '}'
// escaped, each code its data (a <pc> its start data, content and end
// data), <cp> its character, <mrk> its content; <sm> and <em> mark
// nothing in MF2.

#include "catalog_set.h"
#include "exchange_xliff.h"
#include "resource_edit.h"
#include "stale.h"
#include "xml.h"

#include "maul-locale/message.h"

#include <stdlib.h>
#include <string.h>

typedef struct Str
{
    char* bytes;
    size_t length;
} Str;

static void Append(Str* s, const char* bytes, size_t length)
{
    char* grown = realloc(s->bytes, s->length + length + 1);
    if (grown == nullptr)
    {
        exit(1);
    }
    memcpy(grown + s->length, bytes, length);
    s->bytes = grown;
    s->length += length;
    s->bytes[s->length] = '\0';
}

static const char* Local(const char* name)
{
    const char* colon = strchr(name, ':');
    return colon != nullptr ? colon + 1 : name;
}

static int Is(const XmlNode* node, const char* name)
{
    return strcmp(Local(node->name), name) == 0;
}

static const XmlNode* Child(const XmlNode* node, const char* name)
{
    for (int i = 0; i < node->childCount; i++)
    {
        if (Is(node->children[i], name))
        {
            return node->children[i];
        }
    }
    return nullptr;
}

static void AppendUtf8(Str* s, unsigned long cp)
{
    char bytes[4];
    size_t n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    static const unsigned char s_lead[] = {0, 0, 0xC0, 0xE0, 0xF0};
    for (size_t i = n - 1; i > 0; i--)
    {
        bytes[i] = (char)(0x80 | (cp & 0x3F));
        cp >>= 6;
    }
    bytes[0] = (char)(s_lead[n] | cp);
    Append(s, bytes, n);
}

// Text into MF2 pattern text: '\', '{' and '}' escaped.
static void AppendEscaped(Str* s, const char* text, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        if (text[i] == '\\' || text[i] == '{' || text[i] == '}')
        {
            Append(s, "\\", 1);
        }
        Append(s, text + i, 1);
    }
}

typedef struct Reader
{
    const XmlNode* data;
    const char* problem;
} Reader;

// A <data> element's text, its <cp> characters in place, as written.
static int DataText(Reader* r, const char* id, Str* out)
{
    for (int i = 0; r->data != nullptr && id != nullptr && i < r->data->childCount; i++)
    {
        const XmlNode* d = r->data->children[i];
        const char* did = XmlGetAttribute(d, "id");
        if (!Is(d, "data") || did == nullptr || strcmp(did, id) != 0)
        {
            continue;
        }
        size_t from = 0;
        for (int c = 0; c < d->childCount; c++)
        {
            const char* hex = XmlGetAttribute(d->children[c], "hex");
            Append(out, d->text + from, d->children[c]->textAt - from);
            from = d->children[c]->textAt;
            if (Is(d->children[c], "cp") && hex != nullptr)
            {
                AppendUtf8(out, strtoul(hex, nullptr, 16));
            }
        }
        Append(out, d->text + from, d->textLength - from);
        return 1;
    }
    r->problem = "a code names no data";
    return 0;
}

static void Content(Reader* r, const XmlNode* node, Str* out);

static void Inline(Reader* r, const XmlNode* child, Str* out)
{
    if (Is(child, "ph") || Is(child, "sc") || Is(child, "ec"))
    {
        (void)DataText(r, XmlGetAttribute(child, "dataRef"), out);
    }
    else if (Is(child, "pc"))
    {
        (void)DataText(r, XmlGetAttribute(child, "dataRefStart"), out);
        Content(r, child, out);
        (void)DataText(r, XmlGetAttribute(child, "dataRefEnd"), out);
    }
    else if (Is(child, "cp"))
    {
        const char* hex = XmlGetAttribute(child, "hex");
        Str c = {0};
        AppendUtf8(&c, hex != nullptr ? strtoul(hex, nullptr, 16) : 0xFFFD);
        AppendEscaped(out, c.bytes, c.length);
        free(c.bytes);
    }
    else if (Is(child, "mrk"))
    {
        Content(r, child, out);
    }
}

// Mixed content in order: text escaped, each child as its kind.
static void Content(Reader* r, const XmlNode* node, Str* out)
{
    size_t from = 0;
    for (int i = 0; i < node->childCount; i++)
    {
        const XmlNode* child = node->children[i];
        AppendEscaped(out, node->text + from, child->textAt - from);
        from = child->textAt;
        Inline(r, child, out);
    }
    AppendEscaped(out, node->text + from, node->textLength - from);
}

// A unit's source and target, each segment's in order; whether any segment
// has a target, and whether all of them are past "initial".
typedef struct Unit
{
    Str source;
    Str target;
    int hasTarget;
    int initial;
} Unit;

static Unit ReadUnit(Reader* r, const XmlNode* unit)
{
    Unit u = {{0}, {0}, 0, 0};
    r->data = Child(unit, "originalData");
    Append(&u.source, "", 0);
    Append(&u.target, "", 0);
    for (int i = 0; i < unit->childCount; i++)
    {
        const XmlNode* s = unit->children[i];
        if (!Is(s, "segment") && !Is(s, "ignorable"))
        {
            continue;
        }
        const XmlNode* source = Child(s, "source");
        const XmlNode* target = Child(s, "target");
        const char* state = XmlGetAttribute(s, "state");
        if (source != nullptr)
        {
            Content(r, source, &u.source);
        }
        Content(r, target != nullptr ? target : source != nullptr ? source : s, &u.target);
        u.hasTarget = u.hasTarget || (target != nullptr && Is(s, "segment"));
        u.initial =
            u.initial || (Is(s, "segment") && (state == nullptr || strcmp(state, "initial") == 0));
    }
    return u;
}

static int Compiles(const Str* s)
{
    size_t size = 0;
    mlocTextResult r = mlocCompileMessage(s->bytes, s->length, nullptr, 0, &size, nullptr);
    return r.status == mloc_success || r.status == mloc_errorCapacity;
}

// An import: the source's entry, the target's MF2 and the hash to record.
typedef struct Import
{
    const CatalogEntry* source;
    char* text;
    size_t length;
    int record;
    uint64_t hash;
} Import;

typedef struct Imports
{
    Import* items;
    size_t count;
} Imports;

// One unit into the imports; 0 for one that cannot be read back.
static int Choose(const CatalogInput* s, const XmlNode* unit, const char* path, Imports* out,
                  FILE* report)
{
    const char* name = XmlGetAttribute(unit, "name");
    const CatalogEntry* key = name != nullptr ? CatalogFindEntry(s, name) : nullptr;
    if (name == nullptr)
    {
        fprintf(report, "%s:%d: a unit without a name, the key\n", path, unit->line);
        return 0;
    }
    if (key == nullptr || key->keep)
    {
        fprintf(report, "%s:%d: not a key to translate: %s\n", path, unit->line, name);
        return 1;
    }
    Reader r = {nullptr, nullptr};
    Unit u = ReadUnit(&r, unit);
    int ok = r.problem == nullptr;
    if (!ok)
    {
        fprintf(report, "%s:%d: %s\n", path, unit->line, r.problem);
    }
    else if (u.hasTarget && !Compiles(&u.target))
    {
        fprintf(report, "%s:%d: a translation that does not compile: %s\n", path, unit->line, name);
        ok = 0;
    }
    else if (u.hasTarget)
    {
        out->items = CatalogGrow(out->items, out->count, sizeof(Import));
        Import* in = &out->items[out->count++];
        *in = (Import){key, u.target.bytes, u.target.length, 0, 0};
        in->record = !u.initial && SourceHash(u.source.bytes, u.source.length, &in->hash);
        u.target.bytes = nullptr;
    }
    free(u.source.bytes);
    free(u.target.bytes);
    return ok;
}

static void Apply(Resource* r, const Imports* imports)
{
    for (size_t i = 0; i < imports->count; i++)
    {
        const Import* in = &imports->items[i];
        ResItem* item = ResourceEntryFor(r, &in->source->section->id, &in->source->item->id);
        ResourceSetValue(item, in->text, in->length);
        if (in->record)
        {
            char text[sourceHashDigits + 1];
            SourceHashText(in->hash, text);
            ResourceSetMeta(item, "mloc-source", text);
        }
    }
}

// Every unit of every file.
static int ChooseAll(const CatalogInput* s, const XmlNode* root, const char* path, Imports* out,
                     FILE* report)
{
    int ok = Is(root, "xliff");
    for (int f = 0; ok && f < root->childCount; f++)
    {
        const XmlNode* file = root->children[f];
        for (int u = 0; Is(file, "file") && u < file->childCount; u++)
        {
            if (Is(file->children[u], "unit"))
            {
                ok = Choose(s, file->children[u], path, out, report) && ok;
            }
        }
    }
    return ok;
}

int CatalogImportXliff(const char* source, const char* translation, const char* xliff, FILE* report)
{
    char* paths[] = {(char*)source, (char*)translation};
    CatalogSet set;
    int ok = CatalogSetRead(&set, paths, 2, report);
    char error[512] = "";
    XmlNode* root = ok ? XmlParseFile(xliff, error, sizeof(error)) : nullptr;
    if (ok && root == nullptr)
    {
        fprintf(report, "%s: %s\n", xliff, error);
        ok = 0;
    }
    Imports imports = {0};
    if (ok && !ChooseAll(&set.inputs[0], root, xliff, &imports, report))
    {
        if (!Is(root, "xliff"))
        {
            fprintf(report, "%s: not an XLIFF document\n", xliff);
        }
        ok = 0;
    }
    if (ok)
    {
        Apply(&set.inputs[1].resource, &imports);
        ok = ResourceWriteFile(translation, &set.inputs[1].resource);
        fprintf(report, ok ? "%s: %zu entries imported\n" : "%s: cannot write\n", translation,
                imports.count);
    }
    for (size_t i = 0; i < imports.count; i++)
    {
        free(imports.items[i].text);
    }
    free(imports.items);
    XmlFree(root);
    CatalogSetFree(&set);
    return ok;
}
