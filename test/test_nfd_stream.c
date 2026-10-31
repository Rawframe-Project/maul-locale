// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The NFD stream against Maul Unicode's whole-string NFD: random texts of
// decomposable letters, Hangul, marks, marks that decompose and
// ill-formed bytes; runs of marks past the window against a reference
// sort; the count of a run's marks held from wrapping; the quick path's
// edges; and, given its path, every line of NormalizationTest.txt.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "nfd_stream.h"
#include "test_harness.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxText = 4096,
    maxCodePoints = 2048,
};

// The stream's code points of a text.
static size_t Stream(const char* text, size_t length, uint32_t* out)
{
    NfdStream s;
    mlocNfdStart(&s, (const uint8_t*)text, length);
    size_t n = 0;
    uint32_t c = 0;
    while (n < maxCodePoints && mlocNfdNext(&s, &c))
    {
        out[n++] = c;
    }
    return n;
}

// Maul Unicode's NFD of a text as code points; false when it refuses it.
static bool Oracle(const char* text, size_t length, uint32_t* out, size_t* countOut)
{
    static char nfd[maxText * 3];
    size_t needed = 0;
    muniTextResult r =
        muniNormalize(text, length, muni_nfd, muni_convertReplace, nfd, sizeof(nfd), &needed);
    if (r.status != muni_success)
    {
        return false;
    }
    r = muniConvertUtf8ToUtf32(nfd, needed, muni_convertStrict, out, maxCodePoints, countOut);
    return r.status == muni_success;
}

// Appends a code point's UTF-8 to text of a capacity, when it fits.
static size_t Append(char* text, size_t capacity, size_t at, uint32_t c)
{
    size_t size = 0;
    if (muniEncodeUtf8(c, text + at, capacity - at, &size) != muni_success)
    {
        return at;
    }
    return at + size;
}

static uint32_t s_seed = 12345;

static uint32_t Random(uint32_t bound)
{
    s_seed = s_seed * 1103515245U + 12345U;
    return (s_seed >> 8) % bound;
}

static void TestRandom(void)
{
    // Letters that decompose, Hangul, jamo, marks of several classes,
    // marks that decompose (U+0344, U+0F73), a starter whose
    // decomposition ends with a mark (U+1D15E, U+2ADC), and ill-formed
    // bytes.
    static const uint32_t pieces[] = {
        0x61,   0x41,   0xE0,    0x1D6,   0x1FA,  0x1EA0, 0x1EB7, 0xAC00, 0xAC01, 0x1100,
        0x1161, 0x11A8, 0x300,   0x301,   0x316,  0x323,  0x327,  0x334,  0x344,  0x345,
        0x5B0,  0xF73,  0x1D15E, 0x1D165, 0x2ADC, 0x338,  0x1F82, 0x3D4,  0xFB2C, 0x0,
    };
    static const char* const bad[] = {"\xC3", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\x80",
                                      "\xE2\x82"};
    static char text[maxText];
    static uint32_t got[maxCodePoints];
    static uint32_t want[maxCodePoints];
    size_t compared = 0;
    bool same = true;
    for (int round = 0; round < 20000 && same; round++)
    {
        size_t at = 0;
        uint32_t pieceCount = Random(24);
        for (uint32_t i = 0; i < pieceCount; i++)
        {
            uint32_t k = Random(sizeof(pieces) / sizeof(pieces[0]) + 5);
            if (k < sizeof(pieces) / sizeof(pieces[0]))
            {
                at = Append(text, sizeof(text), at, pieces[k]);
            }
            else
            {
                const char* b = bad[k - sizeof(pieces) / sizeof(pieces[0])];
                memcpy(text + at, b, strlen(b));
                at += strlen(b);
            }
        }
        size_t wanted = 0;
        if (!Oracle(text, at, want, &wanted))
        {
            continue;
        }
        size_t n = Stream(text, at, got);
        same = n == wanted && memcmp(got, want, n * sizeof(uint32_t)) == 0;
        compared++;
    }
    CHECK(same && compared > 15000,
          "random texts read as Maul Unicode's NFD, ill-formed bytes as U+FFFD");
}

// The reference: a starter's decomposition, then every mark decomposed
// and sorted stably by class.
static size_t Reference(const uint32_t* marks, size_t count, uint32_t* out)
{
    static char text[maxText];
    size_t n = 0;
    for (size_t i = 0; i < count; i++)
    {
        size_t at = Append(text, sizeof(text), 0, marks[i]);
        uint32_t d[8];
        size_t k = 0;
        if (Oracle(text, at, d, &k))
        {
            memcpy(out + n, d, k * sizeof(uint32_t));
            n += k;
        }
    }
    for (size_t i = 1; i < n; i++)
    {
        uint32_t c = out[i];
        uint8_t k = muniGetCombiningClass(c);
        size_t j = i;
        for (; j > 0 && muniGetCombiningClass(out[j - 1]) > k &&
               muniGetCombiningClass(out[j - 1]) != 0 && k != 0;
             j--)
        {
            out[j] = out[j - 1];
        }
        out[j] = c;
    }
    return n;
}

