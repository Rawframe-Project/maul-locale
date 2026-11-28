// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C's best patterns for test/skeletons_icu.tsv: reads lines of a
// locale, a calendar and a skeleton, tab-separated, and writes each line
// with the pattern ICU's DateTimePatternGenerator gives for the skeleton
// in that locale and calendar.
//   g++ -O2 -std=c++17 -I$ICU/include icu4c_skeletons.cpp -L$ICU/lib \
//       -licui18n -licuuc -licudata -Wl,-rpath,$ICU/lib -o icu_skeletons
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unicode/dtptngen.h>
#include <vector>

using namespace icu;

static std::vector<std::string> Split(const std::string& s, char c)
{
    std::vector<std::string> out;
    std::string part;
    std::istringstream in(s);
    while (std::getline(in, part, c))
    {
        out.push_back(part);
    }
    return out;
}

int main()
{
    // One generator per locale and calendar; making one reads all its data.
    std::map<std::string, std::unique_ptr<DateTimePatternGenerator>> generators;
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::vector<std::string> f = Split(line, '\t');
        std::string id = f[0] + "@calendar=" + f[1];
        UErrorCode e = U_ZERO_ERROR;
        std::unique_ptr<DateTimePatternGenerator>& generator = generators[id];
        if (!generator)
        {
            generator.reset(DateTimePatternGenerator::createInstance(Locale(id.c_str()), e));
        }
        UnicodeString pattern;
        if (U_SUCCESS(e))
        {
            pattern = generator->getBestPattern(UnicodeString::fromUTF8(f[2]), e);
        }
        std::string utf8;
        pattern.toUTF8String(utf8);
        printf("%s\t%s\n", line.c_str(), U_FAILURE(e) ? "error" : utf8.c_str());
    }
    return 0;
}
