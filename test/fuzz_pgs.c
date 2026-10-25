// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the catalog tool's reading of selection messages as
// XLIFF's Plural, Gender and Select module sees them. Any bytes are read or
// refused; a message read is written again from its head, keys and
// patterns, and that text reads to the same selectors and variants.

#include "pgs.h"

#include <stdint.h>
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

static bool Same(const PgsMessage* a, const PgsMessage* b)
{
    bool same = strcmp(a->head, b->head) == 0 && a->selectorCount == b->selectorCount &&
                a->variantCount == b->variantCount;
    for (size_t s = 0; same && s < a->selectorCount; s++)
    {
        same = a->selectors[s].kind == b->selectors[s].kind &&
               strcmp(a->selectors[s].name, b->selectors[s].name) == 0;
    }
    for (size_t v = 0; same && v < a->variantCount; v++)
    {
        const PgsVariant* x = &a->variants[v];
        const PgsVariant* y = &b->variants[v];
        same = strcmp(x->pattern, y->pattern) == 0;
        for (size_t s = 0; same && s < a->selectorCount; s++)
        {
            same = strcmp(x->keys[s], y->keys[s]) == 0 && strcmp(x->cases[s], y->cases[s]) == 0;
        }
    }
    return same;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    PgsMessage first = {0};
    if (!PgsRead((const char*)data, size, &first))
    {
        return 0;
    }
    Require(first.selectorCount >= 1 && first.selectorCount <= pgsMaxSelectors &&
            first.variantCount >= 1);
    char* const** keys = malloc(first.variantCount * sizeof(*keys));
    char** patterns = malloc(first.variantCount * sizeof(*patterns));
    Require(keys != nullptr && patterns != nullptr);
    for (size_t v = 0; v < first.variantCount; v++)
    {
        keys[v] = first.variants[v].keys;
        patterns[v] = first.variants[v].pattern;
    }
    size_t length = 0;
    char* text = PgsWrite(first.head, (char* const* const*)keys, patterns, first.variantCount,
                          first.selectorCount, &length);
    Require(text != nullptr);
    PgsMessage second = {0};
    Require(PgsRead(text, length, &second));
    Require(Same(&first, &second));
    PgsFree(&second);
    free(text);
    free(patterns);
    free((void*)keys);
    PgsFree(&first);
    return 0;
}
