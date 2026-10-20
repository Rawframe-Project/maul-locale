// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder (record mloc-0071) on a small root: relations of
// each strength after a reset and before it, a reset on a tailored
// string, a prefix, an extension, a contraction, case bits, the special
// positions, [import] through the caller, and the rules refused, each
// by the order of the elements it gives. conform_collation_builder
// checks every CLDR type against ICU4C 79.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_builder.h"
#include "collation_root.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

static const char* s_path = "collation_builder.txt";

static const char s_file[] = "[Unified_Ideograph 4E00..4E05]\n"
                             "[fixed last secondary common byte 45]\n"
                             "[fixed first ignorable secondary byte 80]\n"
                             "[fixed first ignorable tertiary byte 3C]\n"
                             "FDD0 0063; [,, 3D 02]\n"
                             "0009; [03 05, 05, 05]\n"
                             "FDD1 201C; [05 06 02, 05, 05]\n"
                             "201C; [05 08, 05, 05]\n"
                             "FDD1 263A; [0C 02 02, 05, 05]\n"
                             "263A; [0C 04, 05, 05]\n"
                             "FDD1 004C; [2A 02 02, 05, 05]\n"
                             "0061; [2A 10, 05, 05]\n"
                             "0062; [2A 12, 05, 05]\n"
                             "0063; [2A 14, 05, 05]\n"
                             "0078; [2A 20, 05, 05]\n"
                             "0058; [2A 20, 05, 9C]\n"
                             "0301; [, 88, 05]\n"
                             "0302; [, 8A, 05]\n"
                             "FDD1 5B57; [FD 02 02, 05, 05]\n";

static RootTable s_root;
static CollationBuilder s_b;
static char s_error[256];

static const CollationRules* Import(void* context, const char* tag)
{
    return strcmp(tag, "xx") == 0 ? context : nullptr;
}

// Builds the rules into s_b; false with s_error on a refusal.
static bool Build(const char* text, const CollationRules* imported)
{
    CollationRules rules;
    s_error[0] = '\0';
    if (!CollationRulesParse(text, strlen(text), &rules, s_error, sizeof(s_error)))
    {
        return false;
    }
    bool ok =
        CollationBuilderInit(&s_b, &s_root, Import, (void*)imported, s_error, sizeof(s_error)) &&
        CollationBuilderAdd(&s_b, &rules) && CollationBuilderFinish(&s_b);
    CollationRulesFree(&rules);
    return ok;
}

// The first element of a string of code points through the tailoring.
static uint64_t First(const uint32_t* text, uint32_t length)
{
    uint64_t out[collationMaxExpansion] = {0};
    CollationMappingsElements(&s_b.mappings, text, length, 0, out, 0);
    return out[0];
}

static uint64_t One(uint32_t c)
{
    return First(&c, 1);
}

static uint32_t P(uint64_t e)
{
    return (uint32_t)(e >> 32);
}

static uint32_t S(uint64_t e)
{
    return (uint32_t)e >> 16;
}

static uint32_t T(uint64_t e)
{
    return (uint32_t)e & 0x3F3F;
}

static void TestStrengths(void)
{
    CHECK(Build("&a<x", nullptr) && P(One('x')) > 0x2A100000 && P(One('x')) < 0x2A120000 &&
              S(One('x')) == 0x0500 && T(One('x')) == 0x0500,
          "a primary after a, before b");
    CollationBuilderFree(&s_b);
    CHECK(Build("&a<<x", nullptr) && P(One('x')) == 0x2A100000 && S(One('x')) > 0x0500 &&
              S(One('x')) < 0x8000,
          "a secondary after a, below the secondaries of secondary elements");
    CollationBuilderFree(&s_b);
    CHECK(Build("&a<<<x", nullptr) && P(One('x')) == 0x2A100000 && S(One('x')) == 0x0500 &&
              T(One('x')) > 0x0500 && T(One('x')) < 0x3C00,
          "a tertiary after a");
    CollationBuilderFree(&s_b);
    CHECK(Build("&a=x", nullptr) && One('x') == One('a'), "an identical relation");
    CollationBuilderFree(&s_b);
}

