// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog set's report, written as text or JSON (report.h).

#include "json.h"
#include "report.h"

#include <string.h>

static size_t CountOf(const LocaleReport* r, ReportKind kind)
{
    size_t count = 0;
    for (size_t i = 0; i < r->count; i++)
    {
        count += r->findings[i].kind == kind;
    }
    return count;
}

void ReportWriteText(const Report* report, FILE* out)
{
    fputs("locale      translated", out);
    for (int k = 0; k < reportKindCount; k++)
    {
        fprintf(out, " %10s", ReportKindName((ReportKind)k));
    }
    fputc('\n', out);
    for (size_t i = 0; i < report->count; i++)
    {
        const LocaleReport* r = &report->locales[i];
        char coverage[32];
        snprintf(coverage, sizeof(coverage), "%u/%u", r->translated, r->translatable);
        fprintf(out, "%-11s %10s", r->input->locale, coverage);
        for (int k = 0; k < reportKindCount; k++)
        {
            fprintf(out, " %10zu", CountOf(r, (ReportKind)k));
        }
        fputc('\n', out);
    }
    for (size_t i = 0; i < report->count; i++)
    {
        const LocaleReport* r = &report->locales[i];
        for (size_t f = 0; f < r->count; f++)
        {
            const Finding* finding = &r->findings[f];
            fprintf(out, "%s: %s %s", r->input->path, ReportKindName(finding->kind), finding->key);
            if (finding->kind == reportLong)
            {
                fprintf(out, " (%u, at most %u)", finding->length, finding->most);
            }
            fputc('\n', out);
        }
    }
}

static void WriteKind(const LocaleReport* r, ReportKind kind, FILE* out)
{
    fputs(",", out);
    JsonString(out, ReportKindName(kind), strlen(ReportKindName(kind)));
    fputs(":[", out);
    const char* separator = "";
    for (size_t i = 0; i < r->count; i++)
    {
        const Finding* f = &r->findings[i];
        if (f->kind != kind)
        {
            continue;
        }
        fputs(separator, out);
        separator = ",";
        if (kind == reportLong)
        {
            fputs("{\"key\":", out);
            JsonString(out, f->key, strlen(f->key));
            fprintf(out, ",\"length\":%u,\"most\":%u}", f->length, f->most);
        }
        else
        {
            JsonString(out, f->key, strlen(f->key));
        }
    }
    fputc(']', out);
}

void ReportWriteJson(const Report* report, FILE* out)
{
    const char* source = report->count > 0 ? report->locales[0].input->locale : "";
    fputs("{\"source\":", out);
    JsonString(out, source, strlen(source));
    fprintf(out, ",\"keys\":%u,\"locales\":[",
            report->count > 0 ? report->locales[0].input->count : 0U);
    for (size_t i = 0; i < report->count; i++)
    {
        const LocaleReport* r = &report->locales[i];
        fputs(i > 0 ? ",{\"locale\":" : "{\"locale\":", out);
        JsonString(out, r->input->locale, strlen(r->input->locale));
        fputs(",\"path\":", out);
        JsonString(out, r->input->path, strlen(r->input->path));
        fprintf(out, ",\"translated\":%u,\"translatable\":%u", r->translated, r->translatable);
        for (int k = 0; k < reportKindCount; k++)
        {
            WriteKind(r, (ReportKind)k, out);
        }
        fputc('}', out);
    }
    fputs("]}\n", out);
}
