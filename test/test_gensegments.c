// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's segmentation units (segment_gen.h): a file of
// common/segments read into its unit, overrides into sets of the code
// table, suppressions into ICU4C's two tables, and the forms it refuses with their reasons: an
// escaped surrogate, Japanese's word rules or CJ as an ideograph with a part repeated and another
// missing, and a variable too long to read.

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
static SegmentGenSets s_sets;

// The reason a file's segmentations are refused, or nullptr with the unit
// in s_unit and its overrides in s_sets, which ReadAgain keeps.
static const char* ReadAgain(const char* segmentations)
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
    const char* problem = SegmentsUnit(root, &s_sets, &s_unit, &s_size);
    XmlFree(root);
    return problem;
}

static const char* Read(const char* segmentations)
{
    memset(&s_sets, 0, sizeof(s_sets));
    return ReadAgain(segmentations);
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
    CHECK(Gives(SENTENCE("<variable id=\"$STerm\">[[$STerm] [\\u037E ;]]</variable>"),
                segmentGivesSentence, 0) &&
              mlocLoad32(s_unit + 8) == 0 && mlocLoad32(s_unit + 12) == 1 &&
              s_size == segmentHeaderSize + 4 && s_sets.count == 1 &&
              s_sets.sets[0].given == segmentGivesSentence && s_sets.sets[0].count == 2 &&
              s_sets.sets[0].codePoints[0] == 0x3B && s_sets.sets[0].codePoints[1] == 0x37E &&
              strcmp(s_sets.sets[0].classes[1], "muni_sbSTerm") == 0,
          "a class's variable with characters added: a set of the code table, sorted");
    CHECK(
        Gives(WORD(HIRAGANA HIRAGANA_FE IDEOGRAPHIC IDEOGRAPHIC_FE, RULE_HIRAGANA RULE_IDEOGRAPHIC),
              segmentGivesWord, segmentJapaneseWords),
        "Japanese's word variables and rules");
    CHECK(Gives(LINE(CJ_ID CJ_NS), segmentGivesLine, segmentCjIdeographic),
          "the line variables of CJ as an ideograph");
    CHECK(Read("") == nullptr && s_unit == nullptr, "no unit for a file without segmentations");
}

// Whether the code table's source is refused with room up to a line's
// fifth byte (or to the source's last byte, at its end).
static bool Truncated(const char* source, size_t size, const char* line)
{
    char out[4096];
    size_t written = 1;
    size_t capacity = line == nullptr ? 0 : (size_t)(line - source) + 5;
    if (line == source + size - 1)
    {
        capacity = size;
    }
    if (line == nullptr || capacity > size || capacity > sizeof(out) ||
        SegmentOverridesText(&s_sets, out, capacity, &written) || written != 0)
    {
        printf("  not refused with room for %zu of %zu bytes\n", capacity, size);
        return false;
    }
    return true;
}

static void TestSets(void)
{
    CHECK(Read(SENTENCE("<variable id=\"$STerm\">[[$STerm] [;]]</variable>")) == nullptr &&
              ReadAgain(WORD("<variable id=\"$MidNum\">[[$MidNum] [.]]</variable>", "")) ==
                  nullptr &&
              mlocLoad32(s_unit + 8) == 2 &&
              ReadAgain(SENTENCE("<variable id=\"$STerm\">[[$STerm] [;]]</variable>")) == nullptr &&
              mlocLoad32(s_unit + 12) == 1 && s_sets.count == 2,
          "a set given again is the same set");
    char* source = nullptr;
    size_t size = 0;
    CHECK(
        SegmentOverridesSource(&s_sets, &source, &size) && strlen(source) == size &&
            strstr(source, "    {0x003B, muni_sbSTerm},\n    {0x002E, muni_wbMidNum},\n") !=
                nullptr &&
            strstr(source, "    {0, 1, mloc_segmentSentence},\n    {1, 1, mloc_segmentWord},\n") !=
                nullptr &&
            strstr(source, "mlocSegmentOverrideSetCount = 2;") != nullptr,
        "the code table's source: the overrides, then each set's first, count and type");
    if (source != nullptr)
    {
        CHECK(Truncated(source, size, strstr(source, "    {0x002E")) &&
                  Truncated(source, size, strstr(source, "    {1, 1,")) &&
                  Truncated(source, size, source + size - 1),
              "a source cut short in an override's line, a set's line or its last line is "
              "refused");
        char out[4096];
        size_t written = 0;
        CHECK(size < sizeof(out) && SegmentOverridesText(&s_sets, out, size + 1, &written) &&
                  written == size && memcmp(out, source, size + 1) == 0,
              "a source that fits exactly");
    }
    free(source);
}

