// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation options against ICU4C's root: reads
// lines of two UTF-8 texts in hex, separated by a tab, and writes, for
// each line, ICU's order (-1, 0 or 1) under each of the option sets
// below, tab-separated. Normalization is on, as UCA orders a text's NFD
// (ICU's root default leaves text that is not FCD unreordered). The
// order is the sort keys', which UCA defines the order by: ICU 74's
// ucol_strcollUTF8 disagrees with its own keys when a common prefix ends
// in a shifted variable and a completely ignorable code point and a mark
// follows (".\u00AD" then U+0300), with normalization on or off. Built
// against the system's ICU:
//   g++ -O2 icu4c_collation_options.cpp -licui18n -licuuc -o icu_options
// The texts use only code points whose root order ICU 74 (UCA 15.1) and
// CLDR 49 (UCA 18) share. With --keys, writes instead the first text's
// sort key under each set, in hex, as conform_collation_options --keys
// writes ours; byte for byte that needs ICU4C 79, built on CLDR 49:
//   g++ -O2 -I$ICU/include icu4c_collation_options.cpp -L$ICU/lib \
//       -licui18n -licuuc -Wl,-rpath,$ICU/lib -o icu79_options

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unicode/ucol.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>
#include <vector>

// Ends a set's reorder codes (UCOL_REORDER_CODE_NONE is Zzzz, "others").
static const int32_t kEnd = -2;

struct Options
{
    UColAttributeValue strength;
    UColAttributeValue alternate;
    UColAttributeValue backwards;
    UColAttributeValue caseFirst;
    UColAttributeValue caseLevel;
    UColAttributeValue numeric;
    // Reorder codes, kEnd-ended (none: the root's order).
    int32_t reorder[5];
};

// The sets, in the order conform_collation_options prints them.
static const Options s_sets[] = {
    {UCOL_TERTIARY, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_OFF, UCOL_OFF, UCOL_OFF, {kEnd}},
    {UCOL_TERTIARY, UCOL_NON_IGNORABLE, UCOL_ON, UCOL_OFF, UCOL_OFF, UCOL_OFF, {kEnd}},
    {UCOL_TERTIARY, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_UPPER_FIRST, UCOL_OFF, UCOL_OFF, {kEnd}},
    {UCOL_TERTIARY, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_LOWER_FIRST, UCOL_OFF, UCOL_OFF, {kEnd}},
    {UCOL_PRIMARY, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_OFF, UCOL_ON, UCOL_OFF, {kEnd}},
    {UCOL_SECONDARY, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_UPPER_FIRST, UCOL_ON, UCOL_OFF, {kEnd}},
    {UCOL_TERTIARY, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_UPPER_FIRST, UCOL_ON, UCOL_OFF, {kEnd}},
    {UCOL_QUATERNARY, UCOL_SHIFTED, UCOL_OFF, UCOL_OFF, UCOL_OFF, UCOL_OFF, {kEnd}},
    {UCOL_QUATERNARY, UCOL_SHIFTED, UCOL_ON, UCOL_LOWER_FIRST, UCOL_OFF, UCOL_OFF, {kEnd}},
    {UCOL_IDENTICAL, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_OFF, UCOL_OFF, UCOL_OFF, {kEnd}},
    {UCOL_IDENTICAL, UCOL_SHIFTED, UCOL_ON, UCOL_UPPER_FIRST, UCOL_ON, UCOL_OFF, {kEnd}},
    {UCOL_TERTIARY, UCOL_NON_IGNORABLE, UCOL_OFF, UCOL_OFF, UCOL_OFF, UCOL_ON, {kEnd}},
    {UCOL_QUATERNARY, UCOL_SHIFTED, UCOL_OFF, UCOL_UPPER_FIRST, UCOL_OFF, UCOL_ON, {kEnd}},
    {UCOL_IDENTICAL, UCOL_NON_IGNORABLE, UCOL_ON, UCOL_OFF, UCOL_ON, UCOL_ON, {kEnd}},
    {UCOL_TERTIARY,
     UCOL_NON_IGNORABLE,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     {USCRIPT_GREEK, kEnd}},
    {UCOL_TERTIARY,
     UCOL_NON_IGNORABLE,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     {USCRIPT_LATIN, USCRIPT_HAN, kEnd}},
    {UCOL_TERTIARY,
     UCOL_NON_IGNORABLE,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_ON,
     {UCOL_REORDER_CODE_DIGIT, UCOL_REORDER_CODE_PUNCTUATION, kEnd}},
    {UCOL_TERTIARY,
     UCOL_NON_IGNORABLE,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     {USCRIPT_CYRILLIC, USCRIPT_UNKNOWN, USCRIPT_LATIN, kEnd}},
    {UCOL_QUATERNARY,
     UCOL_SHIFTED,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     UCOL_OFF,
     {UCOL_REORDER_CODE_SPACE, USCRIPT_HAN, USCRIPT_UNKNOWN, UCOL_REORDER_CODE_DIGIT, kEnd}},
};

