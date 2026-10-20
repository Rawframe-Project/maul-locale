// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root elements the tailoring builder reads (record mloc-0071), on a
// small file of FractionalUCA.txt's form: the table unit by unit
// (ignorables, primaries, runs of two- and three-byte primaries, the
// case bits dropped, a common unit after weights below it, Han's runs
// around a Han character with weights of its own, the root's own
// primaries, the sentinel), the queries on it, and the elements it
// refuses. conform_collation_root_elements checks the whole root
// against ICU4C's table and queries.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_root.h"
#include "collation_root_elements.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

static const char* s_path = "collation_root_elements.txt";

static int Build(const char* text, RootElements* e, char* error, size_t size)
{
    FILE* f = fopen(s_path, "wb");
    if (f == nullptr)
    {
        return 0;
    }
    fputs(text, f);
    fclose(f);
    RootTable t;
    if (!RootRead(s_path, &t, error, size))
    {
        return 0;
    }
    int ok = RootElementsBuild(&t, e, error, size);
    RootFree(&t);
    return ok;
}

static const char s_file[] = "[UCA version = 18.0.1]\n"
                             "[Unified_Ideograph 4E00..4E05]\n"
                             "[top_byte\t03\tSPACE]\n"
                             "[top_byte\t2A\tLatn\tCOMPRESS]\n"
                             "[fixed last secondary common byte 45]\n"
                             "[fixed first ignorable secondary byte 80]\n"
                             "[fixed first ignorable tertiary byte 3C]\n"
                             "FDD0 0063; [,, 3D 02]\n"
                             "0300; [, 8A, 05]\n"
                             "0301; [, 88, 05]\n"
                             "0009; [03 05, 05, 05]\n"
                             "000A; [03 07, 05, 05]\n"
                             "000B; [03 09, 05, 05]\n"
                             "000C; [03 0B, 05, 05][, 8A, 05]\n"
                             "1F600; [0D 4C 5C, 05, 05]\n"
                             "1F601; [0D 4C 5E, 05, 05]\n"
                             "1F602; [0D 4C 60, 05, 05]\n"
                             "0061; [2A 10, 05, 05]\n"
                             "0041; [2A 10, 05, 9C]\n"
                             "0062; [2A 12, 05, 05]\n"
                             "00E1; [2A 12, 05, 05][, 88, 05]\n"
                             "0063; [2A 14, 05, 05]\n"
                             "01C4; [2A 16, 03, 05]\n"
                             "2F00; [U+4E01, 70, 20]\n"
                             "FDD1 0061; [2A 10, 05, 05]\n"
                             "FDD1 FDD0; [E4, 05, 05]\n"
                             "FDD0 0034; [10, 05, 05]\n"
                             "FFFD; [EF FD, 05, 05]\n"
                             "FFFE; [02, 05, 05]\n"
                             "FFFF; [EF FF, 05, 05]\n";

static const uint32_t s_units[] = {
    5, 6, 8, 0x05000500, 0x4580003C,
    // The tertiary and secondary ignorables.
    0x00003D82, 0x88000580, 0x8A000580,
    // 03 05 to 03 0B, two apart; 0D 4C 5C, then 0D 4C 5E to 0D 4C 60.
    0x03050000, 0x030B0002, 0x0D4C5C00, 0x0D4C6002, 0x10000000,
    // A's tertiary without its case bits; 2A 12 to 2A 16, the last with a
    // secondary below the common one, then the common unit.
    0x2A100000, 0x05001C80, 0x2A120000, 0x2A160002, 0x03000580, 0x05000580,
    // Han: the run to U+4E01, its own weights, the run to the end.
    0xFD020200, 0xFD020402, 0x70002080, 0xFD020600, 0xFD020E02,
    // The root's own primaries and the sentinel.
    0xFE040200, 0xFF020200, 0xFFFD0000, 0xFFFF0000, 0xFFFFFF00};

