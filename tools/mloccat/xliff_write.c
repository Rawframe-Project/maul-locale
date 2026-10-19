// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// XLIFF 2.2 export (exchange_xliff.h).

#include "catalog_set.h"
#include "exchange_xliff.h"
#include "mf2_parts.h"
#include "stale.h"
#include "xliff_codes.h"

#include <stdlib.h>
#include <string.h>

static void Line(FILE* out, int indent, const char* text)
{
    fprintf(out, "%*s%s", indent, "", text);
}

static void WriteNotes(FILE* out, const CatalogEntry* e)
{
    int any = e->item->comment.length > 0 ||
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
    Line(out, 6, "</notes>\n");
}

static void WriteUnit(FILE* out, size_t number, const CatalogEntry* e, const CatalogEntry* done)
{
    XliffCodes codes = {0};
    XliffCodesAdd(&codes, &e->item->value, 0);
    if (done != nullptr)
    {
        XliffCodesAdd(&codes, &done->item->value, 1);
    }
    fprintf(out, "    <unit id=\"u%zu\" name=\"", number);
    XliffWriteText(out, e->key, e->length, 1);
    const ResMeta* most = CatalogFindMeta(e->item->meta, e->item->metaCount, "mloc-max-length");
    if (most != nullptr)
    {
        fprintf(out, "\" slr:sizeRestriction=\"%.*s", (int)most->value.length, most->value.bytes);
    }
    fputs("\">\n", out);
    WriteNotes(out, e);
    XliffCodesWriteData(&codes, out);
    int stale = done != nullptr && SourceStateOf(done->item->meta, done->item->metaCount,
                                                 &e->item->value) == sourceStale;
    Line(out, 6,
         done == nullptr ? "<segment>\n"
         : stale         ? "<segment state=\"initial\">\n"
                         : "<segment state=\"translated\">\n");
    Line(out, 8, "<source>");
    XliffCodesWriteMessage(&codes, 0, out);
    fputs("</source>\n", out);
    if (done != nullptr)
    {
        Line(out, 8, "<target>");
        XliffCodesWriteMessage(&codes, 1, out);
        fputs("</target>\n", out);
    }
    Line(out, 6, "</segment>\n");
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
            WriteUnit(out, ++number, &s->entries[i], CatalogFindEntry(t, s->entries[i].key));
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
