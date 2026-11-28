// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C's interval patterns for test/intervals_icu.tsv: reads lines of a
// locale, a calendar and a skeleton, tab-separated, makes ICU's
// DateIntervalFormat for each and writes the line with the patterns it
// resolved per greatest difference (era, year, month, day, am or pm, hour,
// minute) as "first part|second part|later first" ("-" for an empty
// part), the fallback pattern and the skeleton's own pattern. The patterns
// are DateIntervalFormat's private fIntervalPatterns, which no public call
// gives, so this oracle alone reads them by making private members public.
//   g++ -O2 -std=c++17 -I$ICU/include icu4c_interval_patterns.cpp -L$ICU/lib \
//       -licui18n -licuuc -licudata -Wl,-rpath,$ICU/lib -o icu_interval_patterns
#define private   public
#define protected public
#include <unicode/dtitvfmt.h>
#include <unicode/dtitvinf.h>
#include <unicode/smpdtfmt.h>
#undef private
#undef protected

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
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

static std::string Part(const UnicodeString& text)
{
    std::string out;
    text.toUTF8String(out);
    return out.empty() ? "-" : out;
}

int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::vector<std::string> f = Split(line, '\t');
        UErrorCode e = U_ZERO_ERROR;
        Locale locale((f[0] + "@calendar=" + f[1]).c_str());
        LocalPointer<DateIntervalFormat> format(
            DateIntervalFormat::createInstance(UnicodeString::fromUTF8(f[2]), locale, e));
        if (U_FAILURE(e))
        {
            printf("%s\terror\n", line.c_str());
            continue;
        }
        printf("%s", line.c_str());
        for (int field = DateIntervalInfo::kIPI_ERA; field <= DateIntervalInfo::kIPI_MINUTE;
             field++)
        {
            const DateIntervalFormat::PatternInfo& p = format->fIntervalPatterns[field];
            printf("\t%s|%s|%d", Part(p.firstPart).c_str(), Part(p.secondPart).c_str(),
                   p.laterDateFirst ? 1 : 0);
        }
        UnicodeString fallback;
        format->fInfo->getFallbackIntervalPattern(fallback);
        UnicodeString pattern;
        format->fDateFormat->toPattern(pattern);
        printf("\t%s\t%s\n", Part(fallback).c_str(), Part(pattern).c_str());
    }
    return 0;
}
