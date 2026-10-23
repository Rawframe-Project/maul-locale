// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for units: reads "locale unit width start [end]" lines
// (width short, narrow or long), prints ICU's text, then a tab and the
// unit fields joined by '|'. With an end, the range.
#include <iostream>
#include <sstream>
#include <string>
#include <unicode/measunit.h>
#include <unicode/numberformatter.h>
#include <unicode/numberrangeformatter.h>
using namespace icu;
template <typename R> static std::string Fields(const R& r, UErrorCode& s)
{
    UnicodeString str = r.toString(s);
    ConstrainedFieldPosition cfp;
    cfp.constrainField(UFIELD_CATEGORY_NUMBER, UNUM_MEASURE_UNIT_FIELD);
    std::string out;
    while (r.nextPosition(cfp, s))
    {
        std::string p;
        str.tempSubStringBetween(cfp.getStart(), cfp.getLimit()).toUTF8String(p);
        out += (out.empty() ? "" : "|") + p;
    }
    std::string text;
    str.toUTF8String(text);
    return text + "\t" + out;
}
int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream in(line);
        std::string loc, unit, width, a, b;
        in >> loc >> unit >> width >> a >> b;
        UErrorCode s = U_ZERO_ERROR;
        MeasureUnit u = MeasureUnit::forIdentifier(unit.c_str(), s);
        UNumberUnitWidth w = width == "long"     ? UNUM_UNIT_WIDTH_FULL_NAME
                             : width == "narrow" ? UNUM_UNIT_WIDTH_NARROW
                                                 : UNUM_UNIT_WIDTH_SHORT;
        auto nf =
            number::NumberFormatter::with().unit(u).unitWidth(w).roundingMode(UNUM_ROUND_HALFEVEN);
        std::string out;
        if (b.empty())
        {
            auto r = nf.locale(Locale(loc.c_str())).formatDecimal(StringPiece(a.c_str()), s);
            out = Fields(r, s);
        }
        else
        {
            auto rf = number::NumberRangeFormatter::with().numberFormatterBoth(nf).locale(
                Locale(loc.c_str()));
            auto r = rf.formatFormattableRange(Formattable(StringPiece(a.c_str()), s),
                                               Formattable(StringPiece(b.c_str()), s), s);
            out = Fields(r, s);
        }
        std::cout << line << "\t" << (U_FAILURE(s) ? std::string("ERR") : out) << "\n";
    }
}
