// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unsafe-backward ranges on a small root and a tailoring over it: the
// code points after the first of each contraction, a prefix's own
// contractions too, and never a prefix's code points; the readers refuse a
// section whose count, order or code points are wrong.
// conform_collation_fast --unsafe checks every CLDR type against ICU4C's
// sets.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_builder.h"
#include "collation_fast_table.h"
#include "collation_root.h"
#include "collation_tailoring.h"
#include "collation_unit.h"
#include "collation_unsafe_table.h"
#include "table_gen.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char s_file[] = "[UCA version = 18.0.1]\n"
                             "[Unified_Ideograph 4E00..4E01]\n"
                             "0061; [2B 10, 05, 05]\n"
                             "0062; [2B 20, 05, 05]\n"
                             "0063; [2B 30, 05, 05]\n"
                             "0064; [2B 40, 05, 05]\n"
                             "0068; [2B 50, 05, 05]\n"
                             "0063 0068; [2B 60, 05, 05]\n"
                             "004C; [2B 70, 05, 05]\n"
                             "00B7; [, 8C, 05]\n"
                             "004C | 00B7; [, FB B6, 05]\n"
                             "0301; [, 88, 05]\n"
                             "0323; [, 90, 05]\n"
                             "0323 0301; [, 92, 05]\n"
                             "0E40; [2C 10, 05, 05]\n"
                             "0E01; [2C 20, 05, 05]\n"
                             "0E40 0E01; [2C 30, 05, 05]\n";

static RootTable s_root;
static uint8_t* s_bytes;
static size_t s_size;

// The root's unit with an empty fast Latin section and its ranges.
static bool BuildRoot(void)
{
    const char* path = "collation_unsafe_table.txt";
    FILE* f = fopen(path, "wb");
    if (f == nullptr)
    {
        return false;
    }
    fputs(s_file, f);
    fclose(f);
    char error[128] = "";
    bool ok = RootRead(path, &s_root, error, sizeof(error));
    remove(path);
    s_bytes = ok ? CollationRootUnit(&s_root, &s_size, error, sizeof(error)) : nullptr;
    if (s_bytes == nullptr)
    {
        fprintf(stderr, "  %s\n", error);
        return false;
    }
    s_bytes = CollationFastLatinAppend(s_bytes, &s_size, nullptr, 0);
    CollationRoot view;
    if (!mlocReadCollationRoot((DataSpan){s_bytes, (uint32_t)s_size}, &view))
    {
        return false;
    }
    s_bytes = CollationUnsafeAppend(s_bytes, &s_size, &view);
    return true;
}

// Whether a unit's ranges are the expected first and last code points.
static bool RangesAre(const CollationRoot* r, const uint32_t* want, uint32_t count)
{
    bool same = r->unsafeCount == count;
    for (uint32_t i = 0; same && i < count; i++)
    {
        same = mlocLoad32(r->unsafe + (size_t)i * 8) == want[2 * i] &&
               mlocLoad32(r->unsafe + (size_t)i * 8 + 4) == want[2 * i + 1];
    }
    return same;
}

static void TestRoot(void)
{
    CollationRoot r;
    bool read = mlocReadCollationRoot((DataSpan){s_bytes, (uint32_t)s_size}, &r);
    static const uint32_t want[] = {0x68, 0x68, 0x301, 0x301, 0xE01, 0xE01};
    CHECK(read && RangesAre(&r, want, 3), "a contraction's later code points, not a prefix's");
    CHECK(read && mlocCollationInUnsafe(r.unsafe, r.unsafeCount, 0x68) &&
              mlocCollationInUnsafe(r.unsafe, r.unsafeCount, 0xE01) &&
              !mlocCollationInUnsafe(r.unsafe, r.unsafeCount, 0x63) &&
              !mlocCollationInUnsafe(r.unsafe, r.unsafeCount, 0xB7) &&
              !mlocCollationInUnsafe(r.unsafe, r.unsafeCount, 0x4C) &&
              !mlocCollationInUnsafe(r.unsafe, r.unsafeCount, 0x10FFFF),
          "the lookup finds the ranges' code points and no other");
}

