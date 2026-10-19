// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloccat, the catalog tool (record mloc-0053). Commands:
//   mloccat read FILE   writes the resource's data model as JSON, the
//                       draft's Resource<string> shape
//   mloccat check FILE  reads the resource and checks it as a catalog
//                       source: a locale, unique keys, messages that
//                       compile
//   mloccat compile DIRECTORY SOURCE [TRANSLATION...]
//                       compiles a source and its translations into
//                       catalogs, LOCALE.mloc each (compile.h)
//   mloccat accept SOURCE TRANSLATION [KEY...]
//                       records in the translation the source messages its
//                       entries answer, so that a later change shows them
//                       stale (accept.h)

#include "accept.h"
#include "compile.h"
#include "file.h"
#include "resource.h"
#include "resource_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void WriteString(const ResText* t)
{
    putchar('"');
    for (size_t i = 0; i < t->length; i++)
    {
        unsigned char c = (unsigned char)t->bytes[i];
        if (c == '"' || c == '\\')
        {
            printf("\\%c", c);
        }
        else if (c == '\n')
        {
            fputs("\\n", stdout);
        }
        else if (c < 0x20 || c == 0x7F)
        {
            printf("\\u%04x", c);
        }
        else
        {
            putchar(c);
        }
    }
    putchar('"');
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

int main(int argc, char** argv)
{
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
              "       mloccat accept SOURCE TRANSLATION [KEY...]\n",
              stderr);
        return 2;
    }
    return Run(argv[1], argv[2]);
}
