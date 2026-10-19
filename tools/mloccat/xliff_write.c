// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// XLIFF 2.2 export (exchange_xliff.h).

#include "catalog_set.h"
#include "exchange_xliff.h"
#include "mf2_parts.h"
#include "stale.h"
#include "xliff_codes.h"
#include "xliff_pgs.h"

#include <stdlib.h>
#include <string.h>

static void Line(FILE* out, int indent, const char* text)
{
    fprintf(out, "%*s%s", indent, "", text);
}

// The source entry's comment and parameters, and a PGS unit's plural
// examples for the segments they apply to.
static void WriteNotes(FILE* out, const CatalogEntry* e, const XliffPgsUnit* pgs)
{
    int examples = 0;
    for (size_t i = 0; pgs != nullptr && i < pgs->segmentCount; i++)
    {
        examples |= pgs->segments[i].examples != nullptr;
    }
    int any = e->item->comment.length > 0 || examples ||
              CatalogFindMeta(e->item->meta, e->item->metaCount, "param") != nullptr;
    if (!any)
    {
        return;
    }
    Line(out, 6, "<notes>\n");
    if (e->item->comment.length > 0)
    {
        Line(out, 8, "<note category=\"comment\">");
        XliffWriteText(out, e->item->comment.bytes, e->item->comment.length, 0);
        fputs("</note>\n", out);
    }
    for (size_t i = 0; i < e->item->metaCount; i++)
    {
        const ResMeta* m = &e->item->meta[i];
        if (m->key.length == 5 && memcmp(m->key.bytes, "param", 5) == 0)
        {
            Line(out, 8, "<note category=\"param\">");
            XliffWriteText(out, m->value.bytes, m->value.length, 0);
            fputs("</note>\n", out);
        }
    }
    for (size_t i = 0; pgs != nullptr && i < pgs->segmentCount; i++)
    {
        const char* text = pgs->segments[i].examples;
        if (text != nullptr)
        {
            fprintf(out,
                    "        <note appliesTo=\"target\" ref=\"s%zu\" "
                    "category=\"plural_examples\">",
                    i + 1);
            XliffWriteText(out, text, strlen(text), 0);
            fputs("</note>\n", out);
        }
    }
    Line(out, 6, "</notes>\n");
}

// A segment: its source and, with a translation, its target.
static void WriteSegment(FILE* out, const XliffCodes* codes, size_t source, size_t target,
                         const CatalogEntry* done)
{
    Line(out, 8, "<source>");
    XliffCodesWriteMessage(codes, source, out);
    fputs("</source>\n", out);
    if (done != nullptr)
    {
        Line(out, 8, "<target>");
        XliffCodesWriteMessage(codes, target, out);
        fputs("</target>\n", out);
    }
    Line(out, 6, "</segment>\n");
}

static const char* StateOf(const CatalogEntry* e, const CatalogEntry* done)
{
    int stale = done != nullptr && SourceStateOf(done->item->meta, done->item->metaCount,
                                                 &e->item->value) == sourceStale;
    return done == nullptr ? "" : stale ? " state=\"initial\"" : " state=\"translated\"";
}

// A selection message's segments, one per tuple of case values.
static void WritePgsSegments(FILE* out, XliffCodes* codes, const XliffPgsUnit* u,
                             const CatalogEntry* done, const char* state)
{
    size_t* indices = calloc(2 * u->segmentCount + 1, sizeof(size_t));
    if (indices == nullptr)
    {
        exit(1);
    }
    for (size_t i = 0; i < u->segmentCount; i++)
    {
        const XliffPgsSegment* g = &u->segments[i];
        indices[2 * i] = XliffCodesAddPattern(codes, g->source->pattern, strlen(g->source->pattern),
                                              xliffSource);
        if (g->target != nullptr)
        {
            indices[2 * i + 1] = XliffCodesAddPattern(codes, g->target->pattern,
                                                      strlen(g->target->pattern), indices[2 * i]);
        }
    }
    XliffCodesWriteData(codes, out);
    for (size_t i = 0; i < u->segmentCount; i++)
    {
        fprintf(out, "      <segment id=\"s%zu\" pgs:case=\"", i + 1);
        for (size_t k = 0; k < u->source.selectorCount; k++)
        {
            fputs(k > 0 ? " " : "", out);
            XliffWriteText(out, u->segments[i].cases[k], strlen(u->segments[i].cases[k]), 1);
        }
        fprintf(out, "\"%s>\n", state);
        WriteSegment(out, codes, indices[2 * i], indices[2 * i + 1],
                     u->segments[i].target != nullptr ? done : nullptr);
    }
    free(indices);
}

