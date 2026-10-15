// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mlocgen, the generator of Maul Locale's data. Commands:
//
//   mlocgen resolve <CLDR common directory>
//       Reads lines "locale<TAB>chain" from standard input and writes, for
//       each, the resolved value (its text, then its value attributes as
//       "@name=value", tab-separated), "<none>" when nothing resolves, or
//       "<empty>" for an explicitly empty value. The resolution checks
//       compare this output with CLDR's JSON.
//
//   mlocgen generate <CLDR common directory> <repository root> <tzdb directory>
//       Writes the library's generated sources under src/generated/, the
//       zones from the pinned tzdb release's source (record mloc-0041).
//
//   mlocgen zones <tzdb directory> <until year>
//       Prints each zone's compiled offsets up to the start of a year
//       (zone_gen.h), for comparing the compiler with zic and zdump.
//
//   mlocgen chains <CLDR common directory>
//       Reads lines "main<TAB>id" or "collation<TAB>id" and writes, for
//       each, the line, a tab and the keys of the locale's CLDR parents up
//       to root (record mloc-0016); conform_parents compares them with
//       the library's.

#include "cldr.h"
#include "generate.h"
#include "parents_gen.h"
#include "plural_gen.h"
#include "zone_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const s_emptyMarker = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";

static int Resolve(const char* commonDirectory)
{
    Cldr cldr;
    char error[1024];
    if (!CldrOpen(&cldr, commonDirectory, error, sizeof(error)))
    {
        fprintf(stderr, "mlocgen: %s\n", error);
        return 1;
    }
    char line[8192];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char* tab = strchr(line, '\t');
        if (tab == nullptr)
        {
            fprintf(stderr, "mlocgen: a line is not \"locale<TAB>chain\": %s\n", line);
            CldrClose(&cldr);
            return 1;
        }
        *tab = '\0';
        const Leaf* leaf = CldrResolve(&cldr, line, tab + 1, componentMain);
        if (leaf == nullptr)
        {
            puts("<none>");
            continue;
        }
        fputs(strcmp(leaf->text, s_emptyMarker) == 0 ? "<empty>" : leaf->text, stdout);
        for (int i = 0; i < leaf->valueCount; i++)
        {
            printf("\t@%s=%s", leaf->valueNames[i], leaf->values[i]);
        }
        putchar('\n');
    }
    CldrClose(&cldr);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 3 && strcmp(argv[1], "resolve") == 0)
    {
        return Resolve(argv[2]);
    }
    if (argc == 5 && strcmp(argv[1], "generate") == 0)
    {
        char error[1024];
        if (!Generate(argv[2], argv[3], argv[4], error, sizeof(error)))
        {
            fprintf(stderr, "mlocgen: %s\n", error);
            return 1;
        }
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "chains") == 0)
    {
        char error[1024];
        if (!PrintChains(argv[2], error, sizeof(error)))
        {
            fprintf(stderr, "mlocgen: %s\n", error);
            return 1;
        }
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "zones") == 0)
    {
        char error[1024];
        if (!PrintZones(argv[2], (int32_t)atoi(argv[3]), error, sizeof(error)))
        {
            fprintf(stderr, "mlocgen: %s\n", error);
            return 1;
        }
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "plural-samples") == 0)
    {
        char error[1024];
        if (!PrintPluralSamples(argv[2], error, sizeof(error)))
        {
            fprintf(stderr, "mlocgen: %s\n", error);
            return 1;
        }
        return 0;
    }
    fputs("usage: mlocgen resolve <CLDR common directory>\n"
          "       mlocgen chains <CLDR common directory>\n"
          "       mlocgen plural-samples <CLDR common directory>\n"
          "       mlocgen zones <tzdb directory> <until year>\n"
          "       mlocgen generate <CLDR common directory> <repository root> <tzdb directory>\n",
          stderr);
    return 2;
}
