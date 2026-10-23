// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin tables against ICU4C's: reads
// conform_collation_fast's output (a tag first on each line), makes
// ICU's collator for each tag and writes its table as the tool writes
// the library's: the tag, a tab and the locale and type ICU's loader
// took (its actual locale), then the count and the words in hex. The
// table is a private member of RuleBasedCollator, read here by a define
// as only an oracle may; the internal headers come from ICU's sources.
//   g++ -O2 -I$ICU/include -I$ICU_SRC/common -I$ICU_SRC/i18n \
//       icu4c_collation_fast_latin.cpp -L$ICU/lib -licui18n -licuuc \
//       -licudata -Wl,-rpath,$ICU/lib -o icu_fast_latin
#include "collationdata.h"
#define private public
#include "unicode/tblcoll.h"
#undef private
#include "unicode/coll.h"

#include <cstdio>
#include <cstring>
int main()
{
    char line[1 << 16];
    while (fgets(line, sizeof line, stdin))
    {
        line[strcspn(line, " \n")] = 0;
        UErrorCode e = U_ZERO_ERROR;
        icu::Locale loc = icu::Locale::forLanguageTag(line, e);
        icu::Collator* c = U_SUCCESS(e) ? icu::Collator::createInstance(loc, e) : nullptr;
        auto* r = U_SUCCESS(e) ? dynamic_cast<icu::RuleBasedCollator*>(c) : nullptr;
        if (r == nullptr)
        {
            printf("%s\t- error\n", line);
            delete c;
            continue;
        }
        const icu::CollationData* d = r->data;
        printf("%s\t%s %d", line, r->getLocale(ULOC_ACTUAL_LOCALE, e).getName(),
               d->fastLatinTableLength);
        for (int i = 0; i < d->fastLatinTableLength; i++)
        {
            printf(" %x", d->fastLatinTable[i]);
        }
        printf("\n");
        delete c;
    }
}
