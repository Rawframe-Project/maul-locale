// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unsafe-backward ranges against ICU4C's: reads
// conform_collation_fast --unsafe's output (a tag first on each line),
// makes ICU's collator for each tag and writes its unsafeBackwardSet as
// the tool writes the library's: the tag, then each range "first-last"
// in hex. The marks (lead combining class not 0) and the surrogates,
// which ICU adds to the set at load and the library tests apart, are
// left out. The set is a private member of RuleBasedCollator, read here
// by a define as only an oracle may.
//   g++ -O2 -I$ICU/include -I$ICU_SRC/common -I$ICU_SRC/i18n \
//       icu4c_collation_unsafe.cpp -L$ICU/lib -licui18n -licuuc \
//       -licudata -Wl,-rpath,$ICU/lib -o icu_unsafe
#include "collationdata.h"
#define private public
#include "unicode/tblcoll.h"
#undef private
#include "unicode/coll.h"
#include "unicode/uniset.h"

#include <cstdio>
#include <cstring>
int main()
{
    char line[1 << 16];
    UErrorCode e = U_ZERO_ERROR;
    icu::UnicodeSet apart(icu::UnicodeString(u"[[:^lccc=0:][\\ud800-\\udfff]]"), e);
    if (U_FAILURE(e))
    {
        fprintf(stderr, "set: %s\n", u_errorName(e));
        return 1;
    }
    while (fgets(line, sizeof line, stdin))
    {
        line[strcspn(line, " \n")] = 0;
        e = U_ZERO_ERROR;
        icu::Locale loc = icu::Locale::forLanguageTag(line, e);
        icu::Collator* c = U_SUCCESS(e) ? icu::Collator::createInstance(loc, e) : nullptr;
        auto* r = U_SUCCESS(e) ? dynamic_cast<icu::RuleBasedCollator*>(c) : nullptr;
        if (r == nullptr)
        {
            printf("%s error\n", line);
            delete c;
            continue;
        }
        icu::UnicodeSet s;
        s.addAll(*r->data->unsafeBackwardSet).removeAll(apart);
        printf("%s", line);
        for (int i = 0; i < s.getRangeCount(); i++)
        {
            printf(" %x-%x", s.getRangeStart(i), s.getRangeEnd(i));
        }
        printf("\n");
        delete c;
    }
}
