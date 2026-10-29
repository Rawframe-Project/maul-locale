// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A number formatted in words and read back within the smallest stack a
// program is likely to have: under Emscripten this suite keeps its
// default stack of 64 KB and checks every frame against it, so a parse
// that kept its search on the stack again would fail here.

#include "test_harness.h"

#include "maul-locale/spellout.h"

#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocSpelloutWorkspace s_workspace;

static void TestFive(void)
{
    mlocLocale en;
    mlocSpelloutFormatter formatter;
    mlocDecimal five;
    mlocDecimal back;
    char text[16];
    char value[16];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocOutput digits = {.buffer = value, .capacity = sizeof(value)};
    size_t end = 0;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitSpelloutFormatter(&s_data, &en, nullptr, 0, nullptr, &formatter) ==
                  mloc_success &&
              mlocParseDecimal("5", 1, &five).status == mloc_success &&
              mlocSpelloutFormatter_Format(&formatter, &five, &output) == mloc_success &&
              output.length == 4 && memcmp(text, "five", 4) == 0 &&
              mlocSpelloutFormatter_Parse(&formatter, text, output.length, &s_workspace, &back,
                                          &end) == mloc_success &&
              end == 4 && mlocFormatDecimal(&back, &digits) == mloc_success && digits.length == 1 &&
              value[0] == '5',
          "five, formatted and read back");
}

static bool OpenData(void)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_rules[4 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    FILE* file = fopen(MLOC_SPELLOUT_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(s_rules, 1, sizeof(s_rules), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_data = (mlocDataSet){s_sources, 2};
    return size > 0 && size < sizeof(s_rules) &&
           mlocOpenData(s_rules, size, &s_sources[1]) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_spellout_stack: cannot open " MLOC_SPELLOUT_FILE "\n", stderr);
        return 2;
    }
    TestFive();
    return s_failures == 0 ? 0 : 1;
}