static std::string Bytes(const char* hex, size_t n)
{
    std::string out;
    for (size_t i = 0; i + 1 < n; i += 2)
    {
        char pair[3] = {hex[i], hex[i + 1], 0};
        out.push_back(static_cast<char>(strtoul(pair, nullptr, 16)));
    }
    return out;
}

// The order of two texts' sort keys.
// A text's sort key, its final 00 included.
static std::vector<uint8_t> Key(const UCollator* c, const std::string& text, UErrorCode* status)
{
    std::vector<UChar> units(text.size() * 2 + 2);
    int32_t n = 0;
    u_strFromUTF8WithSub(units.data(), (int32_t)units.size(), &n, text.data(), (int32_t)text.size(),
                         0xFFFD, nullptr, status);
    std::vector<uint8_t> key(4096);
    int32_t m = ucol_getSortKey(c, units.data(), n, key.data(), (int32_t)key.size());
    if (m > (int32_t)key.size())
    {
        key.resize((size_t)m);
        m = ucol_getSortKey(c, units.data(), n, key.data(), (int32_t)key.size());
    }
    key.resize((size_t)m);
    return key;
}

static int Order(const UCollator* c, const std::string& a, const std::string& b, UErrorCode* status)
{
    std::vector<uint8_t> keys[2] = {Key(c, a, status), Key(c, b, status)};
    int r = memcmp(keys[0].data(), keys[1].data(), std::min(keys[0].size(), keys[1].size()));
    if (r == 0)
    {
        r = keys[0].size() < keys[1].size() ? -1 : keys[0].size() > keys[1].size() ? 1 : 0;
    }
    return r < 0 ? -1 : r > 0 ? 1 : 0;
}

int main(int argc, char** argv)
{
    bool keys = argc == 2 && strcmp(argv[1], "--keys") == 0;
    UErrorCode status = U_ZERO_ERROR;
    std::vector<UCollator*> collators;
    for (const Options& o : s_sets)
    {
        UCollator* c = ucol_open("", &status);
        ucol_setAttribute(c, UCOL_STRENGTH, o.strength, &status);
        ucol_setAttribute(c, UCOL_ALTERNATE_HANDLING, o.alternate, &status);
        ucol_setAttribute(c, UCOL_FRENCH_COLLATION, o.backwards, &status);
        ucol_setAttribute(c, UCOL_CASE_FIRST, o.caseFirst, &status);
        ucol_setAttribute(c, UCOL_CASE_LEVEL, o.caseLevel, &status);
        ucol_setAttribute(c, UCOL_NUMERIC_COLLATION, o.numeric, &status);
        int32_t codes = 0;
        while (o.reorder[codes] != kEnd)
        {
            codes++;
        }
        ucol_setReorderCodes(c, o.reorder, codes, &status);
        ucol_setAttribute(c, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
        collators.push_back(c);
    }
    if (U_FAILURE(status))
    {
        fprintf(stderr, "icu: %s\n", u_errorName(status));
        return 2;
    }
    static char line[131072];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        char* tab = strchr(line, '\t');
        if (tab == nullptr)
        {
            continue;
        }
        std::string a = Bytes(line, static_cast<size_t>(tab - line));
        std::string b = Bytes(tab + 1, strcspn(tab + 1, "\r\n"));
        for (size_t i = 0; i < collators.size(); i++)
        {
            if (keys)
            {
                printf(i == 0 ? "" : "\t");
                for (uint8_t byte : Key(collators[i], a, &status))
                {
                    printf("%02x", byte);
                }
                continue;
            }
            int order = Order(collators[i], a, b, &status);
            printf(i == 0 ? "%d" : "\t%d", order);
        }
        printf("\n");
    }
    return U_FAILURE(status) ? 2 : 0;
}
