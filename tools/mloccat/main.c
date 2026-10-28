// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloccat, the catalog tool. Commands:
//   mloccat read FILE   writes the resource's data model as JSON, the
//                       draft's Resource<string> shape
//   mloccat check FILE  reads the resource and checks it as a catalog
//                       source: a locale, unique keys, messages that
//                       compile
//   mloccat compile DIRECTORY SOURCE [TRANSLATION...]
//                       compiles a source and its translations into
//                       catalogs, LOCALE.mlct each (compile.h)
//   mloccat accept SOURCE TRANSLATION [KEY...]
//                       records in the translation the source messages its
//                       entries answer, so that a later change shows them
//                       stale (accept.h)
//   mloccat export-po SOURCE TRANSLATION PO
//   mloccat import-po SOURCE TRANSLATION PO
//                       a translation to and from a PO file
//                       (exchange_po.h)
//   mloccat export-xliff SOURCE TRANSLATION XLIFF
//   mloccat import-xliff SOURCE TRANSLATION XLIFF
//                       a translation to and from XLIFF 2.2
//                       (exchange_xliff.h)
//   mloccat pseudo DIRECTORY SOURCE
//                       writes the pseudo-locales en-XA.mf2 and ar-XB.mf2
//                       from the source, to compile with its set (pseudo.h)
//   mloccat report [--json] [--fail-on KINDS] SOURCE [TRANSLATION...]
//                       the set's coverage and findings per locale
//                       (report.h); exits 1 when a finding of KINDS (a
//                       comma-separated list) is found

#include "accept.h"
#include "compile.h"
#include "exchange_po.h"
#include "exchange_xliff.h"
#include "file.h"
#include "json.h"
#include "pseudo.h"
#include "report.h"
#include "resource.h"
#include "resource_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void WriteString(const ResText* t)
{
    JsonString(stdout, t->bytes, t->length);
}

static void WriteId(const ResId* id)
{
    putchar('[');
    for (size_t i = 0; i < id->count; i++)
    {
        fputs(i > 0 ? "," : "", stdout);
        WriteString(&id->parts[i]);
    }
    putchar(']');
}

static void WriteMeta(const ResMeta* meta, size_t count)
{
    fputs("\"meta\":[", stdout);
    for (size_t i = 0; i < count; i++)
    {
        fputs(i > 0 ? ",{\"key\":" : "{\"key\":", stdout);
        WriteString(&meta[i].key);
        fputs(",\"value\":", stdout);
        WriteString(&meta[i].value);
        putchar('}');
    }
    putchar(']');
}

static void WriteItem(const ResItem* item)
{
    if (item->kind == resComment)
    {
        fputs("{\"type\":\"comment\",\"comment\":", stdout);
        WriteString(&item->comment);
        putchar('}');
        return;
    }
    fputs("{\"type\":\"entry\",\"comment\":", stdout);
    WriteString(&item->comment);
    putchar(',');
    WriteMeta(item->meta, item->metaCount);
    fputs(",\"id\":", stdout);
    WriteId(&item->id);
    fputs(",\"value\":", stdout);
    WriteString(&item->value);
    putchar('}');
}

static void WriteResource(const Resource* r)
{
    fputs("{\"comment\":", stdout);
    WriteString(&r->comment);
    putchar(',');
    WriteMeta(r->meta, r->metaCount);
    fputs(",\"sections\":[", stdout);
    for (size_t s = 0; s < r->sectionCount; s++)
    {
        const ResSection* section = &r->sections[s];
        fputs(s > 0 ? ",{\"comment\":" : "{\"comment\":", stdout);
        WriteString(&section->comment);
        putchar(',');
        WriteMeta(section->meta, section->metaCount);
        fputs(",\"id\":", stdout);
        WriteId(&section->id);
        fputs(",\"entries\":[", stdout);
        for (size_t i = 0; i < section->itemCount; i++)
        {
            fputs(i > 0 ? "," : "", stdout);
            WriteItem(&section->items[i]);
        }
        fputs("]}", stdout);
    }
    fputs("]}\n", stdout);
}