static bool Same(const RootElements* e, const uint32_t* units, size_t count)
{
    bool same = e->length == (int32_t)count;
    for (size_t i = 0; same && i < count; i++)
    {
        if (e->units[i] != units[i])
        {
            fprintf(stderr, "  unit %zu is %08X, not %08X\n", i, e->units[i], units[i]);
            same = false;
        }
    }
    return same;
}

static void TestTable(void)
{
    RootElements e = {0};
    char error[128] = "";
    int ok = Build(s_file, &e, error, sizeof(error));
    CHECK(ok, error);
    CHECK(ok && Same(&e, s_units, sizeof(s_units) / sizeof(s_units[0])), "the table, unit by unit");
    CHECK(ok && e.firstHan == 0xFD020200 && e.lastHan == 0xFD020E00,
          "Han's primaries, the last one past the last character's");
    CHECK(ok && RootElementsFirstPrimary(&e) == 0x03050000 &&
              RootElementsFirstTertiary(&e) == 0x3D02 && RootElementsLastTertiary(&e) == 0x3D02 &&
              RootElementsFirstSecondary(&e) == 0x88000500 &&
              RootElementsLastSecondary(&e) == 0x8A000500,
          "the first primary, the tertiary and secondary ignorables' ends");
    CHECK(ok && RootElementsTertiaryBoundary(&e) == 0x3C00 &&
              RootElementsLastCommonSecondary(&e) == 0x4500 &&
              RootElementsSecondaryBoundary(&e) == 0x8000,
          "the bounds from the fixed bytes");
    RootElementsFree(&e);
    CHECK(e.units == nullptr && e.length == 0, "freed");
}

static void TestPrimaries(void)
{
    RootElements e = {0};
    char error[128] = "";
    if (!Build(s_file, &e, error, sizeof(error)))
    {
        CHECK(false, error);
        return;
    }
    CHECK(RootElementsFindPrimary(&e, 0x03070000) == 8 &&
              RootElementsFindPrimary(&e, 0x030B0000) == 9 &&
              RootElementsFindPrimary(&e, 0x2A160000) == 16 &&
              RootElementsFindPrimary(&e, 0xFD020800) == 22 &&
              RootElementsFindPrimary(&e, 0xFFFF0000) == 27,
          "a primary's unit, or its run's first");
    CHECK(RootElementsPrimaryBefore(&e, 0x03070000, false) == 0x03050000 &&
              RootElementsPrimaryBefore(&e, 0x030B0000, false) == 0x03090000 &&
              RootElementsPrimaryBefore(&e, 0x0D4C5C00, false) == 0x030B0000 &&
              RootElementsPrimaryBefore(&e, 0x0D4C6000, false) == 0x0D4C5E00 &&
              RootElementsPrimaryBefore(&e, 0x2A120000, true) == 0x2A100000 &&
              RootElementsPrimaryBefore(&e, 0x2A140000, true) == 0x2A120000 &&
              RootElementsPrimaryBefore(&e, 0xFD020600, false) == 0xFD020400,
          "the primary before, in a run, at its end, after one");
    CHECK(RootElementsPrimaryAfter(&e, 0x03050000, 8, false) == 0x03070000 &&
              RootElementsPrimaryAfter(&e, 0x030B0000, 9, false) == 0x0D4C5C00 &&
              RootElementsPrimaryAfter(&e, 0x0D4C5E00, 10, false) == 0x0D4C6000 &&
              RootElementsPrimaryAfter(&e, 0x2A100000, 13, true) == 0x2A120000 &&
              RootElementsPrimaryAfter(&e, 0xFD020800, 22, false) == 0xFD020A00 &&
              RootElementsPrimaryAfter(&e, 0xFFFD0000, 26, false) == 0xFFFF0000,
          "the primary after, in a run, past deltas, in Han's run");
    RootElementsFree(&e);
}

