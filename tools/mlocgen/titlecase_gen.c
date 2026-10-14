// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A titlecase table (titlecase_gen.h).

#include "titlecase_gen.h"

#include "spellout_tables.h"
#include "table_gen.h"

#include "maul-unicode/case.h"
#include "maul-unicode/properties.h"

#include <stdlib.h>
#include <string.h>

enum
{
    maxCodePoint = 0x10FFFF,
};

void TitlecaseNote(Titlecase* t, const char* text, size_t length, bool firstOnly)
{
    for (size_t at = 0; at < length;)
    {
        uint32_t cp = 0;
        size_t size = 0;
        if (muniDecodeUtf8(text + at, length - at, &cp, &size) == muni_success &&
            muniGetGeneralCategory(cp) == muni_gcLl)
        {
            t->lowercase[cp / 8] |= (uint8_t)(1u << (cp % 8));
        }
        at = firstOnly ? length : at + size;
    }
}

void TitlecaseAdd(const Titlecase* t, uint32_t kind, DataFile* file)
{
    // A letter's full titlecase is at most three code points.
    uint32_t count = 0;
    for (uint32_t cp = 0; cp <= maxCodePoint; cp++)
    {
        count += (t->lowercase[cp / 8] >> (cp % 8)) & 1u;
    }
    uint8_t* records =
        GenAllocate(spelloutTitlecaseHeaderSize + (size_t)count * spelloutTitlecaseSize);
    uint8_t* text = GenAllocate((size_t)count * 16 + 1);
    size_t textSize = 0;
    uint32_t kept = 0;
    for (uint32_t cp = 0; cp <= maxCodePoint; cp++)
    {
        if ((t->lowercase[cp / 8] & (1u << (cp % 8))) == 0)
        {
            continue;
        }
        char letter[4];
        char title[16];
        size_t length = 0;
        size_t needed = 0;
        if (muniEncodeUtf8(cp, letter, &length) != muni_success ||
            muniConvertCase(letter, length, muni_caseTitle, muni_caseDefault, muni_convertStrict,
                            title, sizeof(title), &needed)
                    .status != muni_success)
        {
            GenDie("a titlecase mapping failed", "");
        }
        if (needed == length && memcmp(letter, title, length) == 0)
        {
            continue;
        }
        uint8_t* record =
            records + spelloutTitlecaseHeaderSize + (size_t)kept * spelloutTitlecaseSize;
        GenPut(record, cp, 4);
        GenPut(record + 4, textSize, 2);
        record[6] = (uint8_t)needed;
        record[7] = 0;
        memcpy(text + textSize, title, needed);
        textSize += needed;
        kept++;
    }
    if (kept > 0xFFFF || textSize > spelloutMaxText)
    {
        GenDie("the titlecase table passes its offsets", "");
    }
    size_t recordsSize = spelloutTitlecaseHeaderSize + (size_t)kept * spelloutTitlecaseSize;
    GenPut(records, kept, 2);
    GenPut(records + 2, textSize, 2);
    uint8_t* unit = GenAllocate(recordsSize + textSize);
    memcpy(unit, records, recordsSize);
    memcpy(unit + recordsSize, text, textSize);
    DataFileAdd(file, kind, nullptr, unit, recordsSize + textSize);
    free(unit);
    free(records);
    free(text);
}
