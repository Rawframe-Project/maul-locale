// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C's answers for test/time_ranges_icu.tsv: reads lines of a locale,
// mlocTimeOptions' hour cycle, precision, style and zone (space-separated),
// a zone, a start and an end (seconds from 1970), tab-separated, and
// writes each line with ICU's DateIntervalFormat text and its spans
// (start and end of each date in UTF-16 units, "-" for one time). The
// skeleton is the one the interval generator names the options by
// (tools/mlocgen/interval_gen.c): h or H by the clock, then nothing, m or
// ms by the precision, then the zone form's letters; a standard style
// takes the fields of the locale's time pattern of that length as written.
//   g++ -O2 -std=c++17 -I$ICU/include icu4c_time_ranges.cpp -L$ICU/lib \
//       -licui18n -licuuc -licudata -Wl,-rpath,$ICU/lib -o icu_time_ranges
#include <unicode/datefmt.h>
#include <unicode/dtitvfmt.h>
#include <unicode/dtptngen.h>
#include <unicode/smpdtfmt.h>
#include <unicode/timezone.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// The fields of a pattern as written, in UTS #35's order: the skeleton
// interval_gen.c's SkeletonOfPattern gives a standard format.
static UnicodeString SkeletonOfPattern(const UnicodeString& pattern)
{
    static const char16_t s_order[] = u"GyYuUrQqMLlwWEcedDFgabBhHkKjJCmsSAzZOvVXx";
    UnicodeString runs[sizeof(s_order) / sizeof(s_order[0])];
    bool quoted = false;
    for (int32_t i = 0; i < pattern.length(); i++)
    {
        char16_t c = pattern.charAt(i);
        if (c == u'\'')
        {
            quoted = !quoted;
            continue;
        }
        if (quoted)
        {
            continue;
        }
        for (size_t k = 0; s_order[k] != 0; k++)
        {
            if (s_order[k] == c)
            {
                runs[k].append(c);
                break;
            }
        }
    }
    UnicodeString out;
    for (const UnicodeString& run : runs)
    {
        out.append(run);
    }
    return out;
}

static UnicodeString TimeSkeleton(int hourCycle, int precision, int zone)
{
    static const char16_t* const s_zones[9] = {u"",     u"zzzz", u"z",    u"vvvv", u"v",
                                               u"VVVV", u"VVV",  u"OOOO", u"O"};
    static const char16_t* const s_precisions[3] = {u"ms", u"", u"m"};
    UnicodeString s(hourCycle == 6 ? u"H" : u"h");
    s.append(s_precisions[precision]);
    s.append(s_zones[zone]);
    return s;
}

int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::vector<std::string> f = Split(line, '\t');
        std::vector<std::string> o = Split(f[1], ' ');
        int hourCycle = atoi(o[0].c_str());
        int precision = atoi(o[1].c_str());
        int style = atoi(o[2].c_str());
        int zone = atoi(o[3].c_str());
        UErrorCode e = U_ZERO_ERROR;
        Locale locale(f[0].c_str());
        UnicodeString skeleton;
        if (style != 0)
        {
            LocalPointer<DateFormat> time(
                DateFormat::createTimeInstance((DateFormat::EStyle)(style - 1), locale));
            UnicodeString pattern;
            dynamic_cast<SimpleDateFormat*>(time.getAlias())->toPattern(pattern);
            skeleton = SkeletonOfPattern(pattern);
        }
        else
        {
            skeleton = TimeSkeleton(hourCycle, precision, zone);
        }
        LocalPointer<DateIntervalFormat> format(DateIntervalFormat::createInstance(skeleton, locale, e));
        if (U_FAILURE(e))
        {
            printf("%s\t%s\t%s\t%s\t%s\terror\t-\n", f[0].c_str(), f[1].c_str(), f[2].c_str(),
                   f[3].c_str(), f[4].c_str());
            continue;
        }
        format->adoptTimeZone(TimeZone::createTimeZone(UnicodeString::fromUTF8(f[2])));
        DateInterval range(atof(f[3].c_str()) * 1000.0, atof(f[4].c_str()) * 1000.0);
        FormattedDateInterval value = format->formatToValue(range, e);
        UnicodeString text = value.toString(e);
        std::string spans;
        ConstrainedFieldPosition position;
        position.constrainCategory(UFIELD_CATEGORY_DATE_INTERVAL_SPAN);
        while (value.nextPosition(position, e))
        {
            char span[64];
            snprintf(span, sizeof(span), "%s%d:%d:%d", spans.empty() ? "" : ",", position.getField(),
                     position.getStart(), position.getLimit());
            spans += span;
        }
        std::string utf8;
        text.toUTF8String(utf8);
        printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\n", f[0].c_str(), f[1].c_str(), f[2].c_str(), f[3].c_str(),
               f[4].c_str(), U_FAILURE(e) ? "error" : utf8.c_str(), spans.empty() ? "-" : spans.c_str());
    }
    return 0;
}
