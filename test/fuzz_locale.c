// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for locale ids. A refusal points inside the text; a
// parsed locale writes a tag that parses back to the same locale and
// writes the same tag again. Canonicalizing on the compiled-in data
// refuses within the text too, and a canonical tag canonicalizes to
// itself.

#include "maul-locale/locale.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// Canonicalizes the input; a result written and canonicalized again
// stays the same.
static void Canonicalize(const uint8_t* data, size_t size)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet set = {&builtin, 1};
    mlocLocale locale;
    mlocTextResult result = mlocCanonicalizeLocale(&set, (const char*)data, size, &locale, nullptr);
    if (result.status != mloc_success)
    {
        Require(result.status == mloc_errorSyntax || result.status == mloc_errorCapacity);
        Require(result.offset <= size);
        return;
    }
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    Require(mlocFormatLocale(&locale, &output) == mloc_success);
    mlocLocale again;
    mlocLocaleChanges changes = 0;
    Require(mlocCanonicalizeLocale(&set, tag, output.length, &again, &changes).status ==
            mloc_success);
    Require(memcmp(&locale, &again, sizeof(locale)) == 0 && changes == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Canonicalize(data, size);
    mlocLocale locale;
    mlocTextResult result = mlocParseLocale((const char*)data, size, &locale);
    if (result.status != mloc_success)
    {
        Require(result.status == mloc_errorSyntax || result.status == mloc_errorCapacity);
        Require(result.offset <= size);
        return 0;
    }
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    Require(mlocFormatLocale(&locale, &output) == mloc_success);
    mlocLocale again;
    Require(mlocParseLocale(tag, output.length, &again).status == mloc_success);
    Require(memcmp(&locale, &again, sizeof(locale)) == 0);
    char second[256];
    mlocOutput secondOutput = {.buffer = second, .capacity = sizeof(second)};
    Require(mlocFormatLocale(&again, &secondOutput) == mloc_success);
    Require(secondOutput.length == output.length && memcmp(tag, second, output.length) == 0);
    return 0;
}
