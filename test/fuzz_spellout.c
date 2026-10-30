// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for spell-out formatting and parsing with CLDR's rules,
// the spell-out file beside the compiled-in data. The first byte chooses a
// locale of the spell-out file, the second a public rule set or a
// numbering system, the third the capitalization and what is done; the
// rest is a decimal's text, the bytes of an integer, or text to parse.
// Formatting refuses with capacity alone and writes the same twice;
// parsing refuses with syntax (reading nothing) or capacity alone, ends
// inside the text and reads the same twice, and the text a value is
// written as parses without fault.

#include "maul-locale/data.h"
#include "maul-locale/spellout.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxText = 4096,
};

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// The compiled-in data and the spell-out file; false when the file does
// not open.
static bool OpenData(mlocDataSet* set, const mlocData** rulesOut)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_rules[4 << 20];
    static int s_state;
    if (s_state == 0)
    {
        s_state = -1;
        mlocOpenBuiltinData(&s_sources[0]);
        FILE* file = fopen(MLOC_SPELLOUT_FILE, "rb");
        if (file != nullptr)
        {
            size_t size = fread(s_rules, 1, sizeof(s_rules), file);
            fclose(file);
            if (size < sizeof(s_rules) &&
                mlocOpenData(s_rules, size, &s_sources[1]) == mloc_success)
            {
                s_state = 1;
            }
        }
    }
    *set = (mlocDataSet){s_sources, 2};
    *rulesOut = &s_sources[1];
    return s_state == 1;
}

// A formatter of the chosen locale and rule set or numbering system.
static bool MakeFormatter(const mlocDataSet* set, const mlocData* rules, uint8_t locale,
                          uint8_t which, mlocCapitalization capitalization,
                          mlocSpelloutFormatter* out)
{
    static const char* const s_systems[] = {"roman", "romanlow", "hebr",     "armn",
                                            "geor",  "hans",     "jpanyear", "taml"};
    size_t count = mlocData_GetLocaleCount(rules);
    const char* id = nullptr;
    size_t length = 0;
    mlocLocale parsed;
    char tag[64];
    if (count == 0 || mlocData_GetLocale(rules, locale % count, &id, &length) != mloc_success ||
        length >= sizeof(tag))
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        tag[i] = id[i] == '_' ? '-' : id[i];
    }
    if (mlocParseLocale(tag, length, &parsed).status != mloc_success)
    {
        return false;
    }
    mlocSpelloutOptions options = {.capitalization = capitalization};
    if (which >= 248)
    {
        const char* system = s_systems[which - 248];
        return mlocInitNumberingSystemFormatter(set, &parsed, system, strlen(system), &options,
                                                out) >= 0;
    }
    size_t sets = mlocGetSpelloutRuleSetCount(set, &parsed);
    const char* name = nullptr;
    size_t nameLength = 0;
    if (sets == 0 ||
        mlocGetSpelloutRuleSet(set, &parsed, which % sets, &name, &nameLength) != mloc_success)
    {
        return false;
    }
    return mlocInitSpelloutFormatter(set, &parsed, name, nameLength, &options, out) >= 0;
}

// The memory every parse searches with.
static mlocSpelloutWorkspace s_workspace;

// Parses text twice: a refusal of syntax or a limit alone, the same
// reading both times, an end inside the text on success, 0 on a syntax
// error, and past a limit the end and the value left as they were.
static void CheckParse(const mlocSpelloutFormatter* formatter, const char* text, size_t length)
{
    mlocDecimal first;
    mlocDecimal second;
    size_t end = SIZE_MAX;
    size_t again = SIZE_MAX;
    memset(&first, 0xA5, sizeof(first));
    mlocDecimal kept = first;
    mlocResult r = mlocSpelloutFormatter_Parse(formatter, text, length, &s_workspace, &first, &end);
    Require(r == mloc_success || r == mloc_errorSyntax || r == mloc_errorLimit);
    Require(r == mloc_success       ? end > 0 && end <= length
            : r == mloc_errorSyntax ? end == 0
                                    : end == SIZE_MAX);
    Require(r == mloc_success || memcmp(&first, &kept, sizeof(first)) == 0);
    Require(mlocSpelloutFormatter_Parse(formatter, text, length, &s_workspace, &second, &again) ==
                r &&
            again == end);
    Require(r != mloc_success || memcmp(&first, &second, sizeof(first)) == 0);
}

// The decimal of the input: its text when it is one, else its first
// eight bytes as an integer over the power of ten the ninth chooses.
static mlocDecimal ValueOf(const uint8_t* bytes, size_t length)
{
    mlocDecimal decimal;
    if (mlocParseDecimal((const char*)bytes, length, &decimal).status == mloc_success)
    {
        return decimal;
    }
    int64_t n = 0;
    for (size_t i = 0; i < length && i < 8; i++)
    {
        n = (int64_t)((uint64_t)n << 8 | bytes[i]);
    }
    char text[48];
    int written =
        snprintf(text, sizeof(text), "%lldE-%d", (long long)n, length > 8 ? bytes[8] % 25 : 0);
    Require(written > 0 &&
            mlocParseDecimal(text, (size_t)written, &decimal).status == mloc_success);
    return decimal;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    mlocDataSet set;
    const mlocData* rules = nullptr;
    mlocSpelloutFormatter formatter;
    if (size < 3 || !OpenData(&set, &rules) ||
        !MakeFormatter(&set, rules, data[0], data[1], (mlocCapitalization)(data[2] & 3),
                       &formatter))
    {
        return 0;
    }
    const uint8_t* rest = data + 3;
    size_t length = size - 3;
    if ((data[2] & 4) != 0)
    {
        CheckParse(&formatter, (const char*)rest, length);
        return 0;
    }
    mlocDecimal decimal = ValueOf(rest, length);
    static char s_text[maxText];
    static char s_again[maxText];
    mlocOutput output = {.buffer = s_text, .capacity = sizeof(s_text)};
    mlocResult r = mlocSpelloutFormatter_Format(&formatter, &decimal, &output);
    Require(r == mloc_success || r == mloc_errorCapacity || r == mloc_errorLimit);
    if (r != mloc_success)
    {
        return 0;
    }
    mlocOutput repeat = {.buffer = s_again, .capacity = sizeof(s_again)};
    Require(mlocSpelloutFormatter_Format(&formatter, &decimal, &repeat) == mloc_success &&
            repeat.length == output.length && memcmp(s_text, s_again, output.length) == 0);
    mlocOutput measure = {0};
    Require(mlocSpelloutFormatter_Format(&formatter, &decimal, &measure) == mloc_errorCapacity &&
            measure.length == output.length);
    CheckParse(&formatter, s_text, output.length);
    return 0;
}
