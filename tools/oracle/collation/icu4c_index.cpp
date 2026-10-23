// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alphabetic index at run time
// against ICU4C's ImmutableIndex: reads lines of a locale tag, a
// maximum label count (0 for ICU4C's 99), 1 to set the underflow,
// inflow and overflow labels to "U", "I" and "O" (else 0), and names,
// each a name's code points in hex joined by "_", space-separated, all
// tab-separated. Writes the tag, a tab, each visible bucket as its type
// (0 normal, 1 underflow, 2 inflow, 3 overflow), ":" and its label's code
// points in hex joined by "_", space-separated; a tab and each name's
// bucket number, space-separated. "error" for a failure.
//   g++ -O2 -I$ICU/include icu4c_index.cpp -L$ICU/lib -licui18n -licuuc \
//       -licudata -o icu_index (run with LD_LIBRARY_PATH=$ICU/lib)
#include "unicode/alphaindex.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static icu::UnicodeString Text(const char* hex)
{
    icu::UnicodeString s;
    std::string h(hex);
    size_t at = 0;
    while (at < h.size()) {
        size_t next = h.find('_', at);
        std::string part = h.substr(at, next == std::string::npos ? std::string::npos : next - at);
        s.append(static_cast<UChar32>(strtoul(part.c_str(), nullptr, 16)));
        if (next == std::string::npos) { break; }
        at = next + 1;
    }
    return s;
}

static void Print(const icu::UnicodeString& s)
{
    for (int32_t i = 0; i < s.length(); i = s.moveIndex32(i, 1)) {
        printf(i == 0 ? "%X" : "_%X", (unsigned)s.char32At(i));
    }
}

int main()
{
    static char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = 0;
        char* field[4] = {line, nullptr, nullptr, nullptr};
        int n = 1;
        for (char* p = line; *p && n < 4; p++) {
            if (*p == '\t') { *p = 0; field[n++] = p + 1; }
        }
        if (n < 3) { printf("%s\terror\n", line); continue; }
        UErrorCode e = U_ZERO_ERROR;
        icu::Locale locale = icu::Locale::forLanguageTag(field[0], e);
        icu::AlphabeticIndex index(locale, e);
        int32_t max = atoi(field[1]);
        if (max > 0) { index.setMaxLabelCount(max, e); }
        if (atoi(field[2]) == 1) {
            index.setUnderflowLabel(u"U", e).setInflowLabel(u"I", e).setOverflowLabel(u"O", e);
        }
        icu::AlphabeticIndex::ImmutableIndex* immutable =
            U_SUCCESS(e) ? index.buildImmutableIndex(e) : nullptr;
        if (U_FAILURE(e) || immutable == nullptr) { printf("%s\terror\n", field[0]); continue; }
        printf("%s\t", field[0]);
        for (int32_t b = 0; b < immutable->getBucketCount(); b++) {
            const icu::AlphabeticIndex::Bucket* bucket = immutable->getBucket(b);
            printf("%s%d:", b == 0 ? "" : " ", (int)bucket->getLabelType());
            Print(bucket->getLabel());
        }
        printf("\t");
        if (n == 4) {
            char* save = nullptr;
            bool first = true;
            for (char* name = strtok_r(field[3], " ", &save); name; name = strtok_r(nullptr, " ", &save)) {
                printf("%s%d", first ? "" : " ", (int)immutable->getBucketIndex(Text(name), e));
                first = false;
            }
        }
        printf("\n");
        delete immutable;
    }
    return 0;
}
