// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder against ICU4C's: reads
// conform_collation_builder --queries (for each type its rule string and
// the NFD strings its rules name), builds each rule string with ICU's
// RuleBasedCollator and writes each string's elements as
// conform_collation_builder --answers writes the builder's: their count,
// then each 64-bit element in hex, joined back from the
// CollationElementIterator's two 32-bit halves; "error" for a rule
// string ICU refuses. The escapes are decoded first, as ICU's data build
// decodes CLDR's rule strings. Completely ignorable elements are left out.
//   g++ -O2 -I$ICU/include icu4c_collation_tailored.cpp -L$ICU/lib \
//       -licui18n -licuuc -Wl,-rpath,$ICU/lib -o icu_tailored
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unicode/coleitr.h>
#include <unicode/tblcoll.h>
#include <unicode/unistr.h>
#include <vector>
using namespace icu;
// ICU's data build decodes a rule string's backslash-u (4 hex), backslash-U (8),
// backslash-x with braces or 2 hex, and a doubled backslash before the rule
// parser reads it; the parser takes any other
// backslash as escaping one code point.
static int Hex(UChar32 c)
{
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                  : -1;
}
static UnicodeString Decode(const UnicodeString& raw)
{
    UnicodeString out;
    int32_t n = raw.length();
    for (int32_t k = 0; k < n;)
    {
        UChar32 c = raw.char32At(k);
        int32_t used = U16_LENGTH(c);
        if (c == '\\' && k + 1 < n)
        {
            UChar32 x = raw.charAt(k + 1);
            UChar32 v = 0;
            int32_t e = 0;
            if (x == '\\')
            {
                v = '\\';
                e = 2;
            }
            else if (x == 'u' || x == 'U')
            {
                int d = x == 'u' ? 4 : 8;
                for (int i = 0; i < d; i++)
                {
                    v = v * 16 + Hex(raw.charAt(k + 2 + i));
                }
                e = 2 + d;
            }
            else if (x == 'x' && raw.charAt(k + 2) == '{')
            {
                int32_t j = k + 3;
                while (Hex(raw.charAt(j)) >= 0)
                {
                    v = v * 16 + Hex(raw.charAt(j++));
                }
                e = j + 1 - k;
            }
            else if (x == 'x')
            {
                int32_t j = k + 2;
                while (j < k + 4 && Hex(raw.charAt(j)) >= 0)
                {
                    v = v * 16 + Hex(raw.charAt(j++));
                }
                e = j - k;
            }
            if (e)
            {
                out.append(v);
                k += e;
                continue;
            }
        }
        out.append(c);
        k += used;
    }
    return out;
}
int main()
{
    char line[1 << 16];
    std::unique_ptr<RuleBasedCollator> coll;
    bool failed = false;
    while (fgets(line, sizeof line, stdin))
    {
        if (!strncmp(line, "type ", 5))
        {
            fputs(line, stdout);
            continue;
        }
        if (!strncmp(line, "rules ", 6))
        {
            size_t n = strtoul(line + 6, nullptr, 10);
            std::string r(n, '\0');
            if (fread(&r[0], 1, n, stdin) != n)
            {
                return 1;
            }
            fgetc(stdin);
            UErrorCode e = U_ZERO_ERROR;
            coll.reset(new RuleBasedCollator(Decode(UnicodeString::fromUTF8(r)), e));
            failed = U_FAILURE(e);
            if (failed)
            {
                printf("error\n");
                coll.reset();
            }
            continue;
        }
        if (!strncmp(line, "end", 3))
        {
            printf("end\n");
            continue;
        }
        if (line[0] != 'q' || failed)
        {
            continue;
        }
        UnicodeString s;
        char* p = line + 1;
        while (*p && *p != '\n')
        {
            s.append((UChar32)strtoul(p, &p, 16));
        }
        std::unique_ptr<CollationElementIterator> it(coll->createCollationElementIterator(s));
        UErrorCode e = U_ZERO_ERROR;
        std::vector<unsigned long long> ces;
        int32_t o;
        while ((o = it->next(e)) != CollationElementIterator::NULLORDER && U_SUCCESS(e))
        {
            uint32_t x = (uint32_t)o;
            if ((x & 0xc0) == 0xc0 && !ces.empty())
            {
                unsigned long long c = ces.back();
                uint32_t pr = (uint32_t)(c >> 32) | (x >> 16),
                         lo = (uint32_t)c | ((x & 0xff00) << 8) | (x & 0x3f);
                ces.back() = ((unsigned long long)pr << 32) | lo;
                continue;
            }
            unsigned long long c = ((unsigned long long)(x & 0xffff0000) << 32) |
                                   ((x & 0xff00) << 16) | ((x & 0xff) << 8);
            ces.push_back(c);
        }
        std::vector<unsigned long long> kept;
        for (auto c : ces)
        {
            if (c)
            {
                kept.push_back(c);
            }
        }
        printf("%zu", kept.size());
        for (size_t i = 0; i < kept.size() && i < 31; i++)
        {
            printf(" %016llX", kept[i]);
        }
        printf("\n");
    }
}
