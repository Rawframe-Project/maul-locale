// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog set's report: each kind of finding, the coverage, a long
// message measured with its examples in its locale, the text and JSON
// forms, and the kinds read for --fail-on. Run with a directory to write
// into.

#include "accept.h"
#include "catalog_set.h"
#include "file.h"
#include "report.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_directory;

static void Write(const char* name, const char* text, char* path, size_t capacity)
{
    snprintf(path, capacity, "%s/%s", s_directory, name);
    (void)FileWrite(path, text, strlen(text));
}

static bool StartsWith(const char* text, const char* prefix)
{
    return text != nullptr && strncmp(text, prefix, strlen(prefix)) == 0;
}

static bool Has(const LocaleReport* r, ReportKind kind, const char* key, uint32_t length)
{
    for (size_t i = 0; i < r->count; i++)
    {
        const Finding* f = &r->findings[i];
        if (f->kind == kind && strcmp(f->key, key) == 0 &&
            (kind != reportLong || f->length == length))
        {
            return true;
        }
    }
    return false;
}

// Writes a report to a string; the caller frees it.
static char* Written(const Report* report, bool json)
{
    FILE* f = tmpfile();
    char* text = calloc(8192, 1);
    if (f == nullptr || text == nullptr)
    {
        if (f != nullptr)
        {
            fclose(f);
        }
        return text;
    }
    if (json)
    {
        ReportWriteJson(report, f);
    }
    else
    {
        ReportWriteText(report, f);
    }
    rewind(f);
    size_t read = fread(text, 1, 8191, f);
    text[read] = '\0';
    fclose(f);
    return text;
}

static void TestFindings(void)
{
    char en[1024];
    char tr[1024];
    char fi[1024];
    Write("en.mf2",
          "@locale en\n---\n@mloc-max-length 12\n@mloc-example $name=Alexandra Smith $n=3\n"
          "hello = Hello {$name}\nbye = Bye\nnew = New\n@translate no\nbrand = Maul\n"
          "@translate no\nsame = Same\n@mloc-max-length 30\nshort = Hi\n"
          "@translate no\n@mloc-max-length 9\n@mloc-example $flag $n=3\nexact = {$flag}{$n} "
          "items!!\n",
          en, sizeof(en));
    Write("tr.mf2",
          "@locale tr\n---\nhello = Merhaba {$name}\nbye = Ho\xC5\x9F\xC3\xA7\x61 kal\n"
          "brand = Moul\nsame = Same\nshort = Selam\nextra = Fazla\n",
          tr, sizeof(tr));
    Write("fi.mf2", "@locale fi\n---\nhello = Hei {$name}\n", fi, sizeof(fi));
    FILE* quiet = tmpfile();
    FILE* to = quiet != nullptr ? quiet : stderr;
    char* bye[] = {"bye"};
    char* shortKey[] = {"short"};
    bool accepted = CatalogAccept(en, tr, bye, 1, to) && CatalogAccept(en, tr, shortKey, 1, to);
    // The source changes after its translation was accepted.
    Write("en.mf2",
          "@locale en\n---\n@mloc-max-length 12\n@mloc-example $name=Alexandra Smith $n=3\n"
          "hello = Hello {$name}\nbye = Goodbye\nnew = New\n@translate no\nbrand = Maul\n"
          "@translate no\nsame = Same\n@mloc-max-length 30\nshort = Hi\n"
          "@translate no\n@mloc-max-length 9\n@mloc-example $flag $n=3\nexact = {$flag}{$n} "
          "items!!\n",
          en, sizeof(en));
    char* paths[] = {en, tr, fi};
    CatalogSet set;
    Report report = {0};
    bool built = CatalogSetRead(&set, paths, 3, to) && ReportBuild(&set, &report);
    if (quiet != nullptr)
    {
        fclose(quiet);
    }
    if (!accepted || !built || report.count != 3)
    {
        CHECK(false, "the set reads and reports");
        ReportFree(&report);
        CatalogSetFree(&set);
        return;
    }
    const LocaleReport* s = &report.locales[0];
    const LocaleReport* t = &report.locales[1];
    const LocaleReport* f = &report.locales[2];
    CHECK(s->count == 1 && Has(s, reportLong, "hello", 21) && s->translated == 7,
          "the source: its own long message, measured with the example; one at its limit, measured "
          "with its second example after one with no value, is not");
    CHECK(t->translatable == 4 && t->translated == 3 && t->count == 6 &&
              Has(t, reportMissing, "new", 0) && Has(t, reportStale, "bye", 0) &&
              Has(t, reportUnrecorded, "hello", 0) && Has(t, reportOrphaned, "extra", 0) &&
              Has(t, reportDiffers, "brand", 0) && Has(t, reportLong, "hello", 23),
          "a translation: each kind once, the kept key equal to the source not reported, "
          "the recorded and unchanged not stale");
    CHECK(f->translated == 1 && f->count == 5 && Has(f, reportMissing, "bye", 0) &&
              Has(f, reportMissing, "short", 0) && Has(f, reportUnrecorded, "hello", 0) &&
              Has(f, reportLong, "hello", 19),
          "another locale's count and length");
    CHECK(ReportKindsFound(&report) == 0x3FU, "every kind found");
    char* text = Written(&report, false);
    CHECK(StartsWith(text, "locale      translated    missing      stale unrecorded   "
                           "orphaned    differs       long\n"),
          "the text's head: a column per kind");
    CHECK(text != nullptr &&
              strstr(text, "tr                 3/4          1          1          1"
                           "          1          1          1\n") != nullptr &&
              strstr(text, "tr.mf2: long hello (23, at most 12)\n") != nullptr &&
              strstr(text, "tr.mf2: orphaned extra\n") != nullptr,
          "the text: the table and a line per finding");
    free(text);
    char* json = Written(&report, true);
    CHECK(StartsWith(json, "{\"source\":\"en\",\"keys\":7,\"locales\":[{\"locale\":\"en\"") &&
              strstr(json,
                     "\"translated\":3,\"translatable\":4,\"missing\":[\"new\"],"
                     "\"stale\":[\"bye\"],\"unrecorded\":[\"hello\"],\"orphaned\":[\"extra\"],"
                     "\"differs\":[\"brand\"],\"long\":[{\"key\":\"hello\",\"length\":23,"
                     "\"most\":12}]}") != nullptr,
          "the JSON: each kind's keys, a long message's length");
    free(json);
    ReportFree(&report);
    CatalogSetFree(&set);
}

static void TestKinds(void)
{
    unsigned kinds = 0;
    CHECK(ReportReadKinds("stale,long", &kinds) && kinds == (1U << reportStale | 1U << reportLong),
          "a list of kinds");
    CHECK(ReportReadKinds("missing,stale,unrecorded,orphaned,differs,long", &kinds) &&
              kinds == 0x3FU && ReportReadKinds("", &kinds) && kinds == 0,
          "every kind; none");
    CHECK(!ReportReadKinds("stale,bogus", &kinds) && !ReportReadKinds("Stale", &kinds) &&
              !ReportReadKinds("stal", &kinds),
          "an unknown name, a capital, a prefix refused");
    CHECK(strcmp(ReportKindName(reportDiffers), "differs") == 0 &&
              strcmp(ReportKindName(reportKindCount), "") == 0,
          "kind names");
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    TestFindings();
    TestKinds();
    return s_failures == 0 ? 0 : 1;
}