static void TestSequences(void)
{
    CHECK(Build("&a<x<y", nullptr) && P(One('x')) > 0x2A100000 && P(One('y')) > P(One('x')) &&
              P(One('y')) < 0x2A120000,
          "two primaries in a row");
    CollationBuilderFree(&s_b);
    CHECK(Build("&b<x &a<y", nullptr) && P(One('y')) < 0x2A120000 && P(One('x')) > 0x2A120000,
          "two resets");
    CollationBuilderFree(&s_b);
    CHECK(Build("&a<x &x<y", nullptr) && P(One('y')) > P(One('x')) && P(One('y')) < 0x2A120000,
          "a reset on a tailored code point");
    CollationBuilderFree(&s_b);
}

static void TestBefore(void)
{
    CHECK(Build("&[before 1]b<x", nullptr) && P(One('x')) > 0x2A100000 && P(One('x')) < 0x2A120000,
          "[before 1]");
    CollationBuilderFree(&s_b);
    CHECK(Build("&[before 2]a<<x", nullptr) && P(One('x')) == 0x2A100000 && S(One('x')) < 0x0500 &&
              S(One('x')) > 0x0100,
          "[before 2]: a secondary below the common one");
    CollationBuilderFree(&s_b);
    CHECK(Build("&[before 3]a<<<x", nullptr) && P(One('x')) == 0x2A100000 &&
              S(One('x')) == 0x0500 && T(One('x')) < 0x0500 && T(One('x')) > 0x0100,
          "[before 3]: a tertiary below the common one");
    CollationBuilderFree(&s_b);
    CHECK(Build("&a<x &[before 1]x<y", nullptr) && P(One('y')) > 0x2A100000 &&
              P(One('y')) < P(One('x')),
          "[before 1] a tailored code point");
    CollationBuilderFree(&s_b);
}

static void TestContexts(void)
{
    const uint32_t bx[] = {'b', 'x'};
    const uint32_t xc[] = {'x', 'c'};
    CHECK(Build("&a<b|x", nullptr) && P(First(bx + 1, 1)) == 0x2A200000 &&
              P(One('b')) == 0x2A120000,
          "a prefix leaves x alone");
    uint64_t out[collationMaxExpansion];
    CHECK(CollationMappingsElements(&s_b.mappings, bx, 2, 0, out, 0) == 2 &&
              P(out[1]) > 0x2A100000 && P(out[1]) < 0x2A120000,
          "and gives x after b the tailored primary");
    CollationBuilderFree(&s_b);
    CHECK(Build("&a<x/c", nullptr) &&
              CollationMappingsElements(&s_b.mappings, xc, 1, 0, out, 0) == 2 &&
              P(out[0]) > 0x2A100000 && P(out[1]) == 0x2A140000,
          "an extension appends c's element");
    CollationBuilderFree(&s_b);
    CHECK(Build("&a<xc", nullptr) &&
              CollationMappingsElements(&s_b.mappings, xc, 2, 0, out, 0) == 1 &&
              P(out[0]) < 0x2A120000 && P(One('x')) == 0x2A200000,
          "a contraction");
    CollationBuilderFree(&s_b);
}

static void TestCaseBits(void)
{
    CHECK(Build("&a<X<x<<<\\u0301", nullptr) && (One('X') & 0xC000) == 0x8000 &&
              (One('x') & 0xC000) == 0,
          "case bits from the root's elements of the string");
    CollationBuilderFree(&s_b);
}