static void TestLongRuns(void)
{
    static const uint32_t marks[] = {0x300, 0x301, 0x316, 0x323, 0x327,   0x334,
                                     0x344, 0x345, 0x5B0, 0xF73, 0x1D165, 0x338};
    static const uint32_t starters[] = {0x61, 0x1EB7, 0x1D6, 0xAC01};
    static const size_t lengths[] = {62, 63, 64, 65, 66, 127, 128, 300, 1000};
    static uint32_t run[maxCodePoints];
    static uint32_t want[maxCodePoints];
    static uint32_t got[maxCodePoints];
    static char text[maxText];
    bool same = true;
    for (size_t s = 0; s < 4; s++)
    {
        for (size_t l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++)
        {
            // The starter, its marks, a following starter "b" and a mark.
            size_t count = 0;
            run[count++] = starters[s];
            for (size_t i = 0; i < lengths[l]; i++)
            {
                run[count++] = marks[Random(sizeof(marks) / sizeof(marks[0]))];
            }
            size_t at = 0;
            for (size_t i = 0; i < count; i++)
            {
                at = Append(text, sizeof(text), at, run[i]);
            }
            at = Append(text, sizeof(text), at, 0x62);
            at = Append(text, sizeof(text), at, 0x301);
            size_t n = Reference(run, count, want);
            want[n++] = 0x62;
            want[n++] = 0x301;
            size_t m = Stream(text, at, got);
            same = same && m == n && memcmp(got, want, n * sizeof(uint32_t)) == 0;
        }
    }
    CHECK(same, "a starter and 62 to 1,000 marks, decomposing ones among them, in canonical "
                "order across windows, and the text after them");
    // Runs that end where a window does (the first holds the starter and
    // 63 marks, the next 64 each), and the text after them.
    static const size_t ends[] = {127, 191};
    for (size_t e = 0; e < 2; e++)
    {
        size_t at = Append(text, sizeof(text), 0, 0x61);
        for (size_t i = 0; i < ends[e]; i++)
        {
            at = Append(text, sizeof(text), at, 0x301);
        }
        at = Append(text, sizeof(text), at, 0x62);
        size_t n = Stream(text, at, got);
        bool acutes = true;
        for (size_t i = 1; i <= ends[e] && i < n; i++)
        {
            acutes = acutes && got[i] == 0x301;
        }
        CHECK(n == ends[e] + 2 && got[0] == 0x61 && acutes && got[n - 1] == 0x62,
              "a long run ending with a window, and the starter after it");
    }
    // A long run ending the text, in a buffer of the text's own size.
    size_t end = Append(text, sizeof(text), 0, 0x61);
    for (int i = 0; i < 100; i++)
    {
        end = Append(text, sizeof(text), end, i % 2 == 0 ? 0x301 : 0x323);
    }
    char* exact = malloc(end);
    if (exact != nullptr)
    {
        memcpy(exact, text, end);
        size_t n = Stream(exact, end, got);
        CHECK(n == 101 && got[0] == 0x61 && got[1] == 0x323 && got[50] == 0x323 &&
                  got[51] == 0x301 && got[100] == 0x301,
              "a long run ending the text read within it");
        free(exact);
    }
    // Marks of different classes swapped make a canonically equal text,
    // which reads the same.
    size_t at = Append(text, sizeof(text), 0, 0x61);
    size_t swapped = Append(text, sizeof(text), 0, 0x61);
    static char other[maxText];
    memcpy(other, text, swapped);
    for (int i = 0; i < 100; i++)
    {
        at = Append(text, sizeof(text), at, 0x301);
        at = Append(text, sizeof(text), at, 0x323);
        swapped = Append(other, sizeof(other), swapped, 0x323);
        swapped = Append(other, sizeof(other), swapped, 0x301);
    }
    size_t n = Stream(text, at, got);
    size_t m = Stream(other, swapped, want);
    CHECK(n == 201 && m == n && memcmp(got, want, n * sizeof(uint32_t)) == 0 && got[1] == 0x323 &&
              got[100] == 0x323 && got[101] == 0x301,
          "two canonically equal texts of 200 marks read the same");
}

static size_t Hexes(const char* field, size_t length, char* out, size_t capacity)
{
    size_t at = 0;
    for (size_t i = 0; i < length;)
    {
        while (i < length && field[i] == ' ')
        {
            i++;
        }
        if (i == length)
        {
            break;
        }
        char* end = nullptr;
        uint32_t c = (uint32_t)strtoul(field + i, &end, 16);
        i = (size_t)(end - field);
        at = Append(out, capacity, at, c);
    }
    return at;
}

