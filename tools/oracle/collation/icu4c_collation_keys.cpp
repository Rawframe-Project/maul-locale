// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Tailored locales' sort keys against ICU4C's: reads
// conform_collation_keys --queries ("tag" lines, each a BCP 47 tag with
// -u-co and options, then "q" lines of code points in hex), makes ICU's
// collator for each tag through its own loader and data, and writes each
// string's sort key in hex as conform_collation_keys --keys writes the
// library's ("k error" for a tag ICU cannot open). Each tag line also
// names, after a tab, the locale and type ICU's loader took (its actual
// locale), since ICU's data leaves some CLDR locales and types out.
//   g++ -O2 -I$ICU/include icu4c_collation_keys.cpp -L$ICU/lib \
//       -licui18n -licuuc -Wl,-rpath,$ICU/lib -o icu_keys
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unicode/coll.h>
#include <unicode/locid.h>
#include <unicode/unistr.h>
#include <vector>
using namespace icu;
int main()
{
    char line[1 << 16];
    std::unique_ptr<Collator> coll;
    while (fgets(line, sizeof line, stdin))
    {
        if (!strncmp(line, "tag ", 4))
        {
            line[strcspn(line, "\n")] = 0;
            UErrorCode e = U_ZERO_ERROR;
            Locale loc = Locale::forLanguageTag(line + 4, e);
            coll.reset(U_SUCCESS(e) ? Collator::createInstance(loc, e) : nullptr);
            if (U_FAILURE(e))
            {
                coll.reset();
            }
            // The locale and type ICU's loader took, which its data may lack.
            Locale actual = coll ? coll->getLocale(ULOC_ACTUAL_LOCALE, e) : Locale::getRoot();
            printf("%s\t%s\n", line, actual.getName());
            continue;
        }
        if (line[0] != 'q')
        {
            continue;
        }
        if (!coll)
        {
            printf("k error\n");
            continue;
        }
        UnicodeString s;
        char* p = line + 1;
        while (*p && *p != '\n')
        {
            s.append((UChar32)strtoul(p, &p, 16));
        }
        std::vector<uint8_t> key(4096);
        int32_t n = coll->getSortKey(s, key.data(), (int32_t)key.size());
        if (n > (int32_t)key.size())
        {
            key.resize(n);
            n = coll->getSortKey(s, key.data(), n);
        }
        printf("k");
        for (int32_t i = 0; i < n; i++)
        {
            printf("%s%02X", i ? "" : " ", key[i]);
        }
        printf("\n");
    }
}
