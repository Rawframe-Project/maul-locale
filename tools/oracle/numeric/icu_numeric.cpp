// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lines "locale unit amount | h m s" (fields of the unit, "-" absent):
// ICU's MeasureFormat in the numeric width.
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <unicode/measfmt.h>
#include <unicode/measunit.h>
#include <unicode/measure.h>
#include <vector>
using namespace icu;
int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        size_t bar = line.find(" | ");
        std::istringstream in(line.substr(bar + 3));
        std::string loc = line.substr(0, line.find(' '));
        std::string f[3];
        in >> f[0] >> f[1] >> f[2];
        UErrorCode e = U_ZERO_ERROR;
        std::vector<Measure> ms;
        MeasureUnit* units[3] = {MeasureUnit::createHour(e), MeasureUnit::createMinute(e),
                                 MeasureUnit::createSecond(e)};
        for (int i = 0; i < 3; i++)
        {
            if (f[i] == "-")
            {
                delete units[i];
                continue;
            }
            ms.emplace_back(Formattable(f[i].c_str(), e), units[i], e);
        }
        LocalPointer<MeasureFormat> fmt(
            new MeasureFormat(Locale(loc.c_str()), UMEASFMT_WIDTH_NUMERIC, e));
        UnicodeString out;
        FieldPosition fp;
        fmt->formatMeasures(ms.data(), (int32_t)ms.size(), out, fp, e);
        std::string o;
        out.toUTF8String(o);
        printf("%s\t%s\n", line.substr(0, bar).c_str(), U_FAILURE(e) ? "ERR" : o.c_str());
    }
}