static void TestAround(void)
{
    RootElements e = {0};
    char error[128] = "";
    if (!Build(s_file, &e, error, sizeof(error)))
    {
        CHECK(false, error);
        return;
    }
    CHECK(RootElementsLastBefore(&e, 0) == 0 && RootElementsFirstAtLeast(&e, 0) == 0,
          "the elements around the primary 0");
    CHECK(RootElementsLastBefore(&e, 0x2A120000) == 0x2A10000005001C00ULL &&
              RootElementsLastBefore(&e, 0x0D4C5C00) == 0x030B000005000500ULL &&
              RootElementsLastBefore(&e, 0x20000000) == 0x1000000005000500ULL &&
              RootElementsLastBefore(&e, 0xFD020200) == 0x2A16000005000500ULL,
          "the last element before a primary, root or not");
    CHECK(RootElementsFirstAtLeast(&e, 0x20000000) == 0x2A10000005000500ULL &&
              RootElementsFirstAtLeast(&e, 0x2A100000) == 0x2A10000005000500ULL &&
              RootElementsFirstAtLeast(&e, 0x10000100) == 0x2A10000005000500ULL,
          "the first element at a primary or after it");
    RootElementsFree(&e);
}

// Runs across a byte's end, written by hand: two- and three-byte
// primaries, compressible and not.
static void TestSteps(void)
{
    uint32_t units[] = {5,          5,          5,          0x05000500, 0,
                        0x0CFE0000, 0x0D040002, 0x0EFFFE00, 0x0F020402, 0x29FD0000,
                        0x2A0A0004, 0x2AFEFE00, 0x2B040402, 0xFFFFFF00};
    RootElements e = {units, (int32_t)(sizeof(units) / sizeof(units[0])), 0, 0};
    CHECK(RootElementsPrimaryAfter(&e, 0x0CFE0000, 5, false) == 0x0D020000 &&
              RootElementsPrimaryBefore(&e, 0x0D020000, false) == 0x0CFE0000,
          "two-byte primaries across a lead byte");
    CHECK(RootElementsPrimaryAfter(&e, 0x0EFFFE00, 7, false) == 0x0F020200 &&
              RootElementsPrimaryBefore(&e, 0x0F020200, false) == 0x0EFFFE00,
          "three-byte primaries across a lead byte");
    CHECK(RootElementsPrimaryAfter(&e, 0x29FD0000, 9, true) == 0x2A060000 &&
              RootElementsPrimaryBefore(&e, 0x2A060000, true) == 0x29FD0000,
          "compressible two-byte primaries, second bytes 04 to FE");
    CHECK(RootElementsPrimaryAfter(&e, 0x2AFEFE00, 11, true) == 0x2B040200 &&
              RootElementsPrimaryBefore(&e, 0x2B040200, true) == 0x2AFEFE00,
          "compressible three-byte primaries");
    CHECK(RootElementsPrimaryBefore(&e, 0x2B040400, true) == 0x2B040200 &&
              RootElementsPrimaryBefore(&e, 0x0F020400, false) == 0x0F020200,
          "the primary before a run's end");
}

