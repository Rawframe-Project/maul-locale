// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the catalog tool's PO reader, which reads the files
// translators send back. Any bytes are read or refused with a line and a
// reason; a file read is written back and read again to the same header
// and entries.

// open_memstream is POSIX 2008.
#define _POSIX_C_SOURCE 200809L

#include "po.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static bool SameText(const char* a, const char* b)
{
    return a == nullptr ? b == nullptr : b != nullptr && strcmp(a, b) == 0;
}

static bool SameEntry(const PoEntry* a, const PoEntry* b)
{
    return SameText(a->context, b->context) && SameText(a->id, b->id) &&
           SameText(a->string, b->string) && SameText(a->comments, b->comments) &&
           a->fuzzy == b->fuzzy && a->mf2 == b->mf2;
}

static bool Same(const PoFile* a, const PoFile* b)
{
    bool same = SameText(a->header, b->header) && a->count == b->count;
    for (size_t i = 0; same && i < a->count; i++)
    {
        same = SameEntry(&a->entries[i], &b->entries[i]);
    }
    return same;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    PoFile first = {0};
    PoError error = {0};
    if (!PoRead((const char*)data, size, &first, &error))
    {
        Require(error.reason != nullptr && error.line >= 1);
        PoFree(&first);
        return 0;
    }
    char* text = nullptr;
    size_t length = 0;
    FILE* out = open_memstream(&text, &length);
    Require(out != nullptr);
    PoWrite(&first, out);
    fclose(out);
    PoFile second = {0};
    Require(PoRead(text, length, &second, &error));
    Require(Same(&first, &second));
    PoFree(&second);
    PoFree(&first);
    free(text);
    return 0;
}
