// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for parsing with a given pattern: reads
// "pattern<TAB>mode<TAB>text" lines (mode lenient or strict, with
// "plus" after it for the plus sign shown); English symbols; prints the line,
// a tab, the exact decimal and the UTF-8 end, or "fail" and the offset.
#include <unicode/decimfmt.h>
#include <unicode/dcfmtsym.h>
#include <unicode/parsepos.h>
#include <iostream>
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
        size_t a = line.find('\t'), b = line.find('\t', a + 1);
        std::string pattern = line.substr(0, a), mode = line.substr(a + 1, b - a - 1), input = line.substr(b + 1);
        bool plus = mode.size() > 4 && mode.compare(mode.size() - 4, 4, "plus") == 0;
        UErrorCode e = U_ZERO_ERROR;
        auto* symbols = new DecimalFormatSymbols(Locale("en"), e);
        DecimalFormat df(UnicodeString::fromUTF8(pattern), symbols, e);
        if (U_FAILURE(e)) { std::cout << line << "\tERR\n"; continue; }
        df.setLenient(mode.compare(0, 6, "strict") != 0);
        df.setSignAlwaysShown(plus);
        UnicodeString text = UnicodeString::fromUTF8(input);
        Formattable result;
        ParsePosition pos(0);
        df.parse(text, result, pos);
        if (pos.getIndex() == 0) {
            std::cout << line << "\tfail\t" << Utf8Offset(text, pos.getErrorIndex() < 0 ? 0 : pos.getErrorIndex()) << "\n";
            continue;
        }
        StringPiece sp = result.getDecimalNumber(e);
        std::cout << line << "\t" << std::string(sp.data(), sp.size()) << "\t" << Utf8Offset(text, pos.getIndex()) << "\n";
    }
}