static void WriteUnit(FILE* out, size_t number, const CatalogEntry* e, const CatalogEntry* done,
                      const char* locale)
{
    XliffPgsUnit pgs;
    int selection =
        XliffPgsPlan(&pgs, &e->item->value, done != nullptr ? &done->item->value : nullptr, locale);
    fprintf(out, "    <unit id=\"u%zu\" name=\"", number);
    XliffWriteText(out, e->key, e->length, 1);
    const ResMeta* most = CatalogFindMeta(e->item->meta, e->item->metaCount, "mloc-max-length");
    if (most != nullptr)
    {
        fprintf(out, "\" slr:sizeRestriction=\"%.*s", (int)most->value.length, most->value.bytes);
    }
    if (selection)
    {
        fputs("\" pgs:switch=\"", out);
        XliffWriteText(out, pgs.switchText, strlen(pgs.switchText), 1);
    }
    fputs("\">\n", out);
    WriteNotes(out, e, selection ? &pgs : nullptr);
    XliffCodes codes = {0};
    const char* state = StateOf(e, done);
    if (selection)
    {
        WritePgsSegments(out, &codes, &pgs, done, state);
        XliffPgsFree(&pgs);
    }
    else
    {
        size_t source = XliffCodesAdd(&codes, &e->item->value, xliffSource);
        size_t target = done != nullptr ? XliffCodesAdd(&codes, &done->item->value, source) : 0;
        XliffCodesWriteData(&codes, out);
        fprintf(out, "      <segment%s>\n", state);
        WriteSegment(out, &codes, source, target, done);
    }
    Line(out, 4, "</unit>\n");
    XliffCodesFree(&codes);
}

static int HasLimits(const CatalogInput* s)
{
    for (uint32_t i = 0; i < s->count; i++)
    {
        if (CatalogFindMeta(s->entries[i].item->meta, s->entries[i].item->metaCount,
                            "mloc-max-length") != nullptr)
        {
            return 1;
        }
    }
    return 0;
}

static void WriteFile(FILE* out, const char* source, const CatalogInput* s, const CatalogInput* t)
{
    const char* name = strrchr(source, '/');
    name = name != nullptr ? name + 1 : source;
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<xliff xmlns=\"urn:oasis:names:tc:xliff:document:2.2\"\n"
          "       xmlns:slr=\"urn:oasis:names:tc:xliff:sizerestriction:2.0\"\n"
          "       xmlns:pgs=\"urn:oasis:names:tc:xliff:pgs:1.0\"\n"
          "       version=\"2.2\" srcLang=\"",
          out);
    XliffWriteText(out, s->locale, strlen(s->locale), 1);
    fputs("\" trgLang=\"", out);
    XliffWriteText(out, t->locale, strlen(t->locale), 1);
    fputs("\">\n  <file id=\"f1\" original=\"", out);
    XliffWriteText(out, name, strlen(name), 1);
    fputs("\" canResegment=\"no\" xml:space=\"preserve\">\n", out);
    if (HasLimits(s))
    {
        Line(out, 4, "<slr:profiles generalProfile=\"xliff:codepoints\"/>\n");
    }
    size_t number = 0;
    for (uint32_t i = 0; i < s->count; i++)
    {
        if (!s->entries[i].keep)
        {
            WriteUnit(out, ++number, &s->entries[i], CatalogFindEntry(t, s->entries[i].key),
                      t->locale);
        }
    }
    fputs("  </file>\n</xliff>\n", out);
}

int CatalogExportXliff(const char* source, const char* translation, const char* xliff, FILE* report)
{
    char* paths[] = {(char*)source, (char*)translation};
    CatalogSet set;
    int ok = CatalogSetRead(&set, paths, 2, report);
    if (ok)
    {
        FILE* out = fopen(xliff, "wb");
        if (out != nullptr)
        {
            WriteFile(out, source, &set.inputs[0], &set.inputs[1]);
        }
        ok = out != nullptr && fclose(out) == 0;
        fprintf(report, ok ? "%s: written\n" : "%s: cannot write\n", xliff);
    }
    CatalogSetFree(&set);
    return ok;
}
