// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the catalog tool's resource reader. Any bytes are
// read or refused with a line and column inside the text; a resource read
// is checked as a catalog source, written back and read again to the same
// data model.

// open_memstream is POSIX 2008.
#define _POSIX_C_SOURCE 200809L

#include "resource.h"
#include "resource_check.h"
#include "resource_write.h"

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

static bool SameText(const ResText* a, const ResText* b)
{
    return a->length == b->length && (a->length == 0 || memcmp(a->bytes, b->bytes, a->length) == 0);
}

static bool SameId(const ResId* a, const ResId* b)
{
    bool same = a->count == b->count;
    for (size_t i = 0; same && i < a->count; i++)
    {
        same = SameText(&a->parts[i], &b->parts[i]);
    }
    return same;
}

static bool SameMeta(const ResMeta* a, size_t aCount, const ResMeta* b, size_t bCount)
{
    bool same = aCount == bCount;
    for (size_t i = 0; same && i < aCount; i++)
    {
        same = SameText(&a[i].key, &b[i].key) && SameText(&a[i].value, &b[i].value);
    }
    return same;
}

static bool SameItem(const ResItem* a, const ResItem* b)
{
    return a->kind == b->kind && SameId(&a->id, &b->id) && SameText(&a->comment, &b->comment) &&
           SameMeta(a->meta, a->metaCount, b->meta, b->metaCount) && SameText(&a->value, &b->value);
}

static bool SameSection(const ResSection* a, const ResSection* b)
{
    bool same = SameId(&a->id, &b->id) && SameText(&a->comment, &b->comment) &&
                SameMeta(a->meta, a->metaCount, b->meta, b->metaCount) &&
                a->itemCount == b->itemCount;
    for (size_t i = 0; same && i < a->itemCount; i++)
    {
        same = SameItem(&a->items[i], &b->items[i]);
    }
    return same;
}

static bool Same(const Resource* a, const Resource* b)
{
    bool same = a->hasFrontmatter == b->hasFrontmatter && SameText(&a->comment, &b->comment) &&
                SameMeta(a->meta, a->metaCount, b->meta, b->metaCount) &&
                a->sectionCount == b->sectionCount;
    for (size_t s = 0; same && s < a->sectionCount; s++)
    {
        same = SameSection(&a->sections[s], &b->sections[s]);
    }
    return same;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Resource first;
    ResError error;
    if (!ResourceRead((const char*)data, size, &first, &error))
    {
        Require(error.reason != nullptr && error.line >= 1 && error.column >= 1);
        ResourceFree(&first);
        return 0;
    }
    FILE* sink = fopen("/dev/null", "w");
    if (sink != nullptr)
    {
        (void)ResourceCheck(&first, "fuzz", sink);
        fclose(sink);
    }
    char* text = nullptr;
    size_t length = 0;
    FILE* out = open_memstream(&text, &length);
    Require(out != nullptr);
    ResourceWrite(&first, out);
    fclose(out);
    Resource second;
    Require(ResourceRead(text, length, &second, &error));
    Require(Same(&first, &second));
    ResourceFree(&second);
    ResourceFree(&first);
    free(text);
    return 0;
}
