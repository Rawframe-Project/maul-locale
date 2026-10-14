// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out rule compiler and its units (record mloc-0034): rules
// compiled as ICU4C 79 reads them, each case's expected text being
// ICU's getRules for the same rules (a rule set without a name, implicit
// bases, brackets with "|", the special rules, fraction rule sets,
// plural blocks, apostrophes, patterns, ">>>"); the rules ICU refuses,
// and the one the library refuses that ICU takes (a fraction rule set
// read before a rule names it); and the unit the generator writes
// opening, every corruption of it refused; and the engine's limits on
// rules that nest without end.

#include "bytes.h"
#include "data_format.h"
#include "datafile.h"
#include "kinds.h"
#include "locale_context.h"
#include "spellout_compile.h"
#include "spellout_data.h"
#include "spellout_gen.h"
#include "test_harness.h"

#include "maul-locale/spellout.h"

#include <stdlib.h>
#include <string.h>

typedef struct Case
{
    const char* rules;
    const char* expected;
} Case;

static const Case s_cases[] = {
    {"%a:\n"
     "0: zero;\n"
     "one;\n"
     "two;\n"
     "10: ten;\n"
     "1,000: thousand;\n",
     "%a:\n"
     "0: zero;\n"
     "1: one;\n"
     "2: two;\n"
     "10: ten;\n"
     "1000: thousand;\n"},
    {"%a:\n"
     "0: x;\n"
     "80/20: quatre-vingt[->>|s];\n"
     "100: cent[ >>];\n"
     "1000>: k >>;\n",
     "%a:\n"
     "0: x;\n"
     "80/20: quatre-vingts;\n"
     "81/20: quatre-vingt->%a>;\n"
     "100: cent;\n"
     "101: cent >%a>;\n"
     "1000>: k >%a>;\n"},
    {"%a:\n"
     "0: z;\n"
     "1: o;\n"
     "x.x: [<< ]point >>;\n"
     "x,x: << comma >>;\n"
     "x.0: [a]b;\n"
     "Inf: inf;\n"
     "NaN: nan;\n"
     "NaN: nan2;\n"
     "-x: minus >>;\n",
     "%a:\n"
     "0: z;\n"
     "1: o;\n"
     "-x: minus >%a>;\n"
     "x.x: <%a< point >%a>;\n"
     "x,x: <%a< comma >%a>;\n"
     "x.x: ab;\n"
     "0.x: point >%a>;\n"
     "x.0: b;\n"
     "Inf: inf;\n"
     "NaN: nan2;\n"},
    {"%a:\n"
     "0: z;\n"
     "1: o;\n"
     "2: t;\n"
     "x.x: << and >%%f>;\n"
     "0.x: >%%f>;\n"
     "%%f:\n"
     "10: <%a< tenths;\n"
     "100: <%a<< hundredths;\n",
     "%a:\n"
     "0: z;\n"
     "1: o;\n"
     "2: t;\n"
     "x.x: <%a< and >%%f>;\n"
     "0.x: >%%f>;\n"
     "%%f:\n"
     "10: <%a< tenths;\n"
     "100: <%a< hundredths;\n"},
    {"%a:\n"
     "0: z;\n"
     "1: o;\n"
     "1000: << $(cardinal,one{thousand}other{thousands})$[ >>];\n"
     "%b:\n"
     "0: $(ordinal, one{st} two{nd} few{rd} other{th})$ =#,##0=;\n",
     "%a:\n"
     "0: z;\n"
     "1: o;\n"
     "1000: <%a< $(cardinal,one{thousand}other{thousands})$;\n"
     "1001: <%a< $(cardinal,one{thousand}other{thousands})$ >%a>;\n"
     "%b:\n"
     "0: $(ordinal, one{st} two{nd} few{rd} other{th})$ =#,##0=;\n"},
    {"%a:\n"
     "0: z;\n"
     "1: ' one;\n"
     "2:   two;\n"
     "100: <<' hundred;\n",
     "%a:\n"
     "0: z;\n"
     "1: ' one;\n"
     "2: two;\n"
     "100: <%a<' hundred;\n"},
    {"0: zero;\n"
     "1: one;\n",
     "%default:\n"
     "0: zero;\n"
     "1: one;\n"},
    {"%%p:\n"
     "0: =%x=;\n"
     "%x:\n"
     "0: x;\n"
     "%y:\n"
     "0: y;\n"
     "%%q:\n"
     "0: q;\n",
     "%%p:\n"
     "0: =%x=;\n"
     "%x:\n"
     "0: x;\n"
     "%y:\n"
     "0: y;\n"
     "%%q:\n"
     "0: q;\n"},
    {"%x@noparse:\n"
     "0: x;\n"
     "%spellout-numbering:\n"
     "0: =%x=;\n"
     "%z:\n"
     "0: z;\n",
     "%x:\n"
     "0: x;\n"
     "%spellout-numbering:\n"
     "0: =%x=;\n"
     "%z:\n"
     "0: z;\n"},
    {"%a:\n"
     "0: =#,##0=;\n"
     "10: >#,##,##0.0> <0<;\n"
     "100: =0.#=;\n"
     "1000: =#,##0.=;\n",
     "%a:\n"
     "0: =#,##0=;\n"
     "10: >#,##,##0.0> <0<;\n"
     "100: =0.#=;\n"
     "1000: =#,##0.=;\n"},
    {"%a:\n"
     "0: z;\n"
     "10: a >>> b;\n"
     "20: c >>>;\n",
     "%a:\n"
     "0: z;\n"
     "10: a >>> b;\n"
     "20: c >>>;\n"},
};

