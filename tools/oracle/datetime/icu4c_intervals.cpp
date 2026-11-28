// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C's DateIntervalFormat answers for ranges.py: reads lines of a
// locale, a zone, a start and an end (seconds from 1970) and a skeleton,
// tab-separated, and writes for each ICU's text and its spans (each
// date's start and end in UTF-16 units, "-" for one value), tab-separated.
// A skeleton "date:N" or "time:N" is the fields, as written, of the
// locale's standard date or time pattern of that style (0 full to 3
// short), as tools/mlocgen/interval_gen.c's SkeletonOfPattern takes them.
//   g++ -O2 -std=c++17 -I$ICU/include icu4c_intervals.cpp -L$ICU/lib \
//       -licui18n -licuuc -licudata -Wl,-rpath,$ICU/lib -o icu_intervals
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unicode/datefmt.h>
#include <unicode/dtitvfmt.h>
#include <unicode/smpdtfmt.h>
#include <unicode/timezone.h>
#include <unicode/ustring.h>
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

// The fields of a pattern as written, as tools/mlocgen/skeleton.c's
// SkeletonParse keeps them: one run of letters for each of UTS #35's
// fields in ICU's order, a later run of a field replacing an earlier one
// ("rU" keeps U).
static UnicodeString SkeletonOfPattern(const UnicodeString& pattern)
{
    static const char16_t* const s_fields[] = {
        u"G", u"yYurU", u"Qq",  u"MLl",     u"w", u"W",  u"Ece", u"D",
        u"F", u"dg",    u"abB", u"hHkKjJC", u"m", u"sA", u"S",   u"zZOvVXx"};
    const int count = sizeof(s_fields) / sizeof(s_fields[0]);
    UnicodeString runs[count];
    bool quoted = false;
    for (int32_t i = 0; i < pattern.length();)
    {
        char16_t c = pattern.charAt(i);
        if (c == u'\'')
        {
            quoted = !quoted;
            i++;
            continue;
        }
        int32_t end = i + 1;
        while (!quoted && end < pattern.length() && pattern.charAt(end) == c)
        {
            end++;
        }
        for (int k = 0; !quoted && k < count; k++)
        {
            if (u_strchr(s_fields[k], c) != nullptr)
            {
                runs[k] = pattern.tempSubString(i, end - i);
                break;
            }
        }
        i = end;
    }
    UnicodeString out;
    for (const UnicodeString& run : runs)
    {
        out.append(run);
    }
    return out;
}

static UnicodeString Skeleton(const std::string& given, const Locale& locale)
{
    bool date = given.rfind("date:", 0) == 0;
    if (!date && given.rfind("time:", 0) != 0)
    {
        return UnicodeString::fromUTF8(given);
    }
    auto style = (DateFormat::EStyle)atoi(given.c_str() + 5);
    LocalPointer<DateFormat> format(date ? DateFormat::createDateInstance(style, locale)
                                         : DateFormat::createTimeInstance(style, locale));
    UnicodeString pattern;
    dynamic_cast<SimpleDateFormat*>(format.getAlias())->toPattern(pattern);
    return SkeletonOfPattern(pattern);
}

int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::vector<std::string> f = Split(line, '\t');
        UErrorCode e = U_ZERO_ERROR;
        Locale locale(f[0].c_str());
        LocalPointer<DateIntervalFormat> format(
            DateIntervalFormat::createInstance(Skeleton(f[4], locale), locale, e));
        if (U_FAILURE(e))
        {
            printf("error\t-\n");
            continue;
        }
        format->adoptTimeZone(TimeZone::createTimeZone(UnicodeString::fromUTF8(f[1])));
        DateInterval range(atof(f[2].c_str()) * 1000.0, atof(f[3].c_str()) * 1000.0);
        FormattedDateInterval value = format->formatToValue(range, e);
        UnicodeString text = value.toString(e);
        std::string spans;
        ConstrainedFieldPosition position;
        position.constrainCategory(UFIELD_CATEGORY_DATE_INTERVAL_SPAN);
        while (value.nextPosition(position, e))
        {
            char span[64];
            snprintf(span, sizeof(span), "%s%d:%d:%d", spans.empty() ? "" : ",",
                     position.getField(), position.getStart(), position.getLimit());
            spans += span;
        }
        std::string utf8;
        text.toUTF8String(utf8);
        printf("%s\t%s\n", U_FAILURE(e) ? "error" : utf8.c_str(),
               spans.empty() ? "-" : spans.c_str());
    }
    return 0;
}