static void TestNormalizationFile(const char* path)
{
    FILE* f = fopen(path, "rb");
    CHECK(f != nullptr, "NormalizationTest.txt opens");
    if (f == nullptr)
    {
        return;
    }
    static char line[1024];
    static char columns[5][256];
    static uint32_t got[maxCodePoints];
    static uint32_t want[maxCodePoints];
    size_t lines = 0;
    size_t wrong = 0;
    while (fgets(line, sizeof(line), f) != nullptr)
    {
        if (line[0] == '#' || line[0] == '@' || line[0] == '\n')
        {
            continue;
        }
        size_t sizes[5];
        const char* p = line;
        for (int c = 0; c < 5; c++)
        {
            const char* semi = strchr(p, ';');
            if (semi == nullptr)
            {
                break;
            }
            sizes[c] = Hexes(p, (size_t)(semi - p), columns[c], sizeof(columns[c]));
            p = semi + 1;
        }
        // NFD(c1) = NFD(c2) = NFD(c3) = c3 and NFD(c4) = NFD(c5) = c5.
        for (int c = 0; c < 5; c++)
        {
            int target = c < 3 ? 2 : 4;
            size_t n = Stream(columns[c], sizes[c], got);
            size_t m = Stream(columns[target], sizes[target], want);
            uint32_t check[64];
            size_t k = 0;
            bool ok = muniConvertUtf8ToUtf32(columns[target], sizes[target], muni_convertStrict,
                                             check, 64, &k)
                              .status == muni_success &&
                      n == m && m == k && memcmp(got, check, n * sizeof(uint32_t)) == 0;
            wrong += !ok;
        }
        lines++;
    }
    fclose(f);
    CHECK(lines > 19000 && wrong == 0,
          "every column of NormalizationTest.txt reads as its NFD column");
}

// The quick path (one code point that is its own NFD, or a Hangul
// syllable, before a code point that is no mark): the last syllable and
// the code point after it, a mark that starts a text keeping its class,
// and a syllable's jamo placed at its start and end.
static void TestQuick(void)
{
    static const char s_edge[] = "\xED\x9E\xA3\xED\x9E\xA4";
    uint32_t got[16];
    uint32_t want[16];
    size_t wanted = 0;
    size_t n = Stream(s_edge, sizeof(s_edge) - 1, got);
    CHECK(Oracle(s_edge, sizeof(s_edge) - 1, want, &wanted) && n == wanted && n == 4 &&
              memcmp(got, want, n * sizeof(*got)) == 0,
          "U+D7A3 as its three jamo, U+D7A4 after it as itself");
    NfdStream st;
    static const char s_mark[] = "\xCC\x81"
                                 "a";
    mlocNfdStart(&st, (const uint8_t*)s_mark, sizeof(s_mark) - 1);
    uint32_t c = 0;
    uint8_t ccc = 0;
    uint32_t from = 0;
    bool first = mlocNfdNextFrom(&st, &c, &ccc, &from) && c == 0x301 && ccc == 230 && from == 0;
    CHECK(first && mlocNfdNextFrom(&st, &c, &ccc, &from) && c == 'a' && ccc == 0 && from == 2,
          "a mark that starts a text keeps its class");
    static const char s_syllables[] = "\xEA\xB0\x80\xEB\x82\x98";
    mlocNfdStart(&st, (const uint8_t*)s_syllables, sizeof(s_syllables) - 1);
    uint32_t froms[4] = {0};
    size_t k = 0;
    while (k < 4 && mlocNfdNextFrom(&st, &c, &ccc, &froms[k]))
    {
        k++;
    }
    CHECK(k == 4 && froms[0] == 0 && froms[1] == 3 && froms[2] == 3 && froms[3] == 6,
          "each syllable's first jamo at its start, the second at its end");
}

// A run's marks are counted before they are read, to tell whether they
// fit the window. 2^32 marks (4 GiB of UTF-16) would wrap a count kept
// whole and read as a few that fit, so the count stops growing once past
// the window: started near the wrap, it stays past the window.
static void TestMarkCount(void)
{
    uint32_t total = 0;
    for (uint32_t i = 0; i < 40; i++)
    {
        total = mlocNfdCountMarks(total, 2);
    }
    CHECK(total == nfdWindow + 2 && mlocNfdCountMarks(nfdWindow, 4) == nfdWindow + 4,
          "a run's count grows to just past the window, then holds");
    CHECK(mlocNfdCountMarks(UINT32_MAX - 1, 2) == UINT32_MAX - 1 &&
              mlocNfdCountMarks(UINT32_MAX, nfdMaxDecomposition) == UINT32_MAX,
          "a count near 2^32 holds instead of wrapping into the window");
}

int main(int argc, char** argv)
{
    TestMarkCount();
    TestQuick();
    TestRandom();
    TestLongRuns();
    if (argc > 1)
    {
        TestNormalizationFile(argv[1]);
    }
    return s_failures == 0 ? 0 : 1;
}