static void TestCompile(void)
{
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        RbnfGroup group;
        char error[256] = "";
        bool compiled =
            RbnfCompile(s_cases[i].rules, strlen(s_cases[i].rules), &group, error, sizeof(error));
        char* dump = compiled ? RbnfDump(&group) : nullptr;
        bool same = dump != nullptr && strcmp(dump, s_cases[i].expected) == 0;
        if (!same)
        {
            printf("case %zu: %s\n%s", i, error, dump != nullptr ? dump : "");
        }
        CHECK(same, "rules compile as ICU reads them");
        free(dump);
        if (compiled)
        {
            RbnfFree(&group);
        }
    }
}

static const char* const s_refused[] = {
    // "==", and a token that names nothing a substitution takes ("=b=",
    // "< b <": ICU starts a token at any '<', '>' or '=').
    "%a:\n0: ==;\n",
    "%a:\n0: a=b=c;\n",
    "%a:\n0: a < b <;\n",
    "%a:\n0: z;\n-x: <<;\n",
    "%a:\n0: z;\nx.x: << >%%f>;\n%%f:\n10: >>;\n",
    "%a:\n0: =%nothing=;\n",
    "%a:\n10: a;\n5: b;\n",
    "%a:\n0: z $(cardinal,one{a})$;\n",
    "%a:\n0: z;\nx.x: << >%%f>;\n%%f:\n10: <<;\n",
    "%a:\n10: x $(nominal,other{b})$;\n",
    "%a:\n1x0: z;\n",
    "%a:\n10/0: z;\n",
    "%a:\n10>>: z;\n",
    // The library's own refusal: ICU reads %%f as a normal set, then
    // formats with it as a fraction rule set.
    "%%f:\n10: <%a< x;\n%a:\n0: z;\nx.x: << >%%f>;\n",
};

static void TestRefused(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_refused) / sizeof(s_refused[0]); i++)
    {
        RbnfGroup group;
        char error[256];
        if (RbnfCompile(s_refused[i], strlen(s_refused[i]), &group, error, sizeof(error)))
        {
            printf("compiled: %s", s_refused[i]);
            RbnfFree(&group);
            all = false;
        }
    }
    CHECK(all, "rules ICU refuses are refused");
}

// A unit with every kind of piece: brackets, a ">>>" remainder, a
// fraction rule set (two rules of one base), a plural block, patterns
// (one with its point) and a last rule without text.
static const char s_unit[] =
    "%a:\n-x: minus >>;\nx.x: << and >%%f>;\n0: zero;\none;\n10: ten[->>];\n20: x >>> y;\n"
    "100: << $(cardinal,one{c}other{cs})$[ >>];\n1000: =#,##0=;\n%%f:\n10: <%a< tenths;\n"
    "10: <%a< tenth;\n100: <#,##0< hundredths;\n%c:\n0: =#,##0.=;\n1: ;\n";

static uint8_t* Unit(size_t* sizeOut)
{
    RbnfGroup group;
    char error[256];
    if (!RbnfCompile(s_unit, strlen(s_unit), &group, error, sizeof(error)))
    {
        printf("%s\n", error);
        return nullptr;
    }
    uint8_t* bytes = SpelloutPayload(&group, sizeOut);
    RbnfFree(&group);
    return bytes;
}

