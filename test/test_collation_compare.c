// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The general compare (record mloc-0067) over the generated root: each
// level alone, its strength's stop, a text ending first, completely
// ignorable code points, canonically equal texts, Han and unassigned
// code points, and every answer's mirror.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_compare.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static CollationRoot s_root;

// The order of a and b with the settings, checked against its mirror.
static int Compare(const char* a, size_t aLength, const char* b, size_t bLength,
                   const CollationSettings* settings)
{
    int ab = mlocCollationCompare(&s_root, settings, (const uint8_t*)a, aLength, (const uint8_t*)b,
                                  bLength);
    int ba = mlocCollationCompare(&s_root, settings, (const uint8_t*)b, bLength, (const uint8_t*)a,
                                  aLength);
    return ab == -ba ? ab : 2;
}

// The order up to the strength, non-ignorable.
static int Order(const char* a, const char* b, uint32_t strength)
{
    CollationSettings settings = mlocCollationDefaults(&s_root);
    settings.strength = strength;
    return Compare(a, strlen(a), b, strlen(b), &settings);
}

// The order up to the strength, shifted.
static int Shifted(const char* a, const char* b, uint32_t strength)
{
    CollationSettings settings = mlocCollationDefaults(&s_root);
    settings.strength = strength;
    settings.shifted = true;
    return Compare(a, strlen(a), b, strlen(b), &settings);
}

static void TestLevels(void)
{
    CHECK(Order("a", "b", collationPrimary) == -1 && Order("b", "a", collationTertiary) == 1 &&
              Order("a", "a", collationTertiary) == 0,
          "a before b at the first level");
    CHECK(Order("a", "\xC3\xA1", collationPrimary) == 0 &&
              Order("a", "\xC3\xA1", collationSecondary) == -1 &&
              Order("\xC3\xA1", "b", collationSecondary) == -1,
          "an accent only at the second level, after the first's order");
    CHECK(Order("a", "A", collationSecondary) == 0 && Order("a", "A", collationTertiary) == -1 &&
              Order("A", "\xC3\xA1", collationTertiary) == -1,
          "case only at the third level, after the second's order");
    CHECK(Order("cot\xC3\xA9", "c\xC3\xB4te", collationSecondary) == -1,
          "secondaries forwards: cot\xC3\xA9 before c\xC3\xB4te");
}

static void TestShifted(void)
{
    CollationSettings settings = mlocCollationDefaults(&s_root);
    CHECK(settings.variableTop == 0x0BFFFFFF && settings.strength == collationTertiary &&
              !settings.shifted,
          "the root's defaults: tertiary, non-ignorable, the variable top 0B FF FF FF");
    CHECK(Order("a-b", "ab", collationTertiary) == -1 &&
              Shifted("a-b", "ab", collationTertiary) == 0 &&
              Shifted("a-b", "ab", collationQuaternary) == -1 &&
              Shifted("a b", "a-b", collationQuaternary) == -1,
          "punctuation and spaces ignored when shifted until the fourth level, there by their "
          "primaries");
    CHECK(Order("a-b",
                "a-\xCC\x81"
                "b",
                collationSecondary) == -1 &&
              Shifted("a-b",
                      "a-\xCC\x81"
                      "b",
                      collationQuaternary) == 0,
          "an accent after a variable element ignored when shifted");
    CHECK(Shifted("a-b", "a-b\xCC\x81", collationSecondary) == -1,
          "an accent after a letter after a variable element counted");
    CHECK(Shifted("a$b", "ab", collationPrimary) == -1 &&
              Shifted("a\xC3\xA1", "a-a", collationQuaternary) == 1,
          "a currency sign not variable; a variable element only at the fourth level");
    CHECK(Order("a", "a", collationQuaternary) == 0 &&
              Shifted("ab", "ab", collationQuaternary) == 0,
          "equal texts equal at the fourth level");
}

// The order with the settings changed by one function.
static int With(const char* a, const char* b, uint32_t strength, void (*change)(CollationSettings*))
{
    CollationSettings settings = mlocCollationDefaults(&s_root);
    settings.strength = strength;
    change(&settings);
    return Compare(a, strlen(a), b, strlen(b), &settings);
}

static void Backward(CollationSettings* s)
{
    s->backwards = true;
}

