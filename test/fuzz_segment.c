// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the segmenter over the built-in data and the
// segmentation data file. The first byte picks the kind and one of the
// tailored locales, with or without -u-ss-standard and -u-lb; the second
// the size of the pieces; the rest is the text. The breaks must ascend, end
// at the text's end, and be the same in pieces as for the text whole.

#include "maul-locale/data.h"
#include "maul-locale/segment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxBreaks = 4096,
};

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static const char* const s_locales[] = {
    "und",
    "de-u-ss-standard",
    "el",
    "en-US-posix",
    "en-u-ss-standard",
    "fr-u-ss-standard",
    "ja",
    "ja-u-lb-strict",
    "pt-u-ss-standard",
    "ru-u-ss-standard",
    "zh",
    "zh-Hant-u-lb-loose",
    "ja-u-lb-loose",
};

alignas(8) static uint8_t s_file[1 << 16];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
static bool s_open;

static void Open(void)
{
    FILE* f = fopen(MLOC_SEGMENTS_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    Require(size > 0 && mlocOpenData(s_file, size, &s_sources[1]) == mloc_success);
    s_open = true;
}

// The breaks of a text fed in pieces of a size (0: whole) and their count.
static size_t Breaks(const mlocLocale* locale, mlocSegmentKind kind, const char* text,
                     size_t length, size_t pieceSize, size_t* out)
{
    mlocSegmenter s;
    size_t fed = pieceSize == 0 || pieceSize > length ? length : pieceSize;
    Require(mlocInitSegmenter(&s_data, locale, kind, text, fed, fed < length, &s) == mloc_success);
    size_t count = 0;
    for (;;)
    {
        size_t offset = 0;
        mlocResult r = mlocSegmenter_NextBreak(&s, &offset, nullptr);
        if (r == mloc_done)
        {
            return count;
        }
        if (r == mloc_needMoreText)
        {
            Require(fed < length);
            size_t piece = length - fed < pieceSize ? length - fed : pieceSize;
            Require(mlocSegmenter_Feed(&s, text + fed, piece, fed + piece < length) ==
                    mloc_success);
            fed += piece;
            continue;
        }
        Require(r == mloc_success && count < maxBreaks && offset <= length &&
                (count == 0 || offset > out[count - 1]) && offset > 0);
        out[count++] = offset;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (!s_open)
    {
        Open();
    }
    if (size < 2 || size - 2 >= maxBreaks)
    {
        return 0;
    }
    const char* id = s_locales[data[0] % (sizeof(s_locales) / sizeof(s_locales[0]))];
    mlocSegmentKind kind = (mlocSegmentKind)(data[0] / 16 % 3);
    size_t pieceSize = 1 + data[1] % 40;
    mlocLocale locale;
    Require(mlocParseLocale(id, strlen(id), &locale).status == mloc_success);
    // The text in an allocation of its own size, so that a read past it
    // is caught.
    size_t length = size - 2;
    char* text = malloc(length == 0 ? 1 : length);
    Require(text != nullptr);
    memcpy(text, data + 2, length);
    static size_t s_whole[maxBreaks];
    static size_t s_pieces[maxBreaks];
    size_t whole = Breaks(&locale, kind, text, length, 0, s_whole);
    size_t pieces = Breaks(&locale, kind, text, length, pieceSize, s_pieces);
    Require(whole == pieces && memcmp(s_whole, s_pieces, whole * sizeof(size_t)) == 0);
    Require(length == 0 ? whole == 0 : whole > 0 && s_whole[whole - 1] == length);
    free(text);
    return 0;
}