// Whether the unit's entry is this text, with this value when backward.
static bool EntryIs(uint32_t i, const char* text, uint32_t value)
{
    uint32_t backward = mlocLoad32(s_unit + 16);
    uint32_t count = backward + mlocLoad32(s_unit + 20);
    const uint8_t* values = s_unit + segmentHeaderSize;
    const uint8_t* offsets = values + 4 * (size_t)backward;
    const uint8_t* texts = offsets + 4 * ((size_t)count + 1);
    uint32_t start = i < count ? mlocLoad32(offsets + 4 * (size_t)i) : 0;
    uint32_t end = i < count ? mlocLoad32(offsets + 4 * (size_t)i + 4) : 0;
    if (i >= count || end - start != strlen(text) ||
        memcmp(texts + start, text, end - start) != 0 ||
        (i < backward && mlocLoad32(values + 4 * (size_t)i) != value))
    {
        printf("  entry %u is not %s\n", i, text);
        return false;
    }
    return true;
}

static void TestSuppressions(void)
{
    // "Mr." is no family's: a match. "z.B.", "Ph.D." and "e.g." are
    // partial, so "z.", "Ph." and "e." are partials, and their families'
    // members, "z." among them, are forward entries.
    CHECK(Gives("<segmentation type=\"SentenceBreak\"><suppressions type=\"standard\">"
                "<suppression>Mr.</suppression><suppression>z.</suppression>"
                "<suppression>z.B.</suppression><suppression>Ph.D.</suppression>"
                "<suppression>e.g.</suppression><suppression>Mr.</suppression>"
                "</suppressions></segmentation>",
                segmentGivesSentence, 0) &&
              mlocLoad32(s_unit + 16) == 4 && mlocLoad32(s_unit + 20) == 4,
          "four backward and four forward entries, \"Mr.\" once");
    CHECK(EntryIs(0, "e.", segmentSuppressionPartial) &&
              EntryIs(1, "Ph.", segmentSuppressionPartial) &&
              EntryIs(2, "Mr.", segmentSuppressionMatch) &&
              EntryIs(3, "z.", segmentSuppressionPartial),
          "the backward entries, sorted read from the end");
    CHECK(EntryIs(4, "Ph.D.", 0) && EntryIs(5, "e.g.", 0) && EntryIs(6, "z.", 0) &&
              EntryIs(7, "z.B.", 0),
          "the forward entries, sorted");
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
    // 64 escaped characters, U+2000 to U+203F, of 7 bytes each: past the 255
    // bytes the generator reads.
    static const char s_open[] = "<variable id=\"$STerm\">[[$STerm] [";
    static const char s_close[] = "]]</variable>";
    char variables[sizeof(s_open) + 64 * 7 + sizeof(s_close)];
    memcpy(variables, s_open, sizeof(s_open) - 1);
    size_t n = sizeof(s_open) - 1;
    for (int i = 0; i < 64; i++, n += 7)
    {
        char escape[8];
        snprintf(escape, sizeof(escape), "\\u%04X ", 0x2000 + i);
        memcpy(variables + n, escape, 7);
    }
    memcpy(variables + n, s_close, sizeof(s_close));
    char file[4096];
    snprintf(file, sizeof(file), SENTENCE("%s"), variables);
    CHECK(Refused(file, "a variable or rule longer than the generator reads"),
          "a variable longer than the generator reads, not cut short");
}

int main(void)
{
    TestForms();
    TestSets();
    TestSuppressions();
    TestRefusals();
    free(s_unit);
    return s_failures == 0 ? 0 : 1;
}