static void CaseLevel(CollationSettings* s)
{
    s->caseLevel = true;
}

static void UpperFirst(CollationSettings* s)
{
    s->caseFirst = collationUpperFirst;
}

static void LowerFirst(CollationSettings* s)
{
    s->caseFirst = collationLowerFirst;
}

static void UpperLevel(CollationSettings* s)
{
    s->caseLevel = true;
    s->caseFirst = collationUpperFirst;
}

static void Numeric(CollationSettings* s)
{
    s->numeric = true;
}

static void Nothing(CollationSettings* s)
{
    (void)s;
}

static void TestOptions(void)
{
    // cote, côte, coté, côté as French orders them; forwards côte follows
    // coté.
    static const char* const french[] = {"cote", "c\xC3\xB4te", "cot\xC3\xA9",
                                         "c\xC3\xB4t\xC3\xA9"};
    bool ordered = true;
    for (size_t i = 0; i + 1 < 4; i++)
    {
        ordered = ordered && With(french[i], french[i + 1], collationTertiary, Backward) == -1;
    }
    CHECK(ordered && Order("c\xC3\xB4te", "cot\xC3\xA9", collationTertiary) == 1 &&
              With("c\xC3\xB4te", "c\xC3\xB4te", collationTertiary, Backward) == 0,
          "backwards secondaries: cote, c\xC3\xB4te, cot\xC3\xA9, c\xC3\xB4t\xC3\xA9");
    CHECK(With("\xC3\xA9\xEF\xBF\xBE"
               "e",
               "e\xEF\xBF\xBE\xC3\xA9", collationSecondary, Backward) == 1 &&
              With("ab\xCC\x81",
                   "a\xCC\x81"
                   "b",
                   collationSecondary, Backward) == 1 &&
              With("a\xCC\x81\xCC\x81", "a\xCC\x81", collationSecondary, Backward) == 1,
          "backwards within segments the merge separator parts, from the end, the longer "
          "greater");
    CHECK(With("a", "A", collationPrimary, CaseLevel) == -1 &&
              With("a", "\xC3\xA1", collationPrimary, CaseLevel) == 0 &&
              With("A", "\xC3\xA1", collationPrimary, CaseLevel) == 1 &&
              With("A", "a", collationPrimary, UpperLevel) == -1 &&
              With("a", "A", collationPrimary, Nothing) == 0,
          "a case level at the primary strength: case, not accents; upper first reversing it");
    CHECK(With("A", "a", collationTertiary, UpperFirst) == -1 &&
              With("a", "A", collationTertiary, LowerFirst) == -1 &&
              With("ab", "Ab", collationTertiary, UpperFirst) == 1 &&
              With("a", "\xC3\xA1", collationTertiary, UpperFirst) == -1,
          "case first: upper or lower before the other, after the accents' order");
    CHECK(Order("a\xC3\xA0", "a\xC3\xA0", collationIdentical) == 0 &&
              Order("\xC3\xA0", "a\xCC\x80", collationIdentical) == 0 &&
              Order("ab",
                    "a\xC2\xAD"
                    "b",
                    collationQuaternary) == 0 &&
              Order("ab",
                    "a\xC2\xAD"
                    "b",
                    collationIdentical) == -1 &&
              Order("a", "a\xC2\xAD", collationIdentical) == -1 &&
              Order("l\xEF\xBF\xBE"
                    "f",
                    "l\x01\xEF\xBF\xBE"
                    "f",
                    collationIdentical) == -1,
          "the identical level: NFD code points, canonical equivalents equal, ignorables "
          "counted, U+FFFE below the others");
}

// A run of count copies of digit after head, NUL-ended.
static const char* Run(char* out, const char* head, char digit, size_t count)
{
    size_t n = strlen(head);
    memcpy(out, head, n);
    memset(out + n, digit, count);
    out[n + count] = '\0';
    return out;
}

