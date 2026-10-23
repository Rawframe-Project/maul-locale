// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Script reordering against ICU4C: reads the samples
// conform_collation_reorder writes (a code point a range of the root's
// script starts, hex) from samples.txt, and lines of reorder codes from
// the input (scripts as their four letters; space, punct, symbol,
// currency and digit for the groups). Writes for each line ICU's result
// (0, -1 for U_ILLEGAL_ARGUMENT_ERROR, -2 for U_BUFFER_OVERFLOW_ERROR)
// and, when 0, each sample's primary bytes from its sort key, in hex, as
// conform_collation_reorder writes ours. A sort key leaves out a zero
// byte, so compare primaries with their zero bytes dropped.
//
// Built against ICU4C 79 on CLDR 49, whose script starts are the root's:
//   g++ -O2 -I$ICU/include icu4c_collation_reorder.cpp -L$ICU/lib \
//       -licui18n -licuuc -Wl,-rpath,$ICU/lib -o icu_reorder
//
// ICU 79 masks its front with 0xFF00 while placing ranges, so an order
// that passes the last lead byte wraps to the first and is taken:
// unassigned code points and Han then sort before Latin. Maul Locale
// refuses those orders as too large (mloc_errorCapacity), or, for one
// beginning with Latin, places Latin like any other script, as ICU does
// when its own check fails.

#include <unicode/uchar.h>
#include <unicode/ucol.h>
#include <unicode/uscript.h>

#include <cstdio>
#include <cstring>
#include <vector>

int main()
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator* c = ucol_open("", &status);
    std::vector<UChar32> samples;
    FILE* f = fopen("samples.txt", "r");
    unsigned value = 0;
    while (f != nullptr && fscanf(f, "%x", &value) == 1)
    {
        samples.push_back(static_cast<UChar32>(value));
    }
    if (f != nullptr)
    {
        fclose(f);
    }
    static const char* const groups[] = {"space", "punct", "symbol", "currency", "digit"};
    static char line[4096];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        int32_t codes[64];
        int32_t count = 0;
        for (char* word = strtok(line, " \r\n"); word != nullptr && count < 64;
             word = strtok(nullptr, " \r\n"))
        {
            int32_t code = -100;
            for (int32_t i = 0; i < 5; i++)
            {
                code = strcmp(word, groups[i]) == 0 ? UCOL_REORDER_CODE_FIRST + i : code;
            }
            codes[count++] = code != -100 ? code : u_getPropertyValueEnum(UCHAR_SCRIPT, word);
        }
        status = U_ZERO_ERROR;
        ucol_setReorderCodes(c, nullptr, 0, &status);
        ucol_setReorderCodes(c, codes, count, &status);
        printf("%d", status == U_ZERO_ERROR                ? 0
                     : status == U_ILLEGAL_ARGUMENT_ERROR  ? -1
                     : status == U_BUFFER_OVERFLOW_ERROR   ? -2
                                                           : -9);
        for (size_t i = 0; status == U_ZERO_ERROR && i < samples.size(); i++)
        {
            UChar text[2];
            int32_t n = 0;
            U16_APPEND_UNSAFE(text, n, samples[i]);
            uint8_t key[64];
            ucol_getSortKey(c, text, n, key, sizeof(key));
            uint32_t primary = 0;
            for (int b = 0; b < 4 && key[b] > 1; b++)
            {
                primary |= static_cast<uint32_t>(key[b]) << (24 - 8 * b);
            }
            printf(" %X", primary);
        }
        printf("\n");
    }
    ucol_close(c);
    return 0;
}