static void TestPositions(void)
{
    CHECK(Build("&[first variable]<x", nullptr) && P(One('x')) > 0x03050000 &&
              P(One('x')) < 0x05080000,
          "[first variable]");
    CollationBuilderFree(&s_b);
    CHECK(Build("&[last variable]<x", nullptr) && P(One('x')) > 0x05080000 &&
              P(One('x')) < 0x0C020200,
          "[last variable]: after punctuation's last primary");
    CollationBuilderFree(&s_b);
    CHECK(Build("&[first regular]<x", nullptr) && P(One('x')) > 0x0C040000 &&
              P(One('x')) < 0x2A020200,
          "[first regular]: past the symbol group's boundary");
    CollationBuilderFree(&s_b);
}

static void TestMorePositions(void)
{
    CHECK(Build("&[first implicit]<x", nullptr) && P(One('x')) > 0xFD020200 &&
              P(One('x')) < 0xFD020400,
          "[first implicit]: after U+4E00");
    CollationBuilderFree(&s_b);
    CHECK(Build("&[first primary ignorable]<<x", nullptr) && P(One('x')) == 0 &&
              S(One('x')) > 0x8800 && S(One('x')) < 0x8A00,
          "[first primary ignorable]");
    CollationBuilderFree(&s_b);
    CHECK(Build("&[last primary ignorable]<<x", nullptr) && P(One('x')) == 0 &&
              S(One('x')) > 0x8A00,
          "[last primary ignorable]");
    CollationBuilderFree(&s_b);
    CHECK(Build("&[first secondary ignorable]<<<x", nullptr) && P(One('x')) == 0 &&
              S(One('x')) == 0 && T(One('x')) > 0x3D02,
          "[first secondary ignorable]");
    CollationBuilderFree(&s_b);
}

static void TestImport(void)
{
    CollationRules imported;
    char error[128];
    bool parsed = CollationRulesParse("&a<x", 4, &imported, error, sizeof(error));
    CHECK(parsed && Build("[import xx]&x<y", &imported) && P(One('x')) < P(One('y')) &&
              P(One('y')) < 0x2A120000,
          "[import] reads the imported rules in place");
    CollationBuilderFree(&s_b);
    CHECK(!Build("[import yy]&a<y", nullptr) && strstr(s_error, "not found") != nullptr,
          "an import not found is refused");
    CollationBuilderFree(&s_b);
    if (parsed)
    {
        CollationRulesFree(&imported);
    }
}

static void TestRefusals(void)
{
    static const char* const cases[][2] = {
        {"&[before 1][first tertiary ignorable]<x", "ignorable not possible"},
        {"&[before 2][first secondary ignorable]<<x", "secondary-before secondary ignorable"},
        {"&[before 3][first tertiary ignorable]<<<x", "tertiary-before completely ignorable"},
        {"&[last implicit]<x", "[last implicit] not supported"},
        {"&[last trailing]<x", "U+FFFF"},
        {"&\\u0301<x", "after ignorables not supported"},
        {"&\\u0378<x", "unassigned code point"},
        {"&a<x\\u1100", "ending with conjoining Jamo"},
        {"&a<\\u1161x", "starting with conjoining Jamo"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bool refused = !Build(cases[i][0], nullptr) && strstr(s_error, cases[i][1]) != nullptr;
        if (!refused)
        {
            fprintf(stderr, "  %s: %s\n", cases[i][0], s_error);
        }
        CHECK(refused, "a rule ICU refuses");
        CollationBuilderFree(&s_b);
    }
}

int main(void)
{
    FILE* f = fopen(s_path, "wb");
    if (f == nullptr)
    {
        return 1;
    }
    fputs(s_file, f);
    fclose(f);
    if (!RootRead(s_path, &s_root, s_error, sizeof(s_error)))
    {
        CHECK(false, s_error);
        return 1;
    }
    TestStrengths();
    TestSequences();
    TestBefore();
    TestContexts();
    TestCaseBits();
    TestPositions();
    TestMorePositions();
    TestImport();
    TestRefusals();
    RootFree(&s_root);
    remove(s_path);
    return s_failures == 0 ? 0 : 1;
}