static void TestNumeric(void)
{
    CHECK(With("a2", "a10", collationTertiary, Nothing) == 1 &&
              With("a2", "a10", collationTertiary, Numeric) == -1 &&
              With("9", "10", collationPrimary, Numeric) == -1 &&
              With("73", "74", collationPrimary, Numeric) == -1 &&
              With("10233", "10234", collationPrimary, Numeric) == -1 &&
              With("9999999", "10000000", collationPrimary, Numeric) == -1 &&
              With("12345678", "12345679", collationPrimary, Numeric) == -1 &&
              With("1200", "1201", collationPrimary, Numeric) == -1 &&
              With("123000000", "1230000000", collationPrimary, Numeric) == -1,
          "numeric: digits by value, across one, two and three bytes and the pairs");
    CHECK(With("007", "7", collationTertiary, Numeric) == 0 &&
              With("0", "000", collationTertiary, Numeric) == 0 &&
              With("007", "7", collationIdentical, Numeric) == -1 &&
              With("a0", "a", collationPrimary, Numeric) == 1,
          "numeric: leading zeros dropped, one kept for zero");
    CHECK(With("\xD9\xA1\xD9\xA0", "9", collationPrimary, Numeric) == 1 &&
              With("\xEF\xBC\x91"
                   "2",
                   "11", collationPrimary, Numeric) == 1 &&
              With("\xEF\xBC\x91"
                   "2",
                   "12", collationPrimary, Numeric) == 0,
          "numeric: any decimal digits, mixed in one run");
    static char a[600];
    static char b[600];
    // 254 nines then 5 against 254 nines then 40: the second segment
    // decides.
    Run(a, "", '9', 254);
    Run(b, "", '9', 254);
    strcat(a, "5");
    strcat(b, "40");
    CHECK(With(a, b, collationPrimary, Numeric) == -1 &&
              With(Run(a, "1", '0', 300), Run(b, "9", '9', 250), collationPrimary, Numeric) == 1 &&
              With(Run(a, "x", '0', 400), "x0", collationTertiary, Numeric) == 0,
          "numeric: runs past a segment of 254 digits");
}

#define Script(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (d))

// The order at the tertiary strength with the codes' reordering; 3 when
// the codes are refused.
static int Reordered(const char* a, const char* b, const uint32_t* codes, size_t count)
{
    static CollationReorder reorder;
    bool changes = false;
    if (mlocCollationMakeReorder(&s_root, codes, count, &reorder, &changes) != mloc_success)
    {
        return 3;
    }
    CollationSettings settings = mlocCollationDefaults(&s_root);
    settings.reorder = changes ? &reorder : nullptr;
    return Compare(a, strlen(a), b, strlen(b), &settings);
}

