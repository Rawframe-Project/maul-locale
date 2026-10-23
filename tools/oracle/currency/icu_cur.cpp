// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for currencies: reads "locale code display sign usage value"
// lines (display: symbol narrow code formal variant hidden; sign: std or
// account; usage: std or cash), prints ICU's NumberFormatter text.
#include <unicode/numberformatter.h>
#include <unicode/currunit.h>
#include <iostream>
#include <sstream>
#include <string>
using namespace icu;
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string loc, code, display, sign, usage, value;
        in >> loc >> code >> display >> sign >> usage >> value;
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString ucode(code.c_str());
        CurrencyUnit unit(ucode.getTerminatedBuffer(), status);
        UNumberUnitWidth w = display == "narrow" ? UNUM_UNIT_WIDTH_NARROW
                           : display == "code" ? UNUM_UNIT_WIDTH_ISO_CODE
                           : display == "formal" ? UNUM_UNIT_WIDTH_FORMAL
                           : display == "variant" ? UNUM_UNIT_WIDTH_VARIANT
                           : display == "hidden" ? UNUM_UNIT_WIDTH_HIDDEN : display == "name" ? UNUM_UNIT_WIDTH_FULL_NAME
                           : UNUM_UNIT_WIDTH_SHORT;
        auto nf = number::NumberFormatter::withLocale(Locale(loc.c_str())).unit(unit).unitWidth(w).roundingMode(UNUM_ROUND_HALFUP);
        if (sign == "account") nf = nf.sign(UNUM_SIGN_ACCOUNTING);
        if (usage == "cash") nf = nf.precision(number::Precision::currency(UCURR_USAGE_CASH));
        auto r = nf.formatDecimal(StringPiece(value.c_str()), status);
        std::string out; r.toTempString(status).toUTF8String(out);
        std::cout << line << "\t" << (U_FAILURE(status) ? std::string("ERR") : out) << "\n";
    }
}
