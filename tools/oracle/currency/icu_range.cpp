// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for currency ranges: "locale code display start end".
#include <iostream>
#include <sstream>
#include <string>
#include <unicode/currunit.h>
#include <unicode/numberformatter.h>
#include <unicode/numberrangeformatter.h>
using namespace icu;
int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        std::istringstream in(line);
        std::string loc, code, display, a, b;
        in >> loc >> code >> display >> a >> b;
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString ucode(code.c_str());
        CurrencyUnit unit(ucode.getTerminatedBuffer(), status);
        UNumberUnitWidth w = display == "narrow" ? UNUM_UNIT_WIDTH_NARROW
                             : display == "code" ? UNUM_UNIT_WIDTH_ISO_CODE
                             : display == "name" ? UNUM_UNIT_WIDTH_FULL_NAME
                                                 : UNUM_UNIT_WIDTH_SHORT;
        auto nf =
            number::NumberFormatter::with().unit(unit).unitWidth(w).roundingMode(UNUM_ROUND_HALFUP);
        auto rf = number::NumberRangeFormatter::with().numberFormatterBoth(nf).locale(
            Locale(loc.c_str()));
        Formattable fa(StringPiece(a.c_str()), status), fb(StringPiece(b.c_str()), status);
        auto r = rf.formatFormattableRange(fa, fb, status);
        std::string out;
        r.toTempString(status).toUTF8String(out);
        std::cout << line << "\t" << (U_FAILURE(status) ? std::string("ERR") : out) << "\n";
    }
}
