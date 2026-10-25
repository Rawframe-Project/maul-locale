// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lines "locale<TAB>kind<TAB>text" (kind word, sentence or line, the text
// UTF-8 with \n for a line feed): ICU4C's BreakIterator boundaries after
// the start of the text, as UTF-8 byte offsets joined by commas, after a
// tab. The locale is a BCP 47 tag, so its -u-ss and -u-lb keys reach
// createSentenceInstance and createLineInstance as they would a program's.
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/unistr.h>
#include <vector>
using namespace icu;

static std::string Unescape(const std::string& text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == 'n')
        {
            out += '\n';
            i++;
        }
        else
        {
            out += text[i];
        }
    }
    return out;
}

int main()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        size_t tab = line.find('\t');
        size_t tab2 = line.find('\t', tab + 1);
        std::string tag = line.substr(0, tab);
        std::string kind = line.substr(tab + 1, tab2 - tab - 1);
        std::string text = Unescape(line.substr(tab2 + 1));
        UErrorCode status = U_ZERO_ERROR;
        Locale locale = Locale::forLanguageTag(tag, status);
        std::unique_ptr<BreakIterator> b(
            kind == "word"       ? BreakIterator::createWordInstance(locale, status)
            : kind == "sentence" ? BreakIterator::createSentenceInstance(locale, status)
                                 : BreakIterator::createLineInstance(locale, status));
        if (U_FAILURE(status))
        {
            std::printf("%s\tERROR %s\n", line.c_str(), u_errorName(status));
            continue;
        }
        UnicodeString u = UnicodeString::fromUTF8(text);
        b->setText(u);
        // UTF-8 byte offset of each UTF-16 index.
        std::vector<size_t> bytes(u.length() + 1, 0);
        size_t at = 0;
        for (int32_t i = 0; i < u.length();)
        {
            UChar32 c = u.char32At(i);
            int32_t units = U16_LENGTH(c);
            for (int32_t k = 0; k < units; k++)
            {
                bytes[i + k] = at;
            }
            at += c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
            i += units;
        }
        bytes[u.length()] = at;
        std::string answer;
        b->first();
        for (int32_t p = b->next(); p != BreakIterator::DONE; p = b->next())
        {
            answer += (answer.empty() ? "" : ",") + std::to_string(bytes[p]);
        }
        std::printf("%s\t%s\n", line.c_str(), answer.c_str());
    }
    return 0;
}
