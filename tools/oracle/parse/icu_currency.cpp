// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for currency parsing: reads
// "locale<TAB>code<TAB>mode<TAB>any<TAB>text" lines (mode lenient or
// strict; any 0 for DecimalFormat::parse, 1 for parseCurrency) with
// NumberFormat::createInstance(UNUM_CURRENCY) and setCurrency(code);
// prints the line, a tab, the exact decimal, the currency (or "-") and
// the UTF-8 end, or "fail" and the offset.
#include <unicode/decimfmt.h>
#include <unicode/curramt.h>
#include <unicode/numfmt.h>
#include <unicode/parsepos.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
using namespace icu;
static int32_t Utf8Offset(const UnicodeString& s, int32_t utf16) {
    std::string prefix;
    s.tempSubString(0, utf16).toUTF8String(prefix);
    return static_cast<int32_t>(prefix.size());
}
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string f[5];
        size_t at = 0;
        for (int i = 0; i < 5; i++) {
            size_t tab = i < 4 ? line.find('\t', at) : std::string::npos;
            f[i] = line.substr(at, tab == std::string::npos ? std::string::npos : tab - at);
            at = tab == std::string::npos ? line.size() : tab + 1;
        }
        UErrorCode e = U_ZERO_ERROR;
        std::unique_ptr<NumberFormat> nf(NumberFormat::createInstance(Locale(f[0].c_str()), UNUM_CURRENCY, e));
        if (U_FAILURE(e)) { std::cout << line << "\tERR\n"; continue; }
        UnicodeString code = UnicodeString::fromUTF8(f[1]);
        nf->setCurrency(code.getTerminatedBuffer(), e);
        nf->setLenient(f[2] != "strict");
        UnicodeString text = UnicodeString::fromUTF8(f[4]);
        ParsePosition pos(0);
        std::string value, currency = "-";
        if (f[3] == "1") {
            std::unique_ptr<CurrencyAmount> amount(nf->parseCurrency(text, pos));
            if (amount == nullptr) {
                std::cout << line << "\tfail\t" << Utf8Offset(text, pos.getErrorIndex() < 0 ? 0 : pos.getErrorIndex()) << "\n";
                continue;
            }
            Formattable number(amount->getNumber());
            StringPiece sp = number.getDecimalNumber(e);
            value = std::string(sp.data(), sp.size());
            if (number.getType() == Formattable::kDouble && number.getDouble() == 0 &&
                std::signbit(number.getDouble())) {
                value = "-0";
            }
            currency.clear();
            UnicodeString(amount->getISOCurrency()).toUTF8String(currency);
        } else {
            Formattable result;
            nf->parse(text, result, pos);
            if (pos.getIndex() == 0) {
                std::cout << line << "\tfail\t" << Utf8Offset(text, pos.getErrorIndex() < 0 ? 0 : pos.getErrorIndex()) << "\n";
                continue;
            }
            StringPiece sp = result.getDecimalNumber(e);
            value = std::string(sp.data(), sp.size());
            if (result.getType() == Formattable::kDouble && result.getDouble() == 0 &&
                std::signbit(result.getDouble())) {
                value = "-0";
            }
        }
        std::cout << line << "\t" << value << "\t" << currency << "\t" << Utf8Offset(text, pos.getIndex()) << "\n";
    }
}
