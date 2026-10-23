// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lines "locale unit usage width value": ICU's text with usage.
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <unicode/measunit.h>
#include <unicode/numberformatter.h>
#include <unicode/numberrangeformatter.h>
using namespace icu;
using namespace icu::number;
int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream in(line);
        std::string loc, unit, usage, width, value, value2;
        in >> loc >> unit >> usage >> width >> value >> value2;
        UErrorCode e = U_ZERO_ERROR;
        UNumberUnitWidth w = width == "long"     ? UNUM_UNIT_WIDTH_FULL_NAME
                             : width == "narrow" ? UNUM_UNIT_WIDTH_NARROW
                                                 : UNUM_UNIT_WIDTH_SHORT;
        std::string tag = loc;
        for (auto& c : tag)
        {
            if (c == '_')
            {
                c = '-';
            }
        }
        UErrorCode le = U_ZERO_ERROR;
        Locale L = Locale::forLanguageTag(tag.c_str(), le);
        auto f = NumberFormatter::withLocale(L)
                     .unit(MeasureUnit::forIdentifier(unit.c_str(), e))
                     .usage(usage.c_str())
                     .unitWidth(w);
        UnicodeString s;
        if (value2.empty())
        {
            auto r = f.formatDecimal(value.c_str(), e);
            s = r.toString(e);
        }
        else
        {
            auto rf = NumberRangeFormatter::withLocale(L).numberFormatterBoth(
                NumberFormatter::with()
                    .unit(MeasureUnit::forIdentifier(unit.c_str(), e))
                    .usage(usage.c_str())
                    .unitWidth(w));
            auto r = rf.formatFormattableRange(Formattable(value.c_str(), e),
                                               Formattable(value2.c_str(), e), e);
            s = r.toString(e);
        }
        std::string o;
        s.toUTF8String(o);
        printf("%s\t%s\n", line.c_str(), U_FAILURE(e) ? "ERR" : o.c_str());
        fflush(stdout);
    }
}
