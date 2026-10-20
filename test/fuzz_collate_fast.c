// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the fast Latin compare against the general path
// (record mloc-0072), through the public collator. The first byte
// chooses a locale and options, the second where the rest is cut into
// two texts, and each byte after them a character: ASCII as itself,
// most of the rest a letter or sign up to U+017F or general
// punctuation, the last few one the table bails out on (combining
// marks, Cyrillic, U+FFFE, U+FFFF, the middle dot). mlocCompare, which
// tries the fast compare first, must order the texts as their sort keys
// do (the keys never read the table), the mirror the other way, and
// each text with itself as equal.

#include "maul-locale/collate.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

enum
{
    maxText = 1024,
};

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static const char* const s_tags[] = {"und",
                                     "en",
                                     "cs",
                                     "da",
                                     "de-u-co-phonebk",
                                     "es-u-co-trad",
                                     "hu",
                                     "vi-u-co-trad",
                                     "sv",
                                     "pl",
                                     "cy",
                                     "fr-CA",
                                     "en-US-POSIX",
                                     "de-u-co-search",
                                     "en-u-ka-shifted",
                                     "en-u-ka-shifted-kv-space",
                                     "en-u-ka-shifted-kv-currency",
                                     "en-u-ks-level1",
                                     "en-u-ks-level2",
                                     "en-u-ka-shifted-ks-level4",
                                     "en-u-kf-upper",
                                     "en-u-kf-lower",
                                     "en-u-kc",
                                     "en-u-kc-ks-level1",
                                     "en-u-kn",
                                     "en-u-kr-latn-digit",
                                     "en-u-kr-grek-latn",
                                     "da-u-kf-lower-kc",
                                     "hr",
                                     "sk",
                                     "lt",
                                     "en-u-ks-identic"};

enum
{
    tags = sizeof(s_tags) / sizeof(s_tags[0]),
};

static mlocData s_sources[2];
static mlocDataSet s_set = {s_sources, 2};
static mlocCollator s_collators[tags];

// The collators made once; false when the data does not open.
static bool Ready(void)
{
    alignas(8) static uint8_t s_file[1 << 22];
    static int s_state;
    if (s_state == 0)
    {
        s_state = -1;
        FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
        size_t n = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
        if (f != nullptr)
        {
            fclose(f);
        }
        mlocOpenBuiltinData(&s_sources[0]);
        bool ok =
            n > 0 && n < sizeof(s_file) && mlocOpenData(&s_sources[1], s_file, n) == mloc_success;
        for (size_t i = 0; ok && i < tags; i++)
        {
            mlocLocale locale;
            ok = mlocParseLocale(s_tags[i], strlen(s_tags[i]), &locale).status == mloc_success &&
                 mlocInitCollator(&s_collators[i], &s_set, &locale, nullptr) == mloc_success;
        }
        s_state = ok ? 1 : -1;
    }
    return s_state == 1;
}

// The character a byte names.
static uint32_t CodePoint(uint8_t b)
{
    static const uint32_t bails[] = {0x0301, 0x0308, 0x0327, 0x0430, 0xFFFE,
                                     0xFFFF, 0x00B7, 0x1EBD, 0x030C, 0x0323};
    if (b < 0x80)
    {
        return b;
    }
    if (b < 0xE0)
    {
        return 0xA0 + (uint32_t)(b - 0x80) * 2 + (b & 1);
    }
    if (b < 0xF6)
    {
        return 0x2000 + (uint32_t)(b - 0xE0) * 3;
    }
    return bails[b - 0xF6];
}

static size_t Utf8(const uint8_t* bytes, size_t count, char* out)
{
    size_t n = 0;
    for (size_t i = 0; i < count; i++)
    {
        uint32_t c = CodePoint(bytes[i]);
        if (c < 0x80)
        {
            out[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            out[n++] = (char)(0xC0 | c >> 6);
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            out[n++] = (char)(0xE0 | c >> 12);
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return n;
}

static int KeyOrder(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    int c = memcmp(a, b, aLength < bLength ? aLength : bLength);
    return c != 0 ? (c > 0) - (c < 0) : (aLength > bLength) - (aLength < bLength);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Require(Ready());
    if (size < 2 || size > 2 + maxText / 3)
    {
        return 0;
    }
    const mlocCollator* c = &s_collators[data[0] % tags];
    size_t count = size - 2;
    size_t cut = data[1] % (count + 1);
    static char a[maxText];
    static char b[maxText];
    size_t aLength = Utf8(data + 2, cut, a);
    size_t bLength = Utf8(data + 2 + cut, count - cut, b);
    int ab = 2;
    int ba = 2;
    int aa = 2;
    Require(mlocCompare(c, a, aLength, b, bLength, &ab) == mloc_success &&
            mlocCompare(c, b, bLength, a, aLength, &ba) == mloc_success &&
            mlocCompare(c, a, aLength, a, aLength, &aa) == mloc_success);
    Require(ab == -ba && aa == 0);
    static uint8_t ka[1 << 14];
    static uint8_t kb[1 << 14];
    size_t na = 0;
    size_t nb = 0;
    Require(mlocGetSortKey(c, a, aLength, ka, sizeof(ka), &na) == mloc_success &&
            mlocGetSortKey(c, b, bLength, kb, sizeof(kb), &nb) == mloc_success);
    Require(KeyOrder(ka, na, kb, nb) == ab);
    return 0;
}