static void TestReorder(void)
{
    const uint32_t greek[] = {Script('G', 'r', 'e', 'k')};
    const uint32_t others[] = {Script('C', 'y', 'r', 'l'), Script('Z', 'z', 'z', 'z'),
                               Script('L', 'a', 't', 'n')};
    const uint32_t digits[] = {collationGroupDigit, collationGroupPunctuation};
    CHECK(Reordered("a", "\xCE\xB1", nullptr, 0) == -1 &&
              Reordered("\xCE\xB1", "a", greek, 1) == -1 &&
              Reordered("\xCE\xB1", "\xCE\xB2", greek, 1) == -1 &&
              Reordered("\xD0\xB4", "\xCE\xB1", others, 3) == -1 &&
              Reordered("\xCE\xB1", "a", others, 3) == -1 && Reordered("1", ".", digits, 2) == -1 &&
              Reordered("$", "1", digits, 2) == -1 && Reordered(".", "a", digits, 2) == -1,
          "reordering: named scripts first, the groups not named before them, Zzzz sending "
          "the rest to the end");
    const uint32_t twice[] = {Script('G', 'r', 'e', 'k'), Script('G', 'r', 'e', 'k')};
    const uint32_t shared[] = {Script('H', 'i', 'r', 'a'), Script('K', 'a', 'n', 'a')};
    const uint32_t alias[] = {Script('H', 'a', 'n', 'i'), Script('H', 'a', 'n', 's')};
    const uint32_t unknownTwice[] = {Script('Z', 'z', 'z', 'z'), Script('L', 'a', 't', 'n'),
                                     Script('Z', 'z', 'z', 'z')};
    CHECK(Reordered("a", "b", twice, 2) == 3 && Reordered("a", "b", shared, 2) == 3 &&
              Reordered("a", "b", alias, 2) == 3 && Reordered("a", "b", unknownTwice, 3) == 3,
          "reordering refuses duplicates, codes of one range and Zzzz twice");
    static CollationReorder reorder;
    bool changes = true;
    const uint32_t unknown[] = {Script('Z', 'z', 'z', 'z')};
    const uint32_t absent[] = {Script('Q', 'a', 'a', 'a'), collationReservedBeforeLatin,
                               collationReservedAfterLatin};
    bool none = mlocCollationMakeReorder(&s_root, unknown, 1, &reorder, &changes) == mloc_success &&
                !changes;
    none = none &&
           mlocCollationMakeReorder(&s_root, absent, 3, &reorder, &changes) == mloc_success &&
           !changes;
    none = none &&
           mlocCollationMakeReorder(&s_root, nullptr, 0, &reorder, &changes) == mloc_success &&
           !changes;
    CHECK(none, "Zzzz alone, nothing, and codes the root has no range for change nothing");
    // ICU4C 79 on CLDR 49 refuses the first and takes the second; the
    // third it takes with its front wrapped past the last lead byte,
    // unassigned code points then sorting before Latin, and is refused.
    const uint32_t full[] = {Script('E', 't', 'h', 'i'), Script('G', 'r', 'a', 'n'),
                             Script('K', 'a', 'w', 'i'), Script('B', 'u', 'g', 'i'),
                             Script('N', 'e', 'w', 'a')};
    const uint32_t wrap[] = {Script('B', 'h', 'k', 's'), Script('O', 'r', 'k', 'h'),
                             Script('M', 'a', 'r', 'c'), Script('P', 'a', 'u', 'c'),
                             collationGroupSymbol,       Script('S', 'i', 'd', 't')};
    bool capacity =
        mlocCollationMakeReorder(&s_root, full, 5, &reorder, &changes) == mloc_errorCapacity &&
        mlocCollationMakeReorder(&s_root, full, 4, &reorder, &changes) == mloc_success && changes &&
        mlocCollationMakeReorder(&s_root, wrap, 6, &reorder, &changes) == mloc_errorCapacity;
    CHECK(capacity && Reordered("\xCD\xB8", "a", full, 4) == 1 &&
              Reordered("\xE4\xB8\x80", "a", full, 4) == 1,
          "an order past the lead bytes refused, ICU's wrap too; one that fits keeps "
          "unassigned and Han after Latin");
}

static void TestTexts(void)
{
    CHECK(Order("", "a", collationPrimary) == -1 && Order("a", "ab", collationTertiary) == -1 &&
              Order("", "", collationTertiary) == 0,
          "a text that ends first is the lesser");
    CHECK(Order("\xC3\xA0", "a\xCC\x80", collationTertiary) == 0 &&
              Order("\xE1\xBB\x87", "e\xCC\xA3\xCC\x82", collationTertiary) == 0 &&
              Order("\xE1\xBB\x87", "e\xCC\x82\xCC\xA3", collationTertiary) == 0,
          "canonically equal texts equal");
    CollationSettings tertiary = mlocCollationDefaults(&s_root);
    CHECK(Compare("a\0b", 3, "ab", 2, &tertiary) == 0 && Order("a\xC2\xAD"
                                                               "b",
                                                               "ab", collationTertiary) == 0,
          "U+0000 and a soft hyphen completely ignorable");
    CHECK(Order("\xE4\xB8\x80", "\xE4\xB8\x81", collationPrimary) == -1 &&
              Order("z", "\xE4\xB8\x80", collationPrimary) == -1 &&
              Order("\xE4\xB8\x80", "\xCD\xB8", collationPrimary) == -1 &&
              Order("\xCD\xB8", "\xEF\xBF\xBD", collationPrimary) == -1,
          "Latin, then Han in order, then unassigned, then U+FFFD");
    CHECK(Order("ab", "a\xFF", collationPrimary) == -1, "a bad byte sorts as U+FFFD");
}

int main(void)
{
    FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocData data;
    if (size == 0 || size == sizeof(s_file) || mlocOpenData(&data, s_file, size) != mloc_success)
    {
        fputs("test_collation_compare: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    DataView view = mlocViewOf(&data);
    DataSpan span = {0};
    if (!mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) ||
        !mlocReadCollationRoot(span, &s_root))
    {
        fputs("test_collation_compare: no root unit\n", stderr);
        return 2;
    }
    TestLevels();
    TestTexts();
    TestShifted();
    TestOptions();
    TestNumeric();
    TestReorder();
    return s_failures == 0 ? 0 : 1;
}