static bool Valid(const uint8_t* bytes, size_t size)
{
    return mlocValidSpelloutRules(nullptr, (DataSpan){bytes, (uint32_t)size});
}

// Whether a copy of the unit with a byte or a 16-bit field changed is
// refused.
static bool Changed(const uint8_t* unit, size_t size, size_t at, uint32_t value, int bytes)
{
    uint8_t* copy = malloc(size);
    memcpy(copy, unit, size);
    copy[at] = (uint8_t)value;
    if (bytes == 2)
    {
        copy[at + 1] = (uint8_t)(value >> 8);
    }
    bool refused = !Valid(copy, size);
    free(copy);
    return refused;
}

// Whether the unit with two 16-bit fields changed is refused.
static bool TwoChanged(const uint8_t* unit, size_t size, size_t at, uint32_t value, size_t other,
                       uint32_t otherValue)
{
    uint8_t* copy = malloc(size);
    memcpy(copy, unit, size);
    copy[at] = (uint8_t)value;
    copy[at + 1] = (uint8_t)(value >> 8);
    copy[other] = (uint8_t)otherValue;
    copy[other + 1] = (uint8_t)(otherValue >> 8);
    bool refused = !Valid(copy, size);
    free(copy);
    return refused;
}

static bool Refused(const uint8_t* unit, size_t size, size_t at, uint8_t value)
{
    return Changed(unit, size, at, value, 1);
}

// The unit with a rule's divisor made a radix to an exponent.
static bool RadixRefused(const uint8_t* unit, size_t size, size_t rule, uint32_t radix,
                         uint8_t exponent)
{
    uint8_t* copy = malloc(size);
    memcpy(copy, unit, size);
    copy[rule + 10] = (uint8_t)radix;
    copy[rule + 11] = (uint8_t)(radix >> 8);
    copy[rule + 13] &= (uint8_t)~spelloutPowerOfTen;
    copy[rule + 15] = exponent;
    bool refused = !Valid(copy, size);
    free(copy);
    return refused;
}

static void TestUnit(void)
{
    size_t size = 0;
    uint8_t* unit = Unit(&size);
    CHECK(unit != nullptr && Valid(unit, size), "the unit the generator writes is valid");
    if (unit == nullptr)
    {
        return;
    }
    SpelloutRules r = mlocReadSpelloutRules((DataSpan){unit, (uint32_t)size});
    size_t sets = spelloutHeaderSize;
    size_t rules = (size_t)(r.rules - unit);
    size_t pieces = (size_t)(r.pieces - unit);
    size_t plurals = (size_t)(r.plurals - unit);
    size_t patterns = (size_t)(r.patterns - unit);
    size_t text = (size_t)(r.text - unit);
    CHECK(r.setCount == 3 && r.pluralCount == 1 && r.patternCount == 2,
          "its sets, plural block and patterns");
    // The first substitution piece, and the plural block's other form.
    size_t sub = pieces;
    while (unit[sub] != spelloutSubstitution)
    {
        sub += spelloutPieceSize;
    }
    CHECK(!Valid(unit, size - 1) && !Valid(unit, 8) &&
              Refused(unit, size, 0, (uint8_t)(r.setCount + 1)) && Refused(unit, size, 10, 9) &&
              !Refused(unit, size, 14, spelloutGroupUnparseable) && Refused(unit, size, 14, 2) &&
              Refused(unit, size, 15, 1),
          "counts, the default set and the flags");
    CHECK(Refused(unit, size, sets + 2, 0) && Refused(unit, size, sets + 3, 0x80) &&
              Refused(unit, size, sets + 6, 200),
          "a set's name, flags and rules");
    // %a's eight normal rules, then its two special ones.
    size_t special = rules + 8 * spelloutRuleSize;
    CHECK(Refused(unit, size, special + 12, spelloutNormal) &&
              Refused(unit, size, rules + 12, spelloutNegative) &&
              Refused(unit, size, rules + 12, spelloutKindCount) &&
              Refused(unit, size, rules + 13, 0x80) && Refused(unit, size, rules + 14, 0xFF) &&
              Refused(unit, size, rules + 15, 25),
          "a rule's kind, flags, pieces and exponent");
    // "1: one" made "0" again: equal bases only in a fraction rule set,
    // which has two rules of base 10.
    CHECK(Refused(unit, size, rules + spelloutRuleSize, 0), "bases in order");
    // "20: x >>> y" divided by 20 to the 14th fits 64 bits, to the 15th
    // does not; a radix is at least 2.
    size_t twenty = rules + 4 * spelloutRuleSize;
    CHECK(!RadixRefused(unit, size, twenty, 20, 14) && RadixRefused(unit, size, twenty, 20, 15) &&
              RadixRefused(unit, size, twenty, 1, 1),
          "a divisor within 64 bits");
    // %c's two rules made none.
    CHECK(Changed(unit, size, sets + 2 * spelloutSetSize + 6, 0, 2), "a set with no rules");
    // The plural block's "one" made empty at the text's end.
    CHECK(!TwoChanged(unit, size, plurals + 4, r.textSize, plurals + 6, 0),
          "an empty form at the end of the text");
    CHECK(Refused(unit, size, pieces, spelloutPieceTypeCount) && Refused(unit, size, sub + 1, 7) &&
              !Refused(unit, size, sub + 2, 2) && Refused(unit, size, sub + 2, 9) &&
              Refused(unit, size, sub + 1, 0x80),
          "a piece's type and a substitution's kind and target");
    CHECK(Refused(unit, size, plurals + 22, 0xFF) && Refused(unit, size, plurals + 23, 0xFF) &&
              Refused(unit, size, patterns, 0) && Refused(unit, size, patterns + 1, 21),
          "a plural block's other form and a pattern's digits");
    // The block's order, one then other (0x51, then 0xF nibbles).
    CHECK(mlocLoad32(unit + plurals + 24) == 0xFFFFFF51u &&
              !Refused(unit, size, plurals + 24, 0x15) && Refused(unit, size, plurals + 24, 0x11) &&
              Refused(unit, size, plurals + 24, 0x61) && Refused(unit, size, plurals + 24, 0xF1) &&
              Refused(unit, size, plurals + 24, 0x21) && Refused(unit, size, plurals + 26, 0x7F),
          "a plural block's order: another order, a repeat, a category past other, a form "
          "left out, one absent, a nibble after the end");
    CHECK(Refused(unit, size, text, 0xFF) && Refused(unit, size, text, 0),
          "the text is well-formed UTF-8 without NUL");
    free(unit);
}

