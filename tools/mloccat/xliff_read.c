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
#include "message_text.h"
#include "resource_edit.h"
#include "stale.h"
#include "xliff_pgs.h"
#include "xml.h"

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

// An attribute of a module, by its local name whatever its prefix.
static const char* ModuleAttribute(const XmlNode* node, const char* name)
{
    for (int i = 0; i < node->attributeCount; i++)
    {
        const char* full = node->attributes[i].name;
        if (strchr(full, ':') != nullptr && strcmp(Local(full), name) == 0)
        {
            return node->attributes[i].value;
        }
    }
    return nullptr;
}

// A PGS unit's segments: each case's values, its source and its target.
typedef struct PgsSegments
{
    char** cases[xliffPgsMaxSegments];
    char* sources[xliffPgsMaxSegments];
    char* targets[xliffPgsMaxSegments];
    size_t count;
    size_t translated;
} PgsSegments;

// A case's values, as many as the selectors; 0 for another count.
static int CaseValues(const char* text, size_t selectors, char*** out)
{
    *out = (char**)calloc(selectors + 1, sizeof(char*));
    if (*out == nullptr)
    {
        exit(1);
    }
    size_t count = 0;
    for (const char* s = text; *s != '\0';)
    {
        size_t n = strcspn(s, " ");
        if (n > 0 && count < selectors)
        {
            Str value = {0};
            Append(&value, s, n);
            (*out)[count] = value.bytes;
        }
        count += n > 0;
        s += n + (s[n] == ' ');
    }
    return count == selectors;
}

static void ReadSegments(Reader* r, const XmlNode* unit, const PgsMessage* source, PgsSegments* p,
                         int* initial)
{
    for (int i = 0; r->problem == nullptr && i < unit->childCount; i++)
    {
        const XmlNode* s = unit->children[i];
        const char* values = ModuleAttribute(s, "case");
        const char* state = XmlGetAttribute(s, "state");
        if (!Is(s, "segment"))
        {
            continue;
        }
        if (values == nullptr || p->count == xliffPgsMaxSegments)
        {
            r->problem = values == nullptr ? "a selection segment without a case"
                                           : "a selection unit of too many segments";
            break;
        }
        size_t at = p->count++;
        if (!CaseValues(values, source->selectorCount, &p->cases[at]))
        {
            r->problem = "a case whose values are not one per selector";
        }
        Str text = {0};
        Append(&text, "", 0);
        const XmlNode* from = Child(s, "source");
        Content(r, from != nullptr ? from : s, &text);
        p->sources[at] = text.bytes;
        const XmlNode* target = Child(s, "target");
        if (target != nullptr)
        {
            Str t = {0};
            Append(&t, "", 0);
            Content(r, target, &t);
            p->targets[at] = t.bytes;
            p->translated++;
        }
        *initial = *initial || state == nullptr || strcmp(state, "initial") == 0;
    }
}

static void FreePgsSegments(PgsSegments* p, size_t selectors)
{
    for (size_t i = 0; i < p->count; i++)
    {
        for (size_t k = 0; k < selectors; k++)
        {
            free(p->cases[i][k]);
        }
        free((void*)p->cases[i]);
        free(p->sources[i]);
        free(p->targets[i]);
    }
}

// The source the segments show: the source's own text when they are its
// segments now, else the message they fold to, whose hash is another.
static void SourceShown(const XliffPgsUnit* plan, const ResText* now, const PgsSegments* p,
                        Str* out)
{
    size_t length = 0;
    char* shown =
        XliffPgsFold(&plan->source, (char* const* const*)p->cases, p->sources, p->count, &length);
    char** patterns = (char**)calloc(plan->segmentCount + 1, sizeof(char*));
    char*** tuples = (char***)calloc(plan->segmentCount + 1, sizeof(char**));
    if (patterns == nullptr || tuples == nullptr)
    {
        exit(1);
    }
    for (size_t i = 0; i < plan->segmentCount; i++)
    {
        tuples[i] = (char**)plan->segments[i].cases;
        patterns[i] = plan->segments[i].source->pattern;
    }
    size_t planLength = 0;
    char* planned = XliffPgsFold(&plan->source, (char* const* const*)tuples, patterns,
                                 plan->segmentCount, &planLength);
    size_t a = 0;
    size_t b = 0;
    char* x = CanonicalMessage(shown, length, &a);
    char* y = CanonicalMessage(planned, planLength, &b);
    int same = x != nullptr && y != nullptr && a == b && memcmp(x, y, a) == 0;
    Append(out, same ? now->bytes : shown, same ? now->length : length);
    free(x);
    free(y);
    free(shown);
    free(planned);
    free((void*)patterns);
    free((void*)tuples);
}

// A unit of the PGS module: its segments folded back into one message.
static Unit ReadPgsUnit(Reader* r, const XmlNode* unit, const ResText* now, const char* locale,
                        const char* switchText)
{
    Unit u = {{0}, {0}, 0, 0};
    r->data = Child(unit, "originalData");
    XliffPgsUnit plan;
    if (!XliffPgsPlan(&plan, now, nullptr, locale))
    {
        r->problem = "a selection unit whose switch is not the source's";
        return u;
    }
    if (strcmp(plan.switchText, switchText) != 0)
    {
        r->problem = "a selection unit whose switch is not the source's";
        XliffPgsFree(&plan);
        return u;
    }
    PgsSegments p = {0};
    ReadSegments(r, unit, &plan.source, &p, &u.initial);
    if (r->problem == nullptr && p.translated > 0 && p.translated < p.count)
    {
        r->problem = "a selection unit translated in part";
    }
    if (r->problem == nullptr && p.translated > 0)
    {
        u.hasTarget = 1;
        u.target.bytes = XliffPgsFold(&plan.source, (char* const* const*)p.cases, p.targets,
                                      p.count, &u.target.length);
        SourceShown(&plan, now, &p, &u.source);
    }
    FreePgsSegments(&p, plan.source.selectorCount);
    XliffPgsFree(&plan);
    return u;
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
static int Choose(const CatalogInput* s, const XmlNode* unit, const char* path, const char* locale,
                  Imports* out, FILE* report)
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
    const char* switchText = ModuleAttribute(unit, "switch");
    Unit u = switchText != nullptr ? ReadPgsUnit(&r, unit, &key->item->value, locale, switchText)
                                   : ReadUnit(&r, unit);
    int ok = r.problem == nullptr;
    if (!ok)
    {
        fprintf(report, "%s:%d: %s\n", path, unit->line, r.problem);
    }
    else if (u.hasTarget && !MessageCompiles(u.target.bytes, u.target.length))
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
static int ChooseAll(const CatalogInput* s, const XmlNode* root, const char* path,
                     const char* locale, Imports* out, FILE* report)
{
    int ok = Is(root, "xliff");
    for (int f = 0; ok && f < root->childCount; f++)
    {
        const XmlNode* file = root->children[f];
        for (int u = 0; Is(file, "file") && u < file->childCount; u++)
        {
            if (Is(file->children[u], "unit"))
            {
                ok = Choose(s, file->children[u], path, locale, out, report) && ok;
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
    if (ok && !ChooseAll(&set.inputs[0], root, xliff, set.inputs[1].locale, &imports, report))
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
