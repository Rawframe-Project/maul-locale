// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's segmentation units (segment_gen.h): a file of
// common/segments read into its unit, and the forms it refuses with their
// reasons: an escaped surrogate, Japanese's word rules or CJ as an
// ideograph with a part repeated and another missing, and a variable too
// long to read.

#include "bytes.h"
#include "segment_gen.h"
#include "segment_tables.h"
#include "test_harness.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t* s_unit;
static size_t s_size;

// The reason a file's segmentations are refused, or nullptr with the unit
// in s_unit.
static const char* Read(const char* segmentations)
{
    char xml[4096];
    snprintf(xml, sizeof(xml), "<ldml><segmentations>%s</segmentations></ldml>", segmentations);
    char error[256] = "";
    XmlNode* root = XmlParse(xml, strlen(xml), error, sizeof(error));
    free(s_unit);
    s_unit = nullptr;
    s_size = 0;
    if (root == nullptr)
    {
        printf("  cannot parse: %s\n", error);
        return "cannot parse";
    }
    const char* problem = SegmentsUnit(root, &s_unit, &s_size);
    XmlFree(root);
    return problem;
}

// Whether a file is refused for this reason.
static bool Refused(const char* segmentations, const char* reason)
{
    const char* problem = Read(segmentations);
    if (problem == nullptr || strcmp(problem, reason) != 0)
    {
        printf("  refused for \"%s\", want \"%s\"\n", problem != nullptr ? problem : "nothing",
               reason);
        return false;
    }
    return true;
}

// Whether a file is read into a unit with these types and flags.
static bool Gives(const char* segmentations, uint32_t given, uint32_t flags)
{
    const char* problem = Read(segmentations);
    if (problem != nullptr || s_unit == nullptr || s_size < segmentHeaderSize)
    {
        printf("  refused for \"%s\"\n", problem != nullptr ? problem : "nothing, but no unit");
        return false;
    }
    return mlocLoad32(s_unit) == given && mlocLoad32(s_unit + 4) == flags;
}

#define SENTENCE(variables)                                                                        \
    "<segmentation type=\"SentenceBreak\"><variables>" variables "</variables></segmentation>"
#define WORD(variables, rules)                                                                     \
    "<segmentation type=\"WordBreak\"><variables>" variables "</variables><segmentRules>" rules    \
    "</segmentRules></segmentation>"
#define LINE(variables)                                                                            \
    "<segmentation type=\"LineBreak\"><variables>" variables "</variables></segmentation>"

#define HIRAGANA    "<variable id=\"$Hiragana\">\\p{Hiragana}</variable>"
#define HIRAGANA_FE "<variable id=\"$Hiragana\">($Hiragana $FE*)</variable>"
#define IDEOGRAPHIC                                                                                \
    "<variable id=\"$Ideographic\">[[\\p{Ideographic}] [\\u3005 \\u3007 \\u303B]]</variable>"
#define IDEOGRAPHIC_FE   "<variable id=\"$Ideographic\">($Ideographic $FE*)</variable>"
#define RULE_HIRAGANA    "<rule id=\"13.3\">$Hiragana × $Hiragana</rule>"
#define RULE_IDEOGRAPHIC "<rule id=\"13.4\">$Ideographic × $Ideographic</rule>"
#define CJ_ID            "<variable id=\"$ID\">[[\\p{Line_Break=Ideographic}] [$CJ]]</variable>"
#define CJ_NS            "<variable id=\"$NS\">\\p{Line_Break=Nonstarter}</variable>"

static void TestForms(void)
{
    CHECK(Gives(SENTENCE("<variable id=\"$STerm\">[[$STerm] [; \\u037E]]</variable>"),
                segmentGivesSentence, 0) &&
              mlocLoad32(s_unit + 12) == 2 && mlocLoad32(s_unit + segmentHeaderSize) == 0x3B &&
              mlocLoad32(s_unit + segmentHeaderSize + 8) == 0x37E,
          "a class's variable with characters added");
    CHECK(
        Gives(WORD(HIRAGANA HIRAGANA_FE IDEOGRAPHIC IDEOGRAPHIC_FE, RULE_HIRAGANA RULE_IDEOGRAPHIC),
              segmentGivesWord, segmentJapaneseWords),
        "Japanese's word variables and rules");
    CHECK(Gives(LINE(CJ_ID CJ_NS), segmentGivesLine, segmentCjIdeographic),
          "the line variables of CJ as an ideograph");
    CHECK(Read("") == nullptr && s_unit == nullptr, "no unit for a file without segmentations");
}

static void TestRefusals(void)
{
    const char* variable = "a variable the generator does not take";
    CHECK(Refused(SENTENCE("<variable id=\"$STerm\">[[$STerm] [\\uD800]]</variable>"), variable) &&
              Refused(SENTENCE("<variable id=\"$STerm\">[[$STerm] [\\x{DFFF}]]</variable>"),
                      variable) &&
              Refused(SENTENCE("<variable id=\"$STerm\">[[$STerm] [\\U0000DBFF]]</variable>"),
                      variable),
          "an escaped surrogate");
    CHECK(
        Refused(WORD(HIRAGANA HIRAGANA HIRAGANA_FE IDEOGRAPHIC_FE, RULE_HIRAGANA RULE_IDEOGRAPHIC),
                "Japanese's word rules in part") &&
            Refused(
                WORD(HIRAGANA HIRAGANA_FE IDEOGRAPHIC IDEOGRAPHIC_FE, RULE_HIRAGANA RULE_HIRAGANA),
                "Japanese's word rules in part"),
        "Japanese's six parts with one repeated and another missing");
    CHECK(Refused(LINE(CJ_ID CJ_ID), "CJ as an ideograph in part"),
          "CJ as an ideograph's two variables with one repeated");
    char variables[2048];
    int n = snprintf(variables, sizeof(variables), "%s", "<variable id=\"$STerm\">[[$STerm] [");
    for (int i = 0; i < 64; i++)
    {
        n += snprintf(variables + n, sizeof(variables) - (size_t)n, "\\u%04X ", 0x2000 + i);
    }
    snprintf(variables + n, sizeof(variables) - (size_t)n, "%s", "]]</variable>");
    char file[4096];
    snprintf(file, sizeof(file), SENTENCE("%s"), variables);
    CHECK(Refused(file, "a variable or rule longer than the generator reads"),
          "a variable longer than the generator reads, not cut short");
}

int main(void)
{
    TestForms();
    TestRefusals();
    free(s_unit);
    return s_failures == 0 ? 0 : 1;
}