static void TestTailoring(void)
{
    static const char rules[] = "&b < ch < dx\\u0301 &a < b|d &a < e|fg";
    CollationRules parsed = {0};
    CollationBuilder b = {0};
    char error[256] = "";
    bool ok = CollationRulesParse(rules, strlen(rules), &parsed, error, sizeof(error)) &&
              CollationBuilderInit(&b, &s_root, nullptr, nullptr, error, sizeof(error)) &&
              CollationBuilderAdd(&b, &parsed) && CollationBuilderFinish(&b);
    size_t size = 0;
    uint8_t* unit =
        ok ? CollationTailoringUnit(&b.mappings, &b.settings, &size, error, sizeof(error))
           : nullptr;
    CollationTailoring t;
    ok = unit != nullptr;
    if (ok)
    {
        unit = CollationFastLatinAppend(unit, &size, nullptr, 0);
        ok = mlocReadCollationTailoring((DataSpan){unit, (uint32_t)size}, &t);
        unit = ok ? CollationUnsafeAppend(unit, &size, &t.view) : unit;
        ok = ok && mlocReadCollationTailoring((DataSpan){unit, (uint32_t)size}, &t);
    }
    static const uint32_t want[] = {0x67, 0x68, 0x78, 0x78, 0x301, 0x301};
    CHECK(ok && RangesAre(&t.view, want, 3), "a tailoring's own contractions, its prefix left out: "
                                             "each code point after the first, a mark among them");
    CollationBuilderFree(&b);
    CollationRulesFree(&parsed);
    free(unit);
}

// Whether the root's unit with a crafted section reads.
static bool ReadsWith(const uint32_t* words, size_t count, size_t cut)
{
    CollationRoot plain;
    if (!mlocReadCollationRoot((DataSpan){s_bytes, (uint32_t)s_size}, &plain))
    {
        return false;
    }
    size_t at = s_size - 4 - (size_t)plain.unsafeCount * 8;
    size_t size = at + count * 4 - cut;
    uint8_t* bytes = malloc(at + count * 4 + 1);
    memcpy(bytes, s_bytes, at);
    for (size_t i = 0; i < count; i++)
    {
        GenPut(bytes + at + i * 4, words[i], 4);
    }
    CollationRoot r;
    bool ok = mlocReadCollationRoot((DataSpan){bytes, (uint32_t)size}, &r);
    free(bytes);
    return ok;
}

static void TestValidator(void)
{
    CHECK(ReadsWith((const uint32_t[]){0}, 1, 0) && ReadsWith((const uint32_t[]){0}, 1, 4) &&
              ReadsWith((const uint32_t[]){2, 0x41, 0x42, 0x44, 0x10FFFF}, 5, 0),
          "no section, an empty one, and ranges apart up to U+10FFFF read");
    CHECK(!ReadsWith((const uint32_t[]){0}, 1, 1) &&
              !ReadsWith((const uint32_t[]){2, 0x41, 0x42}, 3, 0) &&
              !ReadsWith((const uint32_t[]){1, 0x41, 0x42, 0}, 4, 0),
          "a cut count, too few ranges or a word after them are refused");
    CHECK(!ReadsWith((const uint32_t[]){1, 0x42, 0x41}, 3, 0) &&
              !ReadsWith((const uint32_t[]){1, 0x41, 0x110000}, 3, 0) &&
              !ReadsWith((const uint32_t[]){2, 0x41, 0x44, 0x44, 0x45}, 5, 0) &&
              !ReadsWith((const uint32_t[]){2, 0x45, 0x46, 0x41, 0x42}, 5, 0),
          "a range backwards, past U+10FFFF, touching the last or before it is refused");
}

int main(void)
{
    bool built = BuildRoot();
    CHECK(built, "the small root builds with its ranges");
    if (built)
    {
        TestRoot();
        TestTailoring();
        TestValidator();
        RootFree(&s_root);
    }
    free(s_bytes);
    return s_failures == 0 ? 0 : 1;
}