// The numbering systems unit: "hans" and "hansfin" (zh's cardinal and
// financial rule sets), sorted, the text with a byte no record uses.
static void SystemsUnit(uint8_t* unit, size_t* sizeOut)
{
    static const char s_text[] = "hanszh%spellout-cardinalhansfin%spellout-cardinal-financialx";
    static const uint8_t s_records[2][spelloutSystemSize] = {
        {0, 0, 4, 0, 6, 0, 4, 2, 18, spelloutGroupSpellout},
        {24, 0, 4, 0, 31, 0, 7, 2, 28, spelloutGroupSpellout},
    };
    size_t text = sizeof(s_text) - 1;
    unit[0] = 2;
    unit[1] = 0;
    unit[2] = (uint8_t)text;
    unit[3] = 0;
    memcpy(unit + spelloutSystemHeaderSize, s_records, sizeof(s_records));
    memcpy(unit + spelloutSystemHeaderSize + sizeof(s_records), s_text, text);
    *sizeOut = spelloutSystemHeaderSize + sizeof(s_records) + text;
}

// Whether the unit with one or two bytes changed is refused.
static bool SystemsRefused(size_t at, uint8_t value, size_t other, uint8_t otherValue)
{
    uint8_t unit[96];
    size_t size = 0;
    SystemsUnit(unit, &size);
    unit[at] = value;
    unit[other] = other == at ? value : otherValue;
    return !mlocValidSpelloutSystems(nullptr, (DataSpan){unit, (uint32_t)size});
}

static void TestSystems(void)
{
    uint8_t unit[96];
    size_t size = 0;
    SystemsUnit(unit, &size);
    size_t second = spelloutSystemHeaderSize + spelloutSystemSize;
    CHECK(mlocValidSpelloutSystems(nullptr, (DataSpan){unit, (uint32_t)size}),
          "a numbering systems unit, an id the start of the next");
    static const uint8_t s_empty[4] = {0};
    CHECK(mlocValidSpelloutSystems(nullptr, (DataSpan){s_empty, 4}) &&
              !mlocValidSpelloutSystems(nullptr, (DataSpan){s_empty, 2}),
          "an empty unit, and one shorter than its header");
    // The text size one short: the records still within it.
    CHECK(SystemsRefused(0, 3, 0, 3) && SystemsRefused(2, (uint8_t)(unit[2] - 1), 2, 0),
          "its counts");
    // A locale empty at the text's end.
    CHECK(!SystemsRefused(second + 2, unit[2], second + 7, 0), "an empty range at the end");
    // The second id made "hans" too: not after the first.
    CHECK(SystemsRefused(second + 8, 0, second + 8, 0) &&
              SystemsRefused(second + 8, 30, second + 8, 30) &&
              SystemsRefused(second + 9, spelloutGroupCount, second + 9, spelloutGroupCount) &&
              SystemsRefused(second + 6, 4, second + 6, 4),
          "a name, its range, the group and the ids' order");
}

