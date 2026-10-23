// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for parsing: reads "locale<TAB>style<TAB>mode<TAB>text"
// lines (style decimal, percent, permille, scientific, plus; mode lenient
// or strict) and
// prints the line, a tab, then the value ICU parses and the UTF-8 byte
// offset its parse ended at, or "fail" and the offset parsing failed at.
#include <unicode/decimfmt.h>
#include <unicode/numfmt.h>
#include <unicode/parsepos.h>
#include <cmath>
#include <iostream>
#include <sstream>
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
        std::string fields[4];
        size_t at = 0;
        for (int i = 0; i < 4; i++) {
            size_t tab = i < 3 ? line.find('\t', at) : std::string::npos;
            fields[i] = line.substr(at, tab == std::string::npos ? std::string::npos : tab - at);
            at = tab == std::string::npos ? line.size() : tab + 1;
        }
        UErrorCode e = U_ZERO_ERROR;
        Locale locale(fields[0].c_str());
        bool percent = fields[1] == "percent" || fields[1] == "permille";
        LocalPointer<NumberFormat> nf(
            percent ? NumberFormat::createPercentInstance(locale, e)
            : fields[1] == "scientific" ? NumberFormat::createScientificInstance(locale, e)
                                        : NumberFormat::createInstance(locale, e));
        if (U_FAILURE(e)) { std::cout << line << "\tERR\n"; continue; }
        auto* df = dynamic_cast<DecimalFormat*>(nf.getAlias());
        if (fields[1] == "permille" && df != nullptr) {
            // The per mille style: the percent pattern with the per mille
            // sign, whose multiplier is then 1000.
            UnicodeString pattern;
            df->toPattern(pattern);
            pattern.findAndReplace(u"%", u"\u2030");
            df->applyPattern(pattern, e);
        }
        if (fields[1] == "plus" && df != nullptr) {
            df->setSignAlwaysShown(true);
        }
        nf->setLenient(fields[2] != "strict");
        UnicodeString text = UnicodeString::fromUTF8(fields[3]);
        Formattable result;
        ParsePosition pos(0);
        nf->parse(text, result, pos);
        if (pos.getIndex() == 0) {
            std::cout << line << "\tfail\t" << Utf8Offset(text, pos.getErrorIndex() < 0 ? 0 : pos.getErrorIndex()) << "\n";
            continue;
        }
        std::string value;
        // The decimal quantity ICU parsed, which its double would saturate
        // past 1.8e308; NaN and the infinities ICU gives as doubles alone.
        StringPiece exact = result.getDecimalNumber(e);
        std::string exactText = U_FAILURE(e) ? "" : std::string(exact.data(), exact.size());
        e = U_ZERO_ERROR;
        bool special = result.getType() == Formattable::kDouble &&
                       (std::isnan(result.getDouble()) || std::isinf(result.getDouble()));
        if (special && std::isnan(result.getDouble())) {
            value = "NaN";
        } else if (special && exactText.find_first_of("0123456789") != std::string::npos) {
            value = exactText;
        } else if (special) {
            value = result.getDouble() < 0 ? "-Infinity" : "Infinity";
        } else if (result.getType() == Formattable::kDouble && result.getDouble() == 0 &&
                   std::signbit(result.getDouble()) &&
                   exactText.find_first_of("123456789") == std::string::npos) {
            value = "-0";
        } else {
            StringPiece sp = result.getDecimalNumber(e);
            value = U_FAILURE(e) ? "ERR" : std::string(sp.data(), sp.size());
        }
        std::cout << line << "\t" << value << "\t" << Utf8Offset(text, pos.getIndex()) << "\n";
    }
}
