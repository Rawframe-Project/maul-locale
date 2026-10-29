// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the alphabetic index, through the public calls.
// The first byte chooses a locale, the second what the rest holds: with
// bit 0 the third byte is the most labels kept and the index is built
// for this input, with bit 1 three special labels follow, each a length
// byte (below 16) and its bytes as given, often not UTF-8, with bit 2 the
// index is built at an odd address, and with bit 7 the names are bytes as
// given rather than characters. Without bit 0 the locale's index with the
// default options, built once, serves.
//
// The rest is names, each a length byte (below 32) and its bytes. In the
// character form each byte is a character as fuzz_search.c maps them, and
// the UTF-8 and UTF-16 finds must agree. Every find gives a bucket the
// index shows. A built index is measured first: it fits the size measured,
// and only a special label that is not UTF-8 is refused.

#include "maul-locale/collate.h"
#include "maul-locale/data.h"
#include "maul-locale/index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

enum
{
    indexCapacity = 1 << 17,
    maxName = 32,
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
                                     "de",
                                     "sv",
                                     "cs",
                                     "sk",
                                     "lt",
                                     "fi",
                                     "cy",
                                     "ru",
                                     "el",
                                     "ar",
                                     "he",
                                     "hi",
                                     "ta",
                                     "th",
                                     "km",
                                     "cu",
                                     "bal",
                                     "ja",
                                     "ko",
                                     "zh",
                                     "zh-u-co-stroke",
                                     "zh-u-co-zhuyin",
                                     "zh-Hant",
                                     "yue",
                                     "zh-u-kr-latn-grek-hani",
                                     "en-u-ka-shifted",
                                     "sr-Latn"};

enum
{
    tags = sizeof(s_tags) / sizeof(s_tags[0]),
};

static mlocData s_sources[2];
static mlocDataSet s_set = {s_sources, 2};
static mlocLocale s_locales[tags];
static mlocCollator s_collators[tags];
static uint8_t s_indexes[tags][indexCapacity];

// The collators and the default indexes made once; false when the data
// does not open or an index is not built.
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
            n > 0 && n < sizeof(s_file) && mlocOpenData(s_file, n, &s_sources[1]) == mloc_success;
        for (size_t i = 0; ok && i < tags; i++)
        {
            size_t indexSize = 0;
            ok =
                mlocParseLocale(s_tags[i], strlen(s_tags[i]), &s_locales[i]).status ==
                    mloc_success &&
                mlocInitCollator(&s_set, &s_locales[i], nullptr, &s_collators[i]) == mloc_success &&
                mlocBuildIndex(&s_collators[i], &s_set, &s_locales[i], nullptr, s_indexes[i],
                               indexCapacity, &indexSize) == mloc_success;
        }
        s_state = ok ? 1 : -1;
    }
    return s_state == 1;
}

// The character a byte names, as fuzz_search.c maps them.
static uint32_t CodePoint(uint8_t b)
{
    static const uint32_t s_groups[8][16] = {
        {0x0300, 0x0301, 0x0302, 0x0303, 0x0308, 0x030A, 0x030C, 0x0323, 0x0327, 0x0328, 0x0334,
         0x034F, 0x035C, 0x1AB0, 0x20D0, 0x3099},
        {0x1100, 0x1101, 0x1112, 0x115F, 0x1160, 0x1161, 0x1175, 0x11A8, 0x11C2, 0xAC00, 0xAC01,
         0xD7A3, 0x3131, 0x314F, 0xFFA0, 0x1F1E6},
        {0x0E01, 0x0E40, 0x0E47, 0x0E48, 0x0EC0, 0x1780, 0x17D2, 0x17B6, 0x0915, 0x094D, 0x0937,
         0x093F, 0x200C, 0x200D, 0x0009, 0x000A},
        {0x1F600, 0x1F469, 0x1F4BB, 0x1F3FB, 0x1F1F7, 0xFE0F, 0x2764, 0x1F44D, 0x20000, 0x2A6D6,
         0x10400, 0x10428, 0xE0061, 0xE007F, 0x1D400, 0x10FFFD},
        {0x0391, 0x03B1, 0x03C2, 0x0410, 0x0430, 0x0451, 0x0627, 0x0644, 0x064B, 0x0670, 0x05D0,
         0x05B4, 0x06F0, 0x0660, 0x0966, 0xFF10},
        {0x4E00, 0x4E2D, 0x5B57, 0x3042, 0x304B, 0x3099, 0x30AB, 0x30FC, 0x3005, 0x2F00, 0xF900,
         0xFF21, 0x3000, 0x3001, 0x31F0, 0x3400},
        {0xFFFD, 0xFFFE, 0xFFFF, 0xFDD0, 0xFDD1, 0xE000, 0xF8FF, 0x00AD, 0x2028, 0x0378, 0xD7FF,
         0x10FFFF, 0x0000, 0x00A0, 0x2060, 0x1D16E},
        {0x0030, 0x0039, 0x002D, 0x2010, 0x2019, 0x201C, 0x00B7, 0x0130, 0x0131, 0x00DF, 0x1E9E,
         0x0149, 0x01C4, 0x01C6, 0xFB01, 0x2126},
    };
    if (b < 0x60)
    {
        return 0x20u + b;
    }
    if (b < 0x80)
    {
        return 0xC0u + (uint32_t)(b - 0x60) * 3;
    }
    return s_groups[(b - 0x80) >> 4][b & 15];
}

