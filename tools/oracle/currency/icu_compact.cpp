// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// "locale code display short|long value"
#include <iostream>
#include <sstream>
#include <unicode/currunit.h>
#include <unicode/numberformatter.h>
using namespace icu;
int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream in(line);
        std::string loc, code, display, len, v;
        in >> loc >> code >> display >> len >> v;
        UErrorCode s = U_ZERO_ERROR;
        UnicodeString uc(code.c_str());
        CurrencyUnit u(uc.getTerminatedBuffer(), s);
        UNumberUnitWidth w = display == "narrow"    ? UNUM_UNIT_WIDTH_NARROW
                             : display == "code"    ? UNUM_UNIT_WIDTH_ISO_CODE
                             : display == "hidden"  ? UNUM_UNIT_WIDTH_HIDDEN
                             : display == "formal"  ? UNUM_UNIT_WIDTH_FORMAL
                             : display == "variant" ? UNUM_UNIT_WIDTH_VARIANT
                                                    : UNUM_UNIT_WIDTH_SHORT;
        auto nf = number::NumberFormatter::withLocale(Locale(loc.c_str()))
                      .unit(u)
                      .unitWidth(w)
                      .roundingMode(UNUM_ROUND_HALFUP)
                      .notation(len == "long" ? number::Notation::compactLong()
                                              : number::Notation::compactShort());
        auto r = nf.formatDecimal(StringPiece(v.c_str()), s);
        std::string o;
        r.toTempString(s).toUTF8String(o);
        std::cout << line << "\t" << (U_FAILURE(s) ? "ERR" : o) << "\n";
    }
}
