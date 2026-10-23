// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lines "locale type width item|item|...": ICU's ListFormatter output.
#include <unicode/listformatter.h>
#include <unicode/unistr.h>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace icu;
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string loc, type, width, items;
        in >> loc >> type >> width;
        std::getline(in, items);
        items = items.substr(1);
        std::vector<UnicodeString> v;
        size_t at = 0;
        for (;;) {
            size_t bar = items.find('|', at);
            v.push_back(UnicodeString::fromUTF8(items.substr(at, bar == std::string::npos ? std::string::npos : bar - at)));
            if (bar == std::string::npos) break;
            at = bar + 1;
        }
        UListFormatterType t = type == "and" ? ULISTFMT_TYPE_AND : type == "or" ? ULISTFMT_TYPE_OR : ULISTFMT_TYPE_UNITS;
        UListFormatterWidth w = width == "wide" ? ULISTFMT_WIDTH_WIDE : width == "short" ? ULISTFMT_WIDTH_SHORT : ULISTFMT_WIDTH_NARROW;
        UErrorCode e = U_ZERO_ERROR;
        LocalPointer<ListFormatter> f(ListFormatter::createInstance(Locale(loc.c_str()), t, w, e));
        UnicodeString out;
        if (U_SUCCESS(e)) f->format(v.data(), (int32_t)v.size(), out, e);
        std::string s;
        out.toUTF8String(s);
        printf("%s\t%s\n", line.c_str(), U_FAILURE(e) ? "ERR" : s.c_str());
    }
}
