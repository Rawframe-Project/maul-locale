// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation rule parser: each relation, reset and setting; quoting,
// escapes and comments; starred relations and their ranges; imports in
// their place; sets; and every refusal with the byte it names.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_rules.h"
#include "test_harness.h"

#include "maul-unicode/properties.h"

#include <stdlib.h>
#include <string.h>

static CollationRules s_rules;
static char s_error[256];

static bool Parse(const char* text)
{
    CollationRulesFree(&s_rules);
    s_error[0] = '\0';
    return CollationRulesParse(text, strlen(text), &s_rules, s_error, sizeof(s_error)) == 1;
}

// Whether rule k has the kind, strength and text (as code points).
static bool Is(size_t k, uint8_t kind, uint8_t strength, const uint32_t* text, uint32_t length)
{
    const CollationRule* r = k < s_rules.count ? &s_rules.rules[k] : nullptr;
    return r != nullptr && r->kind == kind && r->strength == strength && r->textLength == length &&
           memcmp(s_rules.points + r->text, text, length * sizeof(uint32_t)) == 0;
}

#define IS(k, kind, strength, ...)                                                                 \
    Is(k, kind, strength, (const uint32_t[]){__VA_ARGS__},                                         \
       sizeof((const uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t))

static void TestRelations(void)
{
    CHECK(Parse("&a<b<<c<<<d<<<<e=f;g,h") && s_rules.count == 8 &&
              IS(0, ruleReset, ruleIdentical, 'a') && IS(1, ruleRelation, rulePrimary, 'b') &&
              IS(2, ruleRelation, ruleSecondary, 'c') && IS(3, ruleRelation, ruleTertiary, 'd') &&
              IS(4, ruleRelation, ruleQuaternary, 'e') && IS(5, ruleRelation, ruleIdentical, 'f') &&
              IS(6, ruleRelation, ruleSecondary, 'g') && IS(7, ruleRelation, ruleTertiary, 'h'),
          "every relation operator, ; and , as << and <<<");
    CHECK(Parse("&C < ç <<< Ç & [before 1] i < ı") && s_rules.count == 5 &&
              IS(1, ruleRelation, rulePrimary, 0xE7) && IS(3, ruleReset, rulePrimary, 'i') &&
              IS(4, ruleRelation, rulePrimary, 0x131),
          "spaces around operators, a second chain, [before 1]");
    const CollationRule* r = nullptr;
    bool ok = Parse("&a<x|ch/h<<y");
    r = ok ? &s_rules.rules[1] : nullptr;
    CHECK(ok && r->prefixLength == 1 && s_rules.points[r->prefix] == 'x' && r->textLength == 2 &&
              r->extensionLength == 1 && s_rules.points[r->extension] == 'h',
          "a prefix, a contraction and an expansion");
    CHECK(Parse("&[last regular]<a &[before 2][first variable]<<b &[top]<c &[variable top]<d") &&
              s_rules.rules[0].position == rulePositionLastRegular &&
              s_rules.rules[2].position == rulePositionFirstVariable &&
              s_rules.rules[2].strength == ruleSecondary &&
              s_rules.rules[4].position == rulePositionLastRegular &&
              s_rules.rules[6].position == rulePositionLastVariable,
          "special reset positions, top and variable top as ICU maps them");
}

static void TestStrings(void)
{
    CHECK(Parse("&'&'<'<'<''<'a''b'<\\-") && IS(0, ruleReset, ruleIdentical, '&') &&
              IS(1, ruleRelation, rulePrimary, '<') && IS(2, ruleRelation, rulePrimary, '\'') &&
              IS(3, ruleRelation, rulePrimary, 'a', '\'', 'b') &&
              IS(4, ruleRelation, rulePrimary, '-'),
          "quoted syntax, a doubled apostrophe in and out of quotes, a backslash escape");
    CHECK(Parse("&\\u00E7<\\U0001F600<\\x{1F601}<'\\uFDD0A'<'\\\\'") &&
              IS(0, ruleReset, ruleIdentical, 0xE7) && IS(1, ruleRelation, rulePrimary, 0x1F600) &&
              IS(2, ruleRelation, rulePrimary, 0x1F601) &&
              IS(3, ruleRelation, rulePrimary, 0xFDD0, 'A') &&
              IS(4, ruleRelation, rulePrimary, '\\'),
          "\\u takes four digits, \\U eight, \\x{} up to six, \\\\ a backslash, in quotes too");
    CHECK(Parse("&\\x30<\\x7") && IS(0, ruleReset, ruleIdentical, '0') &&
              IS(1, ruleRelation, rulePrimary, 0x07),
          "\\x takes one or two digits, a 0 among them");
    CHECK(Parse("# a comment\n&a<b # another\n<c\n!") && s_rules.count == 3 &&
              IS(2, ruleRelation, rulePrimary, 'c'),
          "comments to the line's end, between relations too, and ! ignored");
    CHECK(Parse("&a<*bcd-fh") && s_rules.count == 7 && IS(1, ruleRelation, rulePrimary, 'b') &&
              IS(4, ruleRelation, rulePrimary, 'e') && IS(6, ruleRelation, rulePrimary, 'h') &&
              Parse("&a=*xy") && IS(2, ruleRelation, ruleIdentical, 'y') &&
              Parse("&a<*\\u0000-\\u0002") && s_rules.count == 4 &&
              IS(3, ruleRelation, rulePrimary, 2),
          "starred relations one code point each, a range expanded");
}

static void TestSettings(void)
{
    CHECK(Parse("[strength 2][alternate shifted][maxVariable symbol][caseFirst upper]"
                "[caseLevel on][normalization on][numericOrdering off][backwards 2]") &&
              s_rules.strength == ruleSecondary && s_rules.alternate == 1 &&
              s_rules.maxVariable == 2 && s_rules.caseFirst == 2 && s_rules.caseLevel == 1 &&
              s_rules.normalization == 1 && s_rules.numeric == 0 && s_rules.backwards == 1 &&
              Parse("[strength I]@") && s_rules.strength == ruleIdentical &&
              s_rules.backwards == 1 && s_rules.caseFirst == ruleUnset,
          "every setting and value, @ as [backwards 2], unset ones left so");
    CHECK(Parse("[reorder Zyyy]") && s_rules.reorderCount == 1 &&
              s_rules.reorder[0] == MUNI_SCRIPT('Z', 'y', 'y', 'y'),
          "a script code with a z");
    CHECK(Parse("[reorder Grek digit others latn][hiraganaQ off]") && s_rules.reorderCount == 4 &&
              s_rules.reorder[0] == MUNI_SCRIPT('G', 'r', 'e', 'k') &&
              s_rules.reorder[1] == ruleReorderDigit &&
              s_rules.reorder[2] == MUNI_SCRIPT('Z', 'z', 'z', 'z') &&
              s_rules.reorder[3] == MUNI_SCRIPT('L', 'a', 't', 'n') && Parse("[reorder]") &&
              s_rules.reorderGiven && s_rules.reorderCount == 0,
          "reorder codes: scripts in any case, groups, others; an empty reorder");
    CHECK(Parse("[import und-u-co-search]&a<b[import de-u-co-phonebk]") && s_rules.count == 4 &&
              s_rules.rules[0].kind == ruleImport &&
              strcmp(s_rules.imports + s_rules.rules[0].text, "und-u-co-search") == 0 &&
              s_rules.rules[3].kind == ruleImport &&
              strcmp(s_rules.imports + s_rules.rules[3].text, "de-u-co-phonebk") == 0,
          "imports as rules in their place");
    CHECK(Parse("[suppressContractions [Ии]][optimize [a-c \\u0300 x - z]]") &&
              s_rules.suppressCount == 2 && s_rules.suppress[0] == 0x418 &&
              s_rules.suppress[3] == 0x438 && s_rules.optimizeCount == 3 &&
              s_rules.optimize[1] == 'c' && s_rules.optimize[2] == 0x300 &&
              s_rules.optimize[4] == 'x' && s_rules.optimize[5] == 'z',
          "sets of code points and ranges");
}

static void TestRefusals(void)
{
    static const struct
    {
        const char* rules;
        size_t byte;
        const char* reason;
    } bad[] = {
        {"&a", 2, nullptr},
        {"&[before 1]a<<b", 12, nullptr},
        {"&[before 2]a<<b<c", 15, nullptr},
        {"&a<'b", 5, nullptr},
        {"&a<b\\", 5, nullptr},
        {"&a<\\uD800", 3, nullptr},
        {"&a<\\uFFFD", 3, nullptr},
        {"&a<\\u12", 3, nullptr},
        {"&[first nothing]<a", 1, nullptr},
        {"&a<*\\u00E0", 4, nullptr},
        {"&a<*c-a", 6, nullptr},
        {"&a<*-c", 4, nullptr},
        {"&a<\\u0300|b", 3, nullptr},
        {"[strength 5]", 0, nullptr},
        {"[colour on]", 0, nullptr},
        {"[hiraganaQ on]", 0, "hiraganaQ on"},
        {"[hiraganaQ maybe]", 0, "not a setting"},
        {"&a<\\uDFFF", 3, nullptr},
        {"[optimize [a", 12, nullptr},
        {"[reorder Latin]", 0, nullptr},
        {"[import ]", 0, nullptr},
        {"[optimize [[:L:]]]", 11, nullptr},
        {"[optimize [a-c]", 15, nullptr},
        {"x", 0, nullptr},
        {"&", 1, "without a position"},
        {"&a<\xC3", 3, nullptr},
        {"[caseFirst upper][import de]", 17, nullptr},
        {"@[import de]", 1, nullptr},
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        char want[32];
        snprintf(want, sizeof(want), "byte %zu: ", bad[i].byte);
        bool refused = !Parse(bad[i].rules) && strncmp(s_error, want, strlen(want)) == 0 &&
                       (bad[i].reason == nullptr || strstr(s_error, bad[i].reason) != nullptr);
        if (!refused)
        {
            printf("%s: %s\n", bad[i].rules, s_error);
        }
        CHECK(refused, "a refusal naming its byte");
    }
}

static void TestWriteBack(void)
{
    const char* text = "[import ru][caseFirst lower][reorder Cyrl others]&\\u0418<<<'#'<a|b/c"
                       "&[before 3][last tertiary ignorable]<<<x='''y'";
    CollationRules first;
    char error[128] = "";
    bool ok = CollationRulesParse(text, strlen(text), &first, error, sizeof(error)) == 1;
    size_t length = 0;
    char* written = ok ? CollationRulesWrite(&first, &length) : nullptr;
    ok = ok && Parse(written) && s_rules.count == first.count &&
         s_rules.caseFirst == first.caseFirst && s_rules.reorderCount == 2 &&
         s_rules.importCount == 1 &&
         memcmp(s_rules.points, first.points, first.pointCount * 4) == 0;
    CHECK(ok, "written back, the rules parse the same");
    free(written);
    CollationRulesFree(&first);
    // Syntax, a backslash and white space in sets and strings, a set
    // beginning with '-' and one holding surrogates (both found by the fuzz
    // target).
    static const char* const tricky[] = {
        "[optimize [-ac \\u0300]]",
        "[suppressContractions [\\\\ \\] \\- a]]&'\\'<\\'<'\xC2\x85'<'a b'",
        "[suppressContractions [a \\ude00 \\uD800-\\uDBFF]]",
    };
    for (size_t i = 0; i < 3; i++)
    {
        ok = CollationRulesParse(tricky[i], strlen(tricky[i]), &first, error, sizeof(error)) == 1;
        written = ok ? CollationRulesWrite(&first, &length) : nullptr;
        CollationRules again;
        bool same = ok && CollationRulesParse(written, length, &again, error, sizeof(error)) == 1;
        same = same && CollationRulesSame(&first, &again);
        if (same)
        {
            CollationRulesFree(&again);
        }
        CHECK(same, "sets and strings with syntax, backslashes, spaces and surrogates written "
                    "back the same");
        free(written);
        CollationRulesFree(&first);
    }
}

static void TestSame(void)
{
    static const char* const pairs[][2] = {
        {"[reorder Latn]&a<b", "[reorder Grek]&a<b"},
        {"[optimize [a]]", "[optimize [b]]"},
        {"[suppressContractions [a]]", "[suppressContractions [b]]"},
        {"&a<x|b", "&a<y|b"},
        {"&a<b/x", "&a<b/y"},
        {"&a<b", "&a<c"},
        {"[import de]", "[import fr]"},
        {"[caseFirst upper]", "[caseFirst lower]"},
    };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
    {
        CollationRules a;
        CollationRules b;
        char error[128];
        bool parsed =
            CollationRulesParse(pairs[i][0], strlen(pairs[i][0]), &a, error, sizeof(error)) == 1;
        parsed = parsed && CollationRulesParse(pairs[i][1], strlen(pairs[i][1]), &b, error,
                                               sizeof(error)) == 1;
        CHECK(parsed && CollationRulesSame(&a, &a) && !CollationRulesSame(&a, &b),
              "lists that differ in one part are not the same");
        if (parsed)
        {
            CollationRulesFree(&a);
            CollationRulesFree(&b);
        }
    }
}

int main(void)
{
    TestRelations();
    TestStrings();
    TestSettings();
    TestRefusals();
    TestWriteBack();
    TestSame();
    CollationRulesFree(&s_rules);
    return s_failures == 0 ? 0 : 1;
}
