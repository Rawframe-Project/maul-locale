// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The index labels in the data
// against ICU4C's: reads a locale tag a line and writes the set
// AlphabeticIndex::addIndexExemplars takes from the locale, by the same
// calls: ulocdata's index exemplars, else its standard exemplars with a
// to z added, Hangul and Ethiopic cut, each item upper-cased by the
// locale. A line is the tag, then each label's code points in hex
// joined by "_", the labels in code point order, space-separated; then a
// tab and "fdd0" when the locale's collator has U+FDD0 contractions,
// which AlphabeticIndex takes instead of the set, and a tab and "from"
// with the bundle ICU4C read when it has none of the locale's own (its
// fallback, by truncation or to root).
//   g++ -O2 -I$ICU/include icu4c_index_labels.cpp -L$ICU/lib -licui18n \
//       -licuuc -licudata -o icu_index_labels (run with LD_LIBRARY_PATH=$ICU/lib)
#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/ulocdata.h"
#include "unicode/uniset.h"
#include "unicode/ures.h"
#include "unicode/usetiter.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static icu::UnicodeSet Labels(const icu::Locale& locale, UErrorCode& e)
{
    icu::UnicodeSet exemplars;
    ULocaleData* uld = ulocdata_open(locale.getName(), &e);
    if (U_FAILURE(e))
    {
        return exemplars;
    }
    ulocdata_getExemplarSet(uld, exemplars.toUSet(), 0, ULOCDATA_ES_INDEX, &e);
    if (U_SUCCESS(e))
    {
        ulocdata_close(uld);
        return exemplars;
    }
    e = U_ZERO_ERROR;
    ulocdata_getExemplarSet(uld, exemplars.toUSet(), 0, ULOCDATA_ES_STANDARD, &e);
    ulocdata_close(uld);
    if (U_FAILURE(e))
    {
        return exemplars;
    }
    if (exemplars.containsSome(0x61, 0x7A) || exemplars.isEmpty())
    {
        exemplars.add(0x61, 0x7A);
    }
    if (exemplars.containsSome(0xAC00, 0xD7A3))
    {
        exemplars.remove(0xAC00, 0xD7A3)
            .add(0xAC00)
            .add(0xB098)
            .add(0xB2E4)
            .add(0xB77C)
            .add(0xB9C8)
            .add(0xBC14)
            .add(0xC0AC)
            .add(0xC544)
            .add(0xC790)
            .add(0xCC28)
            .add(0xCE74)
            .add(0xD0C0)
            .add(0xD30C)
            .add(0xD558);
    }
    if (exemplars.containsSome(0x1200, 0x137F))
    {
        icu::UnicodeSet ethiopic(
            icu::UnicodeString(u"[ሀለሐመሠረሰሸቀቈቐቘበቨተቸኀኈነኘአከኰኸዀወዐዘዠየደዸጀገጐጘጠጨጰጸፀፈፐፘ]"), e);
        ethiopic.retainAll(exemplars);
        exemplars.remove(u'ሀ', 0x137F).addAll(ethiopic);
    }
    icu::UnicodeSet upper;
    icu::UnicodeSetIterator it(exemplars);
    while (it.next())
    {
        icu::UnicodeString s = it.getString();
        s.toUpper(locale);
        upper.add(s);
    }
    return upper;
}

int main()
{
    static char line[1024];
    while (fgets(line, sizeof line, stdin))
    {
        line[strcspn(line, "\n")] = 0;
        UErrorCode e = U_ZERO_ERROR;
        icu::Locale locale = icu::Locale::forLanguageTag(line, e);
        icu::UnicodeSet set = U_SUCCESS(e) ? Labels(locale, e) : icu::UnicodeSet();
        if (U_FAILURE(e))
        {
            printf("%s\terror\n", line);
            continue;
        }
        std::vector<std::u32string> labels;
        icu::UnicodeSetIterator it(set);
        while (it.next())
        {
            const icu::UnicodeString& s = it.getString();
            std::u32string cps;
            for (int32_t i = 0; i < s.length(); i = s.moveIndex32(i, 1))
            {
                cps += (char32_t)s.char32At(i);
            }
            labels.push_back(cps);
        }
        std::sort(labels.begin(), labels.end());
        printf("%s\t", line);
        for (size_t i = 0; i < labels.size(); i++)
        {
            printf("%s", i == 0 ? "" : " ");
            for (size_t j = 0; j < labels[i].size(); j++)
            {
                printf(j == 0 ? "%X" : "_%X", (unsigned)labels[i][j]);
            }
        }
        e = U_ZERO_ERROR;
        icu::Collator* c = icu::Collator::createInstance(locale, e);
        auto* rbc = dynamic_cast<icu::RuleBasedCollator*>(c);
        icu::UnicodeSet contractions;
        if (U_SUCCESS(e) && rbc != nullptr)
        {
            rbc->internalAddContractions(0xFDD0, contractions, e);
        }
        printf("%s", U_SUCCESS(e) && !contractions.isEmpty() ? "\tfdd0" : "");
        delete c;
        e = U_ZERO_ERROR;
        UResourceBundle* bundle = ures_open(nullptr, locale.getName(), &e);
        const char* actual =
            U_SUCCESS(e) ? ures_getLocaleByType(bundle, ULOC_ACTUAL_LOCALE, &e) : "";
        const char* asked = locale.getName()[0] == 0 ? "root" : locale.getName();
        if (U_SUCCESS(e) && strcmp(actual, asked) != 0)
        {
            printf("\tfrom %s", actual);
        }
        printf("\n");
        ures_close(bundle);
    }
    return 0;
}