// A name's UTF-8 and UTF-16 forms.
static void Encode(const uint8_t* in, size_t count, char* bytes, size_t* lengthOut, uint16_t* units,
                   size_t* unitsOut)
{
    size_t n = 0;
    size_t u = 0;
    for (size_t i = 0; i < count; i++)
    {
        uint32_t c = CodePoint(in[i]);
        if (c < 0x80)
        {
            bytes[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            bytes[n++] = (char)(0xC0 | c >> 6);
            bytes[n++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            bytes[n++] = (char)(0xE0 | c >> 12);
            bytes[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            bytes[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            bytes[n++] = (char)(0xF0 | c >> 18);
            bytes[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            bytes[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            bytes[n++] = (char)(0x80 | (c & 0x3F));
        }
        if (c < 0x10000)
        {
            units[u++] = (uint16_t)c;
        }
        else
        {
            units[u++] = (uint16_t)(0xD800 | (c - 0x10000) >> 10);
            units[u++] = (uint16_t)(0xDC00 | (c & 0x3FF));
        }
    }
    *lengthOut = n;
    *unitsOut = u;
}

// Builds an index with the options the input gives into buffer; null
// when a special label is refused.
static const void* Build(size_t tag, const uint8_t** at, const uint8_t* end, uint8_t flags,
                         uint8_t* buffer)
{
    mlocIndexOptions options = {0};
    bool specials = (flags & 2) != 0;
    if (*at < end)
    {
        options.maxLabelCount = *(*at)++;
    }
    const char* labels[3] = {nullptr, nullptr, nullptr};
    size_t lengths[3] = {0, 0, 0};
    for (int k = 0; specials && k < 3 && *at < end; k++)
    {
        size_t length = *(*at)++ % 16;
        length = length < (size_t)(end - *at) ? length : (size_t)(end - *at);
        labels[k] = (const char*)*at;
        lengths[k] = length;
        *at += length;
    }
    options.underflowLabel = labels[0];
    options.underflowLength = lengths[0];
    options.inflowLabel = labels[1];
    options.inflowLength = lengths[1];
    options.overflowLabel = labels[2];
    options.overflowLength = lengths[2];
    size_t needed = 0;
    mlocResult r =
        mlocBuildIndex(&s_collators[tag], &s_set, &s_locales[tag], &options, nullptr, 0, &needed);
    if (r == mloc_errorInvalid)
    {
        Require(specials);
        return nullptr;
    }
    Require(r == mloc_errorCapacity && needed > 0 && needed <= indexCapacity);
    size_t built = 0;
    Require(mlocBuildIndex(&s_collators[tag], &s_set, &s_locales[tag], &options, buffer, needed,
                           &built) == mloc_success &&
            built <= needed);
    return buffer;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Require(Ready());
    if (size < 2 || size > 4096)
    {
        return 0;
    }
    size_t tag = data[0] % tags;
    uint8_t flags = data[1];
    const uint8_t* at = data + 2;
    const uint8_t* end = data + size;
    static uint8_t s_built[indexCapacity + 1];
    const void* index = s_indexes[tag];
    if ((flags & 1) != 0)
    {
        index = Build(tag, &at, end, flags, s_built + ((flags & 4) != 0 ? 1 : 0));
        if (index == nullptr)
        {
            return 0;
        }
    }
    uint32_t count = 0;
    Require(mlocGetIndexBucketCount(index, &count) == mloc_success && count >= 1);
    mlocIndexBucket bucket;
    Require(mlocGetIndexBucket(index, 0, &bucket) == mloc_success &&
            bucket.type == mloc_indexUnderflow);
    Require(mlocGetIndexBucket(index, count, &bucket) == mloc_errorInvalid);
    while (at < end)
    {
        size_t length = *at++ % maxName;
        length = length < (size_t)(end - at) ? length : (size_t)(end - at);
        uint32_t found = 0;
        if ((flags & 0x80) != 0)
        {
            Require(mlocFindIndexBucket(index, (const char*)at, length, &found) == mloc_success &&
                    found < count);
            uint16_t units[maxName / 2];
            memcpy(units, at, length / 2 * 2);
            uint32_t found16 = 0;
            Require(mlocFindIndexBucketUtf16(index, units, length / 2, &found16) == mloc_success &&
                    found16 < count);
        }
        else
        {
            char bytes[maxName * 4];
            uint16_t units[maxName * 2];
            size_t byteLength = 0;
            size_t unitLength = 0;
            Encode(at, length, bytes, &byteLength, units, &unitLength);
            uint32_t found16 = 0;
            Require(mlocFindIndexBucket(index, bytes, byteLength, &found) == mloc_success &&
                    mlocFindIndexBucketUtf16(index, units, unitLength, &found16) == mloc_success &&
                    found < count && found16 == found);
        }
        Require(mlocGetIndexBucket(index, found, &bucket) == mloc_success);
        at += length;
    }
    return 0;
}