static void TestLowerLevels(void)
{
    RootElements e = {0};
    char error[128] = "";
    if (!Build(s_file, &e, error, sizeof(error)))
    {
        CHECK(false, error);
        return;
    }
    CHECK(RootElementsSecondaryBefore(&e, 0, 0x8800) == 0 &&
              RootElementsSecondaryBefore(&e, 0, 0x8A00) == 0x8800 &&
              RootElementsSecondaryBefore(&e, 0x2A100000, 0x0500) == 0x0100 &&
              RootElementsSecondaryBefore(&e, 0x2A160000, 0x0300) == 0x0100 &&
              RootElementsSecondaryBefore(&e, 0x2A160000, 0x0500) == 0x0300,
          "the secondary before, of ignorables and of primaries");
    CHECK(RootElementsSecondaryAfter(&e, 0, 0x8800) == 0x8A00 &&
              RootElementsSecondaryAfter(&e, 0, 0x8A00) == 0x10000 &&
              RootElementsSecondaryAfter(&e, 13, 0x0500) == 0x8000 &&
              RootElementsSecondaryAfter(&e, 16, 0x0300) == 0x0500 &&
              RootElementsSecondaryAfter(&e, 8, 0x0500) == 0x8000 &&
              RootElementsSecondaryAfter(&e, 16, 0x0100) == 0x0300,
          "the secondary after, or the limit of the level");
    CHECK(RootElementsTertiaryBefore(&e, 0, 0, 0x3D02) == 0 &&
              RootElementsTertiaryBefore(&e, 0, 0x8800, 0x0500) == 0x0100 &&
              RootElementsTertiaryBefore(&e, 0x2A100000, 0x0500, 0x0500) == 0x0100 &&
              RootElementsTertiaryBefore(&e, 0x2A100000, 0x0500, 0x1C00) == 0x0500,
          "the tertiary before");
    CHECK(RootElementsTertiaryAfter(&e, 0, 0, 0x3D02) == 0x4000 &&
              RootElementsTertiaryAfter(&e, 0, 0x8800, 0x0500) == 0x3C00 &&
              RootElementsTertiaryAfter(&e, 13, 0x0500, 0x0500) == 0x1C00 &&
              RootElementsTertiaryAfter(&e, 13, 0x0500, 0x1C00) == 0x3C00 &&
              RootElementsTertiaryAfter(&e, 16, 0x0300, 0x0500) == 0x3C00,
          "the tertiary after, or the limit of the level");
    RootElementsFree(&e);
}

// One line added to a root that builds: the error, or "" when it builds.
static const char* Refused(const char* line, char* error, size_t size)
{
    char text[512];
    snprintf(text, sizeof(text), "[Unified_Ideograph 4E00..4E05]\n0061; [2A 10, 05, 05]\n%s\n",
             line);
    RootElements e = {0};
    error[0] = '\0';
    if (Build(text, &e, error, size))
    {
        RootElementsFree(&e);
        error[0] = '\0';
    }
    return error;
}

static void TestRefusals(void)
{
    char error[128];
    CHECK(strstr(Refused("0062; [2A 12 04 02, 05, 05]", error, sizeof(error)), "four bytes") !=
              nullptr,
          "a primary of four bytes");
    CHECK(strstr(Refused("0062; [2A 12, 01, 05]", error, sizeof(error)), "01 xx") != nullptr &&
              strstr(Refused("0062; [2A 12, 05, 01]", error, sizeof(error)), "01 xx") != nullptr,
          "a secondary or a tertiary of 01 00");
    CHECK(strstr(Refused("0062; [2A 12, 05, 05 C2]", error, sizeof(error)), "above 3F") != nullptr,
          "a tertiary's second byte with the top bits");
    CHECK(strstr(Refused("2F00; [U+4E01, 03, 05]", error, sizeof(error)), "Han") != nullptr,
          "a Han primary under weights below the common ones");
    CHECK(Refused("2F00; [U+4E01, 05, 05]", error, sizeof(error))[0] == '\0' &&
              Refused("0062; [2A 12, 01 05, 05]", error, sizeof(error))[0] == '\0',
          "Han's common weights and a secondary of 01 05 build");
}

// A table made by hand, as the reader never gives one: no Han ranges,
// no ignorables, no fixed bytes.
static void TestWithoutHan(void)
{
    // U+FFFD to U+FFFF are left out whatever their elements.
    RootMapping mappings[5] = {
        {.text = {0x61}, .textLength = 1, .elements = {0x2A10000005000500ULL}, .elementCount = 1},
        {.text = {0x62}, .textLength = 1, .elements = {0x2A12000005000500ULL}, .elementCount = 1},
        {.text = {0xFFFD}, .textLength = 1, .elements = {0x3100000005000500ULL}, .elementCount = 1},
        {.text = {0xFFFE}, .textLength = 1, .elements = {0x3200000005000500ULL}, .elementCount = 1},
        {.text = {0xFFFF}, .textLength = 1, .elements = {0x3300000005000500ULL}, .elementCount = 1},
    };
    RootTable t = {.mappings = mappings, .count = 5};
    RootElements e = {0};
    char error[128] = "";
    int ok = RootElementsBuild(&t, &e, error, sizeof(error));
    static const uint32_t units[] = {5,          5,          5,          0x05000500,
                                     0,          0x2A100000, 0x2A120000, 0xFE040200,
                                     0xFF020200, 0xFFFD0000, 0xFFFF0000, 0xFFFFFF00};
    CHECK(ok && e.firstHan == 0 && e.length == (int32_t)(sizeof(units) / sizeof(units[0])) &&
              memcmp(e.units, units, sizeof(units)) == 0,
          "a root without Han ranges or ignorables");
    RootElementsFree(&e);
}

