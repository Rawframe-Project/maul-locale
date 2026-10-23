// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for grammatical cases: reads "locale unit width case amount"
// lines (width short, narrow or long; case "-" for none) and prints the
// line, a tab, ICU's text, a tab and the noun class of the result.
#include <iostream>
#include <sstream>
#include <string>
#include <unicode/displayoptions.h>
#include <unicode/measunit.h>
#include <unicode/numberformatter.h>
using namespace icu;
static const char* NounClass(UDisplayOptionsNounClass c)
{
    switch (c)
    {
    case UDISPOPT_NOUN_CLASS_OTHER:
        return "other";
    case UDISPOPT_NOUN_CLASS_NEUTER:
        return "neuter";
    case UDISPOPT_NOUN_CLASS_FEMININE:
        return "feminine";
    case UDISPOPT_NOUN_CLASS_MASCULINE:
        return "masculine";
    case UDISPOPT_NOUN_CLASS_ANIMATE:
        return "animate";
    case UDISPOPT_NOUN_CLASS_INANIMATE:
        return "inanimate";
    case UDISPOPT_NOUN_CLASS_PERSONAL:
        return "personal";
    case UDISPOPT_NOUN_CLASS_COMMON:
        return "common";
    default:
        return "undefined";
    }
}
int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream in(line);
        std::string loc, unit, width, gcase, amount;
        in >> loc >> unit >> width >> gcase >> amount;
        UErrorCode s = U_ZERO_ERROR;
        MeasureUnit u = MeasureUnit::forIdentifier(unit.c_str(), s);
        UNumberUnitWidth w = width == "long"     ? UNUM_UNIT_WIDTH_FULL_NAME
                             : width == "narrow" ? UNUM_UNIT_WIDTH_NARROW
                                                 : UNUM_UNIT_WIDTH_SHORT;
        auto nf =
            number::NumberFormatter::with().unit(u).unitWidth(w).roundingMode(UNUM_ROUND_HALFEVEN);
        if (gcase != "-")
        {
            nf = nf.unitDisplayCase(StringPiece(gcase.c_str()));
        }
        auto r = nf.locale(Locale(loc.c_str())).formatDecimal(StringPiece(amount.c_str()), s);
        UnicodeString str = r.toString(s);
        UDisplayOptionsNounClass c = r.getNounClass(s);
        std::string text;
        str.toUTF8String(text);
        std::cout << line << "\t" << (U_FAILURE(s) ? std::string("ERR") : text) << "\t"
                  << (U_FAILURE(s) ? "" : NounClass(c)) << "\n";
    }
}
