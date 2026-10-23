// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C oracle for unit display names: "locale unit width" lines (short,
// narrow or long), prints MeasureFormat's display name.
#include <unicode/measfmt.h>
#include <unicode/measunit.h>
#include <iostream>
#include <sstream>
#include <string>
using namespace icu;
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string loc, unit, width;
        in >> loc >> unit >> width;
        UErrorCode s = U_ZERO_ERROR;
        MeasureUnit u = MeasureUnit::forIdentifier(unit.c_str(), s);
        UMeasureFormatWidth w = width == "long" ? UMEASFMT_WIDTH_WIDE
                              : width == "narrow" ? UMEASFMT_WIDTH_NARROW : UMEASFMT_WIDTH_SHORT;
        MeasureFormat mf(Locale(loc.c_str()), w, s);
        UnicodeString name = mf.getUnitDisplayName(u, s);
        std::string out;
        name.toUTF8String(out);
        std::cout << line << "\t" << (U_FAILURE(s) ? std::string("ERR") : out) << "\n";
    }
}