// A root of Han 4E00..4E05 (primaries FD 02 02 to FD 02 0E) and U+0061
// with the given lines added: whether its units are the given ones.
static bool Units(const char* lines, const uint32_t* units, size_t count)
{
    char text[512];
    snprintf(text, sizeof(text), "[Unified_Ideograph 4E00..4E05]\n0061; [2A 10, 05, 05]\n%s",
             lines);
    RootElements e = {0};
    char error[128] = "";
    bool same = Build(text, &e, error, sizeof(error)) && Same(&e, units, count);
    RootElementsFree(&e);
    return same;
}

#define UNITS(lines, ...)                                                                          \
    Units(lines, (const uint32_t[]){__VA_ARGS__},                                                  \
          sizeof((const uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t))

static void TestHanEdges(void)
{
    CHECK(UNITS("2F00; [U+4E00, 70, 20]\n", 5, 5, 5, 0x05000500, 0, 0x2A100000, 0xFD020200,
                0x70002080, 0xFD020400, 0xFD020E02, 0xFE040200, 0xFF020200, 0xFFFD0000, 0xFFFF0000,
                0xFFFFFF00),
          "Han's first character with weights of its own");
    CHECK(UNITS("2F05; [U+4E05, 70, 20]\n0062; [FD 02 0E, 70, 20]\n", 5, 5, 5, 0x05000500, 0,
                0x2A100000, 0xFD020200, 0xFD020C02, 0x70002080, 0xFD020E00, 0x70002080, 0xFE040200,
                0xFF020200, 0xFFFD0000, 0xFFFF0000, 0xFFFFFF00),
          "Han's last character and the primary past it, each with weights of its own");
    CHECK(UNITS("0062; [FD 02 0E, 70, 20]\n", 5, 5, 5, 0x05000500, 0, 0x2A100000, 0xFD020200,
                0xFD020E02, 0x70002080, 0xFE040200, 0xFF020200, 0xFFFD0000, 0xFFFF0000, 0xFFFFFF00),
          "the primary past Han's last character alone");
}

static void TestRunLimits(void)
{
    CHECK(UNITS("[top_byte\t2A\tLatn\tCOMPRESS]\n"
                "0062; [0D 50 02, 05, 05]\n0063; [0D 50 81, 05, 05]\n0064; [0D 51 02, 05, 05]\n"
                "0065; [0D 60 02, 05, 05]\n0066; [0D 60 82, 05, 05]\n0067; [0D 61 04, 05, 05]\n"
                "0068; [2A FA, 05, 05]\n0069; [2A FC, 05, 05]\n006A; [2A FE, 05, 05]\n"
                "006B; [2B 05, 05, 05]\n",
                5, 5, 5, 0x05000500, 0, 0x0D500200, 0x0D51027F, 0x0D600200, 0x0D608200, 0x0D610400,
                0x2A100000, 0x2AFA0000, 0x2AFE0002, 0x2B050000, 0xFD020200, 0xFD020E02, 0xFE040200,
                0xFF020200, 0xFFFD0000, 0xFFFF0000, 0xFFFFFF00),
          "a run of step 127, none of 128, none past a compressible lead byte");
}

int main(void)
{
    TestTable();
    TestPrimaries();
    TestAround();
    TestSteps();
    TestLowerLevels();
    TestRefusals();
    TestWithoutHan();
    TestHanEdges();
    TestRunLimits();
    remove(s_path);
    return s_failures == 0 ? 0 : 1;
}
