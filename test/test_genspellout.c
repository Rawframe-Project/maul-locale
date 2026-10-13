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
// opening, every corruption of it refused.

#include "spellout_compile.h"
#include "spellout_data.h"
#include "spellout_gen.h"
#include "test_harness.h"

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
// fraction rule set, a plural block and patterns.
static const char s_unit[] =
    "%a:\n-x: minus >>;\nx.x: << and >%%f>;\n0: zero;\none;\n"
    "10: ten[->>];\n20: x >>> y;\n100: << $(cardinal,one{c}other{cs})$[ >>];\n"
    "1000: =#,##0=;\n%%f:\n10: <%a< tenths;\n100: <#,##0< hundredths;\n";

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

// Whether a copy of the unit with one change is refused.
static bool Refused(const uint8_t* unit, size_t size, size_t at, uint8_t value)
{
    uint8_t* copy = malloc(size);
    memcpy(copy, unit, size);
    copy[at] = value;
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
    CHECK(r.setCount == 2 && r.pluralCount == 1 && r.patternCount == 1,
          "its sets, plural block and patterns");
    // The first substitution piece, and the plural block's other form.
    size_t sub = pieces;
    while (unit[sub] != spelloutSubstitution)
    {
        sub += spelloutPieceSize;
    }
    CHECK(!Valid(unit, size - 1) && Refused(unit, size, 0, (uint8_t)(r.setCount + 1)) &&
              Refused(unit, size, 10, 9) && Refused(unit, size, 14, 1),
          "counts, the default set and the zero field");
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
    // The third rule, "1: one", has a base after "0: zero"'s.
    CHECK(Refused(unit, size, rules + 3 * spelloutRuleSize, 0), "bases in order");
    CHECK(Refused(unit, size, pieces, spelloutPieceTypeCount) && Refused(unit, size, sub + 1, 7) &&
              Refused(unit, size, sub + 2, 9) && Refused(unit, size, sub + 1, 0x80),
          "a piece's type and a substitution's kind and target");
    CHECK(Refused(unit, size, plurals + 22, 0xFF) && Refused(unit, size, plurals + 23, 0xFF) &&
              Refused(unit, size, patterns, 0) && Refused(unit, size, patterns + 1, 21),
          "a plural block's other form and a pattern's digits");
    CHECK(Refused(unit, size, text, 0xFF) && Refused(unit, size, text, 0),
          "the text is well-formed UTF-8 without NUL");
    free(unit);
}

// The numbering systems unit: "hans" (zh's %spellout-cardinal) and
// "roman" (the locale's own %roman-upper), sorted.
static void SystemsUnit(uint8_t* unit, size_t* sizeOut)
{
    static const char s_text[] = "hanszh%spellout-cardinalroman%roman-upper";
    static const uint8_t s_records[2][spelloutSystemSize] = {
        {0, 0, 4, 0, 6, 0, 4, 2, 18, spelloutGroupSpellout},
        {24, 0, 29, 0, 29, 0, 5, 0, 12, spelloutGroupNumbering},
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
          "a numbering systems unit");
    CHECK(SystemsRefused(0, 3, 0, 3) && SystemsRefused(2, 40, 2, 40), "its counts");
    // The second id made "hans" too: not after the first.
    CHECK(SystemsRefused(second + 8, 0, second + 8, 0) &&
              SystemsRefused(second + 8, 13, second + 8, 13) &&
              SystemsRefused(second + 9, spelloutGroupCount, second + 9, spelloutGroupCount) &&
              SystemsRefused(second, 0, second + 6, 4),
          "a name, its range, the group and the ids' order");
}

int main(void)
{
    TestCompile();
    TestRefused();
    TestUnit();
    TestSystems();
    return s_failures == 0 ? 0 : 1;
}