// A titlecase unit of "a" and "ß" ("A" and "Ss").
static const uint8_t s_titlecase[] = {
    2,    0,   3,   0,             // count, text size
    'a',  0,   0,   0, 0, 0, 1, 0, // "a": offset 0, length 1
    0xDF, 0,   0,   0, 1, 0, 2, 0, // "ß": offset 1, length 2
    'A',  'S', 's',                // the text
};

static bool TitlecaseRefused(size_t at, uint8_t value)
{
    uint8_t unit[sizeof(s_titlecase)];
    memcpy(unit, s_titlecase, sizeof(unit));
    unit[at] = value;
    return !mlocValidSpelloutTitlecase(nullptr, (DataSpan){unit, sizeof(unit)});
}

static void TestTitlecase(void)
{
    DataSpan span = {s_titlecase, sizeof(s_titlecase)};
    const uint8_t* text = nullptr;
    uint32_t length = 0;
    CHECK(mlocValidSpelloutTitlecase(nullptr, span) &&
              mlocSpelloutTitlecase(span, 0xDF, &text, &length) && length == 2 &&
              memcmp(text, "Ss", 2) == 0 && mlocSpelloutTitlecase(span, 'a', &text, &length) &&
              length == 1 && text[0] == 'A' && !mlocSpelloutTitlecase(span, 'b', &text, &length) &&
              !mlocSpelloutTitlecase(span, 0, &text, &length),
          "a titlecase unit and its lookup");
    static const uint8_t s_none[4] = {0};
    CHECK(TitlecaseRefused(0, 3) && TitlecaseRefused(2, 2) &&
              !mlocValidSpelloutTitlecase(nullptr, (DataSpan){s_titlecase, 3}) &&
              mlocValidSpelloutTitlecase(nullptr, (DataSpan){s_none, 4}),
          "its counts and header; an empty table");
    CHECK(TitlecaseRefused(12, 'a') && TitlecaseRefused(4, 0xE0) && TitlecaseRefused(15, 0x11),
          "code points out of order or past U+10FFFF");
    CHECK(TitlecaseRefused(10, 0) && TitlecaseRefused(11, 1) && TitlecaseRefused(16, 4) &&
              TitlecaseRefused(18, 3) && TitlecaseRefused(21, 0x80),
          "an empty titlecase, the zero byte, the range and ill-formed UTF-8");
    static const uint8_t s_contexts[] = {contextAll, contextAll + 1};
    CHECK(mlocValidLocaleContext(nullptr, (DataSpan){s_contexts, 1}) &&
              !mlocValidLocaleContext(nullptr, (DataSpan){s_contexts + 1, 1}) &&
              !mlocValidLocaleContext(nullptr, (DataSpan){s_contexts, 2}),
          "a context transforms unit: a byte per usage, no other bits");
}

// The rule text ICU's case locales would titlecase otherwise.
static void TestCasesAsIcu(void)
{
    CHECK(SpelloutCasesAsIcu("nl", "acht; vijf; zij", 15) &&
              !SpelloutCasesAsIcu("nl_BE", "x: ijs;", 7) && SpelloutCasesAsIcu("en", "ijs", 3),
          "Dutch ij only beginning a word");
    CHECK(!SpelloutCasesAsIcu("lt", "i\xCC\x87", 3) && SpelloutCasesAsIcu("lv", "i\xCC\x87", 3) &&
              !SpelloutCasesAsIcu("hy", "\xD6\x87", 2) && SpelloutCasesAsIcu("tr", "iki", 3) &&
              !SpelloutCasesAsIcu("en", "\xC3", 1),
          "a Lithuanian dot above, Armenian ech-yiwn, Turkish i, ill-formed text");
}

