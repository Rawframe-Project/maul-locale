// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_text.h"

#include "maul-unicode/normalize.h"

uint32_t mlocMessageDecode(const uint8_t* text, size_t at, size_t end, size_t* sizeOut)
{
    uint32_t lead = text[at];
    if (lead < 0x80 || at + 1 >= end)
    {
        *sizeOut = 1;
        return lead;
    }
    if (lead < 0xE0)
    {
        *sizeOut = 2;
        return (lead & 0x1F) << 6 | (text[at + 1] & 0x3Fu);
    }
    if (lead < 0xF0)
    {
        *sizeOut = 3;
        return (lead & 0x0F) << 12 | (text[at + 1] & 0x3Fu) << 6 | (text[at + 2] & 0x3Fu);
    }
    *sizeOut = 4;
    return (lead & 0x07) << 18 | (text[at + 1] & 0x3Fu) << 12 | (text[at + 2] & 0x3Fu) << 6 |
           (text[at + 3] & 0x3Fu);
}

bool mlocMessageIsSpace(uint32_t c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0x3000;
}

bool mlocMessageIsBidi(uint32_t c)
{
    return c == 0x061C || c == 0x200E || c == 0x200F || (c >= 0x2066 && c <= 0x2069);
}

bool mlocMessageIsNameStart(uint32_t c)
{
    if (c < 0x80)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' || c == '_';
    }
    if (c < 0xA1 || c == 0x061C || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x200E ||
        c == 0x200F || (c >= 0x2028 && c <= 0x202F) || c == 0x205F ||
        (c >= 0x2066 && c <= 0x2069) || c == 0x3000)
    {
        return false;
    }
    if ((c >= 0xD800 && c <= 0xDFFF) || (c >= 0xFDD0 && c <= 0xFDEF))
    {
        return false;
    }
    // The last two code points of every plane are noncharacters.
    return (c & 0xFFFE) != 0xFFFE && c <= 0x10FFFD;
}

bool mlocMessageIsNameChar(uint32_t c)
{
    return mlocMessageIsNameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

size_t mlocMessageNfcNameBreak(const uint8_t* name, size_t length)
{
    for (size_t at = 0; at < length;)
    {
        size_t size;
        uint32_t c = mlocMessageDecode(name, at, length, &size);
        muniQuickCheck answer = muni_quickCheckYes;
        if (c >= 0x80 &&
            muniCheckNormalization((const char*)name + at, size, muni_nfc, &answer).status ==
                muni_success &&
            answer != muni_quickCheckYes)
        {
            // A code point's NFC is a few code points at most.
            uint8_t normal[32];
            size_t needed = 0;
            if (muniNormalize((const char*)name + at, size, muni_nfc, muni_convertStrict,
                              (char*)normal, sizeof normal, &needed)
                    .status != muni_success)
            {
                return at;
            }
            for (size_t i = 0; i < needed;)
            {
                size_t part;
                uint32_t d = mlocMessageDecode(normal, i, needed, &part);
                bool first = at == 0 && i == 0;
                if (first ? !mlocMessageIsNameStart(d) : !mlocMessageIsNameChar(d))
                {
                    return at;
                }
                i += part;
            }
        }
        at += size;
    }
    return length;
}