static int Run(const char* command, const char* path)
{
    size_t length = 0;
    char* text = FileRead(path, &length);
    if (text == nullptr)
    {
        fprintf(stderr, "mloccat: cannot read %s\n", path);
        return 2;
    }
    Resource resource;
    ResError error;
    int ok = ResourceRead(text, length, &resource, &error);
    if (!ok)
    {
        fprintf(stderr, "%s:%zu:%zu: %s\n", path, error.line, error.column, error.reason);
    }
    else if (strcmp(command, "read") == 0)
    {
        WriteResource(&resource);
    }
    else
    {
        ok = ResourceCheck(&resource, path, stderr);
    }
    ResourceFree(&resource);
    free(text);
    return ok ? 0 : 1;
}

// mloccat report: 0, 1 for a finding of the kinds to fail on, 2 for a
// set that does not read or a usage error.
static int RunReport(int argc, char** argv)
{
    int json = 0;
    unsigned failOn = 0;
    int at = 2;
    for (; at < argc && strncmp(argv[at], "--", 2) == 0; at++)
    {
        if (strcmp(argv[at], "--json") == 0)
        {
            json = 1;
            continue;
        }
        int kinds = strcmp(argv[at], "--fail-on") == 0 && at + 1 < argc;
        if (kinds)
        {
            at++;
        }
        if (!kinds || !ReportReadKinds(argv[at], &failOn))
        {
            fprintf(stderr, "mloccat report: unknown option or kinds: %s\n", argv[at]);
            return 2;
        }
    }
    CatalogSet set;
    Report report = {0};
    int ok = at < argc && CatalogSetRead(&set, argv + at, (size_t)(argc - at), stderr) &&
             ReportBuild(&set, &report);
    if (ok)
    {
        if (json)
        {
            ReportWriteJson(&report, stdout);
        }
        else
        {
            ReportWriteText(&report, stdout);
        }
    }
    unsigned found = ok ? ReportKindsFound(&report) : 0;
    ReportFree(&report);
    if (at < argc)
    {
        CatalogSetFree(&set);
    }
    return !ok ? 2 : (found & failOn) != 0 ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc == 5 && strcmp(argv[1], "export-po") == 0)
    {
        return CatalogExportPo(argv[2], argv[3], argv[4], stderr) ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "import-po") == 0)
    {
        return CatalogImportPo(argv[2], argv[3], argv[4], stderr) ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "export-xliff") == 0)
    {
        return CatalogExportXliff(argv[2], argv[3], argv[4], stderr) ? 0 : 1;
    }
    if (argc == 5 && strcmp(argv[1], "import-xliff") == 0)
    {
        return CatalogImportXliff(argv[2], argv[3], argv[4], stderr) ? 0 : 1;
    }
    if (argc == 4 && strcmp(argv[1], "pseudo") == 0)
    {
        return CatalogPseudo(argv[2], argv[3], stderr) ? 0 : 1;
    }
    if (argc >= 3 && strcmp(argv[1], "report") == 0)
    {
        return RunReport(argc, argv);
    }
    if (argc >= 4 && strcmp(argv[1], "compile") == 0)
    {
        return CatalogCompile(argv[2], argv + 3, (size_t)(argc - 3), stderr) ? 0 : 1;
    }
    if (argc >= 4 && strcmp(argv[1], "accept") == 0)
    {
        return CatalogAccept(argv[2], argv[3], argv + 4, (size_t)(argc - 4), stderr) ? 0 : 1;
    }
    if (argc != 3 || (strcmp(argv[1], "read") != 0 && strcmp(argv[1], "check") != 0))
    {
        fputs("usage: mloccat read FILE | mloccat check FILE\n"
              "       mloccat compile DIRECTORY SOURCE [TRANSLATION...]\n"
              "       mloccat accept SOURCE TRANSLATION [KEY...]\n"
              "       mloccat export-po SOURCE TRANSLATION PO\n"
              "       mloccat import-po SOURCE TRANSLATION PO\n"
              "       mloccat export-xliff SOURCE TRANSLATION XLIFF\n"
              "       mloccat import-xliff SOURCE TRANSLATION XLIFF\n"
              "       mloccat pseudo DIRECTORY SOURCE\n"
              "       mloccat report [--json] [--fail-on KINDS] SOURCE [TRANSLATION...]\n",
              stderr);
        return 2;
    }
    return Run(argv[1], argv[2]);
}