// A data file with one group, root's spell-out rules.
static uint8_t* RootFile(const uint8_t* unit, size_t unitSize, size_t* sizeOut)
{
    DataFile file = {0};
    DataFileAdd(&file, kindSpelloutRules, "root", unit, unitSize);
    mlocDataVersion own;
    mlocGetDataVersion(&own);
    DataFileVersion version = {
        .cldr = mlocPackVersion(own.cldr.major, own.cldr.minor, own.cldr.patch),
        .unicode = mlocPackVersion(own.unicode.major, own.unicode.minor, own.unicode.patch),
        .library = mlocPackVersion(own.library.major, own.library.minor, own.library.patch),
        .tables = own.tables,
    };
    memcpy(version.tzdb, own.tzdb, sizeof(version.tzdb));
    uint8_t* bytes = DataFileWrite(&file, &version, sizeOut);
    DataFileFree(&file);
    return bytes;
}

// Formats a number by a rule set of root's rules, a byte of their unit
// changed first (none for at 0), into text (NUL-terminated on success).
static mlocResult FormatText(const char* rules, const char* name, size_t at, uint8_t value,
                             const char* number, char* text, size_t capacity)
{
    RbnfGroup group;
    char error[256];
    if (!RbnfCompile(rules, strlen(rules), &group, error, sizeof(error)))
    {
        return (mlocResult)-100;
    }
    size_t unitSize = 0;
    uint8_t* unit = SpelloutPayload(&group, &unitSize);
    RbnfFree(&group);
    if (at != 0)
    {
        unit[at] = value;
    }
    size_t size = 0;
    uint8_t* bytes = RootFile(unit, unitSize, &size);
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    mlocDataSet set = {sources, 2};
    mlocLocale locale;
    mlocSpelloutFormatter formatter;
    mlocDecimal decimal;
    mlocOutput output = {.buffer = text, .capacity = capacity - 1};
    mlocResult r = mlocOpenData(&sources[1], bytes, size);
    r = r == mloc_success ? mlocParseLocale("en", 2, &locale).status : r;
    r = r == mloc_success
            ? mlocInitSpelloutFormatter(&formatter, &set, &locale, name, strlen(name), nullptr)
            : r;
    r = r >= 0 && mlocParseDecimal(number, strlen(number), &decimal).status == mloc_success
            ? mlocFormatSpellout(&formatter, &decimal, &output)
            : (mlocResult)-101;
    free(bytes);
    free(unit);
    if (r >= 0)
    {
        text[output.length] = '\0';
    }
    return r;
}

static mlocResult FormatRoot(const char* rules, size_t at, uint8_t value, const char* number)
{
    char text[512];
    return FormatText(rules, "%a", at, value, number, text, sizeof(text));
}

static void TestEngineLimits(void)
{
    // "<<" by ten: 10^63 nests 64 rule sets deep, 10^64 one past ICU's
    // limit.
    static const char s_tens[] = "%a:\n0: x;\n10: << y;\n";
    CHECK(FormatRoot(s_tens, 0, 0, "1e63") == mloc_success &&
              FormatRoot(s_tens, 0, 0, "1e64") == mloc_errorCapacity,
          "rule sets nested past 64 levels");
    // A ">>>" remainder made to take its own rule: 15 keeps 5 forever.
    static const char s_triple[] = "%a:\n0: x;\n10: a >>> b;\n";
    RbnfGroup group;
    char error[256];
    size_t piece = 0;
    if (RbnfCompile(s_triple, strlen(s_triple), &group, error, sizeof(error)))
    {
        size_t size = 0;
        uint8_t* unit = SpelloutPayload(&group, &size);
        SpelloutRules r = mlocReadSpelloutRules((DataSpan){unit, (uint32_t)size});
        piece = (size_t)(r.pieces - unit);
        while (unit[piece] != spelloutSubstitution)
        {
            piece += spelloutPieceSize;
        }
        free(unit);
        RbnfFree(&group);
    }
    CHECK(FormatRoot(s_triple, 0, 0, "15") == mloc_success &&
              FormatRoot(s_triple, piece + 4, 1, "15") == mloc_errorCapacity,
          "rules a remainder calls directly without end");
}

// Rules CLDR's data barely reaches, ICU4C 79's results: a divisor not a
// power of ten taking a fraction (%duration's 60), a negative value
// without a "-x" rule, whose fraction is x - floor(x) for a fraction rule
// set and its own digits digit by digit, and a fraction rule set's
// choice of base.
static const char s_minutes[] = "%a:\n-x: minus >>;\n0: =#,##0.##=;\n"
                                "60/60: <%%m< min[ >>];\n%%m:\n0: =#,##0=;\n";
