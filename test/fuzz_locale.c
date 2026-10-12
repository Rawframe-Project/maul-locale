// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for locale ids. A refusal points inside the text; a
// parsed locale writes a tag that parses back to the same locale and
// writes the same tag again.

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

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
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
