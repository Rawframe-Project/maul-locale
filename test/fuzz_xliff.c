// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the catalog tool's XLIFF import, which reads the
// files translators' tools send back: the XML reader, units, codes over
// original data, and Plural, Gender and Select segments folded into one
// message. The input is the XLIFF document, imported against a fixed
// source with plain, marked-up and selection messages and an Arabic
// translation, so every plural category has a case. A refusal reports
// why; an import leaves a translation that writes and reads back.

// open_memstream and mkdtemp are POSIX 2008.
#define _POSIX_C_SOURCE 200809L

#include "catalog_set.h"
#include "exchange_xliff.h"
#include "file.h"
#include "resource.h"
#include "resource_write.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int LLVMFuzzerInitialize(int* argc, char*** argv);
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static const char s_source[] =
    "@locale en\n---\n# A greeting.\n@param $name The user\n"
    "@mloc-max-length 30\nhello = Hi {$name} & {#b}you{/b} \\{x\\}\n"
    "bye = Bye\n@translate no\nbrand = Maul\n"
    "multi = .local $c = {$n :integer}\n .match $c\n one {{One {#i}file}}\n"
    " * {{{$n} files}}\n"
    "files = .input {$n :integer}\n .match $n\n 0 {{No files}}\n"
    " one {{One {#b}file{/b}}}\n * {{{$n} files}}\n"
    "place = .input {$p :number select=ordinal}\n .input {$g :string}\n .match $p $g\n"
    " one fem {{First, she}}\n * * {{{$p}th}}\n"
    "sel = .input {$s :string}\n .match $s\n 9 {{nine}}\n |a\\|b| {{pipe}}\n * {{other}}\n";

static const char s_translation[] = "@locale ar\n---\nbye = Ma'a salama\n";

static char s_directory[200];
static char s_paths[2][256];
static CatalogSet s_set;

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void ReadSet(void)
{
    char* paths[] = {s_paths[0], s_paths[1]};
    FILE* sink = fopen("/dev/null", "w");
    Require(sink != nullptr);
    bool read = CatalogSetRead(&s_set, paths, 2, sink);
    fclose(sink);
    Require(read);
}

// The catalogs' directory goes when the run ends.
static void Remove(void)
{
    remove(s_paths[0]);
    remove(s_paths[1]);
    rmdir(s_directory);
}

int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    (void)argc;
    (void)argv;
    const char* tmp = getenv("TMPDIR");
    snprintf(s_directory, sizeof(s_directory), "%s/fuzz_xliff_XXXXXX",
             tmp != nullptr && tmp[0] != '\0' ? tmp : "/tmp");
    Require(mkdtemp(s_directory) != nullptr);
    atexit(Remove);
    snprintf(s_paths[0], sizeof(s_paths[0]), "%s/en.mf2", s_directory);
    snprintf(s_paths[1], sizeof(s_paths[1]), "%s/ar.mf2", s_directory);
    Require(FileWrite(s_paths[0], s_source, strlen(s_source)));
    Require(FileWrite(s_paths[1], s_translation, strlen(s_translation)));
    ReadSet();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    char* said = nullptr;
    size_t saidLength = 0;
    FILE* report = open_memstream(&said, &saidLength);
    Require(report != nullptr);
    size_t count = 0;
    int ok = XliffImport(&s_set, (const char*)data, size, "fuzz.xlf", &count, report);
    fclose(report);
    if (!ok)
    {
        Require(saidLength > 0);
    }
    else if (count > 0)
    {
        char* text = nullptr;
        size_t length = 0;
        FILE* out = open_memstream(&text, &length);
        Require(out != nullptr);
        ResourceWrite(&s_set.inputs[1].resource, out);
        fclose(out);
        Resource again;
        ResError error;
        Require(ResourceRead(text, length, &again, &error));
        ResourceFree(&again);
        free(text);
        CatalogSetFree(&s_set);
        ReadSet();
    }
    free(said);
    return 0;
}