static const char s_digits[] = "%a:\nx.x: << point >>;\n"
                               "0: zero; one; two; three; four; five; six; seven; eight; nine;\n"
                               "10: ten[ >>];\n";
static const char s_parts[] = "%b:\nx.x: << point >%a>;\n0: =#,##0=;\n%a:\n10: <%b< tenths;\n"
                              "100: <%b< hundredths;\n1000: <%b< thousandths;\n";

// A base of one, two equal bases (the second unless the value times the
// base is one), and a set after the last base, which the choice must not
// read.
static const char s_choice[] = "%b:\nx.x: << point >%a>;\n0: =#,##0=;\n%a:\n1: <%b< wholes;\n"
                               "10: <%b< tenth;\n10: <%b< tenths;\n1000: <%b< thousandths;\n"
                               "%c:\n1000: past;\n";

static const struct
{
    const char* rules;
    const char* name;
    const char* number;
    const char* expected;
} s_results[] = {
    {s_minutes, "%a", "61.5", "1 min 1.5"},
    {s_minutes, "%a", "3661.25", "61 min 1.25"},
    {s_minutes, "%a", "-61.5", "minus 1 min 1.5"},
    {s_minutes, "%a", "120.75", "2 min 0.75"},
    {s_minutes, "%a", "59.5", "0 min"},
    {s_digits, "%a", "-1.25", "two point two five"},
    {s_digits, "%a", "-0.001", "one point zero zero one"},
    {s_digits, "%a", "-12.375", "ten three point three seven five"},
    {s_parts, "%b", "-1.25", "-2 point 75 hundredths"},
    {s_parts, "%b", "-0.001", "-1 point 999 thousandths"},
    {s_parts, "%b", "-12.375", "-13 point 625 thousandths"},
    {s_parts, "%b", "-0.0625", "-1 point 938 thousandths"},
    {s_parts, "%a", "0.5", "5 tenths"},
    {s_parts, "%a", "0.0625", "63 thousandths"},
    {s_parts, "%a", "0.9999", "10 tenths"},
    {s_parts, "%a", "0.0005", "1 thousandths"},
    {s_parts, "%a", "0.00049", "0 tenths"},
    {s_parts, "%a", "2", " tenths"},
    {s_parts, "%a", "100.5", "1,005 tenths"},
    {s_choice, "%a", "1.0004", "1 wholes"},
    {s_choice, "%a", "0.9996", "1 wholes"},
    {s_choice, "%a", "0.1", "1 tenth"},
    {s_choice, "%a", "0.2", "2 tenths"},
    {s_choice, "%a", "0.125", "125 thousandths"},
    {s_choice, "%a", "0.04", "40 thousandths"},
};

static void TestEngineResults(void)
{
    for (size_t i = 0; i < sizeof(s_results) / sizeof(s_results[0]); i++)
    {
        char text[256];
        mlocResult r = FormatText(s_results[i].rules, s_results[i].name, 0, 0, s_results[i].number,
                                  text, sizeof(text));
        if (r != mloc_success || strcmp(text, s_results[i].expected) != 0)
        {
            printf("%s %s: got %d \"%s\", expected \"%s\"\n", s_results[i].name,
                   s_results[i].number, (int)r, r == mloc_success ? text : "",
                   s_results[i].expected);
            s_failures++;
        }
    }
}

// A negative value's fraction, 1 - f, holds at most a decimal's 40
// places; a value too large to scale by a fraction rule set's bases
// cannot be its numerator.
static void TestFractionLimits(void)
{
    char text[256];
    CHECK(FormatText(s_parts, "%b", 0, 0, "-1e-40", text, sizeof(text)) == mloc_success &&
              FormatText(s_parts, "%b", 0, 0, "-1e-41", text, sizeof(text)) == mloc_errorCapacity,
          "a negative fraction to 40 places");
    CHECK(FormatText(s_parts, "%a", 0, 0, "1e999999", text, sizeof(text)) == mloc_errorCapacity,
          "a numerator past a decimal's exponent");
    // A remainder by a divisor not a power of ten, of a value past 64 bits.
    static const char s_seconds[] = "%a:\n0: =#,##0.##=;\n60/60: >#,##0.##> s;\n";
    CHECK(FormatText(s_seconds, "%a", 0, 0, "61.5", text, sizeof(text)) == mloc_success &&
              strcmp(text, "1.5 s") == 0 &&
              FormatText(s_seconds, "%a", 0, 0, "1e30", text, sizeof(text)) == mloc_errorCapacity,
          "a remainder past 64 bits");
}

// Parses text by root's rules; the value as mlocFormatDecimal writes it.
static mlocResult ParseText(const char* rules, const char* text, char* value, size_t capacity,
                            size_t* endOut)
{
    RbnfGroup group;
    char error[256];
    *endOut = 0;
    value[0] = '\0';
    if (!RbnfCompile(rules, strlen(rules), &group, error, sizeof(error)))
    {
        return (mlocResult)-100;
    }
    size_t unitSize = 0;
    uint8_t* unit = SpelloutPayload(&group, &unitSize);
    RbnfFree(&group);
    size_t size = 0;
    uint8_t* bytes = RootFile(unit, unitSize, &size);
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    mlocDataSet set = {sources, 2};
    mlocLocale locale;
    mlocSpelloutFormatter formatter;
    mlocDecimal decimal;
    mlocResult r = mlocOpenData(&sources[1], bytes, size);
    r = r == mloc_success ? mlocParseLocale("en", 2, &locale).status : r;
    r = r == mloc_success
            ? mlocInitSpelloutFormatter(&formatter, &set, &locale, nullptr, 0, nullptr)
            : r;
    r = r >= 0 ? mlocParseSpellout(&formatter, text, strlen(text), &decimal, endOut) : r;
    mlocOutput output = {.buffer = value, .capacity = capacity - 1};
    if (r == mloc_success && mlocFormatDecimal(&decimal, &output) == mloc_success)
    {
        value[output.length] = '\0';
    }
    free(bytes);
    free(unit);
    return r;
}

static bool ParsesTo(const char* rules, const char* text, const char* expected, size_t end)
{
    char value[64];
    size_t got = 0;
    mlocResult r = ParseText(rules, text, value, sizeof(value), &got);
    bool same = expected == nullptr
                    ? r == mloc_errorSyntax && got == 0
                    : r == mloc_success && got == end && strcmp(value, expected) == 0;
    if (!same)
    {
        printf("\"%s\": %d %s end %zu\n", text, (int)r, value, got);
    }
    return same;
}

// ICU4C 79's parse of each text by the same rules.
static void TestParseRules(void)
{
    // A plural block's forms are searched in the order written: "aab"
    // first finds the block after " ", "b" first finds a "b" without it.
    static const char s_oneFirst[] = "%a:\n0: x;\n1: y;\n10: << $(cardinal,one{aab}other{b})$;\n";
    static const char s_otherFirst[] = "%a:\n0: x;\n1: y;\n10: << $(cardinal,other{b}one{aab})$;\n";
    CHECK(ParsesTo(s_oneFirst, "y aab b", "10", 5) && ParsesTo(s_otherFirst, "y aab b", "1", 1),
          "a plural block's order decides the form read");
    static const char s_twice[] = "%a:\n0: zero;\n10: << << ten;\n";
    CHECK(ParsesTo(s_twice, "zero", nullptr, 0),
          "two substitutions of one class: ICU parses nothing");
    static const char s_triple[] = "%a:\n0: x;\n1: y;\n2: z;\n10: q >>>;\n20: w >>> v;\n";
    CHECK(ParsesTo(s_triple, "q z", "12", 3) && ParsesTo(s_triple, "w q z v", "32", 7) &&
              ParsesTo(s_triple, "w q y v", nullptr, 0) && ParsesTo(s_triple, "y", "1", 1),
          "a \">>>\" remainder reads by the rule before only");
    static const char s_numerator[] =
        "%a:\nx.x: << p >%%f>;\n0: zero; one; two; three; four; five; six; seven; eight; nine;\n"
        "%%f:\n10: <%a< tenths;\n100: <%a<< hundredths;\n";
    CHECK(ParsesTo(s_numerator, "zero p five tenths", "0.5", 18) &&
              ParsesTo(s_numerator, "zero p five hundredths", "0.5", 22) &&
              ParsesTo(s_numerator, "zero p zero five hundredths", "0", 4) &&
              ParsesTo(s_numerator, "one p two", "1", 3),
          "numerators; with \"<<\", the zeros read leave ICU short of the text");
}

int main(void)
{
    TestCompile();
    TestRefused();
    TestUnit();
    TestSystems();
    TestTitlecase();
    TestCasesAsIcu();
    TestEngineLimits();
    TestEngineResults();
    TestFractionLimits();
    TestParseRules();
    return s_failures == 0 ? 0 : 1;
}
