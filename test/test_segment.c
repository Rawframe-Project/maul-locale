// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The public segmenter over the built-in data and the segmentation data
// file: each tailoring as ICU4C 79.1 answers it, where the library follows
// CLDR's files and ICU4C does not the answer CLDR's rules give, the -u-ss
// and -u-lb keys, the same breaks for text in pieces as for text whole,
// and the refusals; then every case of the ICU4C oracle's table
// (test/segment_icu.tsv), whole and in pieces, its differences those
// tools/segment-exceptions.txt lists.

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/segment.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 16];
static char s_table[1 << 17];
static char s_exceptions[1 << 13];

// The built-in data, for the parents, then the segmentation file.
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};

static mlocSegmentKind KindOf(char kind)
{
    return kind == 'w' ? mloc_segmentWord : kind == 's' ? mloc_segmentSentence : mloc_segmentLine;
}

// A segmenter copied to another variable, the old one spoiled: a
// segmenter is a value.
static mlocSegmenter* Moved(mlocSegmenter* from, mlocSegmenter* to)
{
    *to = *from;
    memset(from, 0xA5, sizeof(*from));
    return to;
}

// The segments of a text as "[a][b]", the text fed in pieces of the size
// given (0: whole); false when a call fails. The segmenter moves to
// another variable after it is made and after each piece, here and in
// Offsets.
static bool Segments(const char* id, char kind, const char* text, size_t pieceSize, char* out,
                     size_t capacity)
{
    mlocLocale locale;
    mlocSegmenter one;
    mlocSegmenter two;
    size_t length = strlen(text);
    size_t fed = pieceSize == 0 || pieceSize > length ? length : pieceSize;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        mlocInitSegmenter(&one, &s_data, &locale, KindOf(kind), text, fed, fed < length) !=
            mloc_success)
    {
        return false;
    }
    mlocSegmenter* s = Moved(&one, &two);
    size_t at = 0;
    size_t written = 0;
    for (;;)
    {
        size_t offset = 0;
        mlocResult r = mlocNextSegmentBreak(s, &offset, nullptr);
        if (r == mloc_done)
        {
            out[written] = '\0';
            return at == length;
        }
        if (r == mloc_needMoreText)
        {
            size_t piece = length - fed < pieceSize ? length - fed : pieceSize;
            if (mlocFeedSegmenter(s, text + fed, piece, fed + piece < length) != mloc_success)
            {
                return false;
            }
            s = s == &one ? Moved(&one, &two) : Moved(&two, &one);
            fed += piece;
            continue;
        }
        if (r != mloc_success || offset <= at || offset > length ||
            written + offset - at + 3 > capacity)
        {
            return false;
        }
        out[written++] = '[';
        memcpy(out + written, text + at, offset - at);
        written += offset - at;
        out[written++] = ']';
        at = offset;
    }
}

// Whether a text's segments are these, whole and in pieces of every size
// up to 8 bytes.
static bool Is(const char* id, char kind, const char* text, const char* want)
{
    char got[1024];
    for (size_t piece = 0; piece <= 8; piece++)
    {
        if (!Segments(id, kind, text, piece, got, sizeof(got)) || strcmp(got, want) != 0)
        {
            printf("  %s %c in pieces of %zu: %s\n", id, kind, piece, got);
            return false;
        }
    }
    return true;
}

static void TestSentences(void)
{
    CHECK(Is("en-u-ss-standard", 's', "Mr. Smith left. He came.", "[Mr. Smith left. ][He came.]") &&
              Is("en", 's', "Mr. Smith left. He came.", "[Mr. ][Smith left. ][He came.]"),
          "en: -u-ss-standard keeps a sentence going after Mr.; without it every break stays");
    CHECK(Is("de-u-ss-standard", 's', "Er kam z. Bald ging er. Dann z.B. Hans kam.",
             "[Er kam z. ][Bald ging er. Dann z.B. Hans kam.]"),
          "de: z. is a forward entry alone (z.B. shares it), so ICU4C breaks after it; er. "
          "and z.B. end with matches");
    CHECK(Is("en-u-ss-standard", 's', "See Mr. Smith. Then Ph.D. Students came. Then Ph. Done.",
             "[See Mr. Smith. ][Then Ph.D. ][Students came. ][Then Ph. ][Done.]"),
          "en: Ph.D. breaks, D. being a partial whose walk finds no entry; Ph. alone breaks");
    // Maul Unicode holds the break after "Mr. " back until the letter after
    // the numbers, past the bytes a segmenter keeps of the earlier pieces.
    CHECK(
        Is("en-u-ss-standard", 's',
           "Mr. (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, "
           "23, 24, 25) Smith left. He came.",
           "[Mr. (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, "
           "23, 24, 25) Smith left. ][He came.]") &&
            Is("en-u-ss-standard", 's',
               "Xy. (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, "
               "22, "
               "23, 24, 25) Smith left.",
               "[Xy. ][(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, "
               "22, 23, 24, 25) Smith left.]") &&
            Is("de-u-ss-standard", 's',
               "Er kam z. (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, "
               "21, 22, 23) Bald.",
               "[Er kam z. ][(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, "
               "20, 21, 22, 23) Bald.]"),
        "a break held back past the kept bytes: a match suppresses it, a partial's walk "
        "keeps it, and no entry keeps it");
    CHECK(Is("el", 's',
             "\xCE\x9D\xCE\xB1\xCE\xB9; \xCE\x8C\xCF\x87\xCE\xB9. "
             "\xCE\xA4\xCE\xAD\xCE\xBB\xCE\xBF\xCF\x82",
             "[\xCE\x9D\xCE\xB1\xCE\xB9; ][\xCE\x8C\xCF\x87\xCE\xB9. "
             "][\xCE\xA4\xCE\xAD\xCE\xBB\xCE\xBF\xCF\x82]"),
          "el: a sentence ends at the semicolon");
}

static void TestWords(void)
{
    CHECK(Is("en-US-posix", 'w', "3.14 a.b", "[3.14][ ][a][.][b]") &&
              Is("en-US", 'w', "3.14 a.b", "[3.14][ ][a.b]"),
          "POSIX English keeps the full stop out of words");
    CHECK(Is("en-US-u-va-posix", 'w', "3.14 a.b", "[3.14][ ][a][.][b]") &&
              Is("en-US-u-ss-standard-va-posix", 's', "Ab Mr. Cd ef. Gh.", "[Ab Mr. Cd ef. ][Gh.]"),
          "-u-va-posix is POSIX English: its words, and English's sentences");
    // Rules 13.3 and 13.4 keep a Hiragana run, and a run of ideographs (with
    // U+3005), one word.
    CHECK(Is("ja", 'w',
             "\xE3\x81\xB2\xE3\x82\x89\xE3\x81\x8C\xE3\x81\xAA\xE6\xBC\xA2\xE5\xAD\x97"
             "\xE3\x81\xA7\xE3\x81\x99\xE4\xBA\xBA\xE3\x80\x85",
             "[\xE3\x81\xB2\xE3\x82\x89\xE3\x81\x8C\xE3\x81\xAA][\xE6\xBC\xA2\xE5\xAD\x97]"
             "[\xE3\x81\xA7\xE3\x81\x99][\xE4\xBA\xBA\xE3\x80\x85]") &&
              Is("en", 'w', "\xE6\xBC\xA2\xE5\xAD\x97", "[\xE6\xBC\xA2][\xE5\xAD\x97]"),
          "ja: a Hiragana run and a run of ideographs are each one word (CLDR's 13.3 and "
          "13.4; ICU4C uses its dictionary); not in the root");
}

// Small a, small a (CJ), a, the prolonged sound mark (CJ), i.
#define KANA "\xE3\x81\x82\xE3\x81\x81\xE3\x81\x82\xE3\x83\xBC\xE3\x81\x84"

static void TestLines(void)
{
    const char* strict = "[\xE3\x81\x82\xE3\x81\x81][\xE3\x81\x82\xE3\x83\xBC][\xE3\x81\x84]";
    const char* normal = "[\xE3\x81\x82][\xE3\x81\x81][\xE3\x81\x82][\xE3\x83\xBC][\xE3\x81\x84]";
    CHECK(Is("ja", 'l', KANA, normal) && Is("ja-u-lb-strict", 'l', KANA, strict) &&
              Is("ja-u-lb-loose", 'l', KANA, normal) && Is("und", 'l', KANA, strict) &&
              Is("en-u-lb-normal", 'l', KANA, normal),
          "CJ breaks as an ideograph in ja and with normal or loose, as a nonstarter with "
          "strict and in the root");
    CHECK(Is("zh", 'l', KANA, normal) && Is("zh-Hant", 'l', KANA, normal) &&
              Is("zh-u-lb-strict", 'l', KANA, strict),
          "zh: CLDR's normal (ICU4C's zh default is strict, zh-u-lb-strict here)");
}

// The breaks of a text as ICU4C's table writes them: byte offsets joined
// by commas, the text fed in pieces of the size given (0: whole).
static bool Offsets(const char* id, char kind, const char* text, size_t length, size_t pieceSize,
                    char* out, size_t capacity)
{
    mlocLocale locale;
    mlocSegmenter one;
    mlocSegmenter two;
    size_t fed = pieceSize == 0 || pieceSize > length ? length : pieceSize;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        mlocInitSegmenter(&one, &s_data, &locale, KindOf(kind), text, fed, fed < length) !=
            mloc_success)
    {
        return false;
    }
    mlocSegmenter* s = Moved(&one, &two);
    size_t written = 0;
    out[0] = '\0';
    for (;;)
    {
        size_t offset = 0;
        mlocResult r = mlocNextSegmentBreak(s, &offset, nullptr);
        if (r == mloc_done)
        {
            return true;
        }
        if (r == mloc_needMoreText)
        {
            size_t piece = length - fed < pieceSize ? length - fed : pieceSize;
            if (mlocFeedSegmenter(s, text + fed, piece, fed + piece < length) != mloc_success)
            {
                return false;
            }
            s = s == &one ? Moved(&one, &two) : Moved(&two, &one);
            fed += piece;
            continue;
        }
        int n = snprintf(out + written, capacity - written, written == 0 ? "%zu" : ",%zu", offset);
        if (r != mloc_success || n < 0 || (size_t)n >= capacity - written)
        {
            return false;
        }
        written += (size_t)n;
    }
}

static bool ReadFile(const char* path, char* out, size_t capacity)
{
    FILE* f = fopen(path, "rb");
    size_t size = f == nullptr ? 0 : fread(out, 1, capacity - 1, f);
    if (f != nullptr)
    {
        fclose(f);
    }
    out[size] = '\0';
    return size > 0 && size < capacity - 1;
}

// Whether the exceptions list a case ("locale\tkind\ttext"), marking the
// line found by its reason's first byte.
static bool Listed(const char* item, size_t length)
{
    for (char* line = s_exceptions; *line != '\0';)
    {
        char* end = strchr(line, '\n');
        size_t size = end == nullptr ? strlen(line) : (size_t)(end - line);
        char* tab = memchr(line, '\t', size);
        if (line[0] != '#' && tab != nullptr && (size_t)(line + size - tab - 1) == length &&
            memcmp(tab + 1, item, length) == 0)
        {
            line[0] = '*';
            return true;
        }
        line += size + (end != nullptr);
    }
    return false;
}

// A case's text, with "\n" a line feed.
static size_t Unescape(const char* text, size_t length, char* out)
{
    size_t n = 0;
    for (size_t i = 0; i < length; i++)
    {
        bool lineFeed = text[i] == '\\' && i + 1 < length && text[i + 1] == 'n';
        out[n++] = lineFeed ? '\n' : text[i];
        i += lineFeed;
    }
    return n;
}

static void TestOracle(void)
{
    bool read = ReadFile(MLOC_SEGMENT_ICU_FILE, s_table, sizeof(s_table)) &&
                ReadFile(MLOC_SEGMENT_EXCEPTIONS_FILE, s_exceptions, sizeof(s_exceptions));
    CHECK(read, "the oracle's table and its exceptions are read");
    int cases = 0;
    int unlisted = 0;
    for (char* line = s_table; read && *line != '\0';)
    {
        char* end = strchr(line, '\n');
        *end = '\0';
        char* answer = strrchr(line, '\t');
        char* kind = strchr(line, '\t');
        char* text = strchr(kind + 1, '\t');
        char id[64];
        char raw[512];
        char whole[1024];
        char pieces[1024];
        snprintf(id, sizeof(id), "%.*s", (int)(kind - line), line);
        size_t length = Unescape(text + 1, (size_t)(answer - text - 1), raw);
        bool ok = Offsets(id, kind[1], raw, length, 0, whole, sizeof(whole));
        for (size_t piece = 1; ok && piece <= 3; piece += 2)
        {
            ok = Offsets(id, kind[1], raw, length, piece, pieces, sizeof(pieces)) &&
                 strcmp(pieces, whole) == 0;
        }
        if (!ok || (strcmp(whole, answer + 1) != 0 && !Listed(line, (size_t)(answer - line))))
        {
            printf("  %s: %s, ICU4C %s\n", line, ok ? whole : "fails or differs in pieces",
                   answer + 1);
            unlisted++;
        }
        cases++;
        line = end + 1;
    }
    CHECK(read && cases > 2000 && unlisted == 0,
          "every case as ICU4C answers it, or listed, and the same in pieces");
    int stale = 0;
    for (const char* line = s_exceptions; read && *line != '\0'; line = strchr(line, '\n') + 1)
    {
        if (line[0] != '#' && line[0] != '*')
        {
            printf("  stale: %.*s\n", (int)strcspn(line, "\n"), line);
            stale++;
        }
    }
    CHECK(read && stale == 0, "every exception still differs");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    mlocSegmenter s;
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitSegmenter(&s, &builtin, &locale, mloc_segmentWord, "a", 1, false) ==
                  mloc_errorNotFound,
          "data without the segmentation file");
    CHECK(mlocInitSegmenter(nullptr, &s_data, &locale, mloc_segmentWord, "a", 1, false) ==
                  mloc_errorInvalid &&
              mlocInitSegmenter(&s, &s_data, &locale, (mlocSegmentKind)3, "a", 1, false) ==
                  mloc_errorInvalid &&
              mlocInitSegmenter(&s, &s_data, &locale, mloc_segmentWord, nullptr, 1, false) ==
                  mloc_errorInvalid,
          "a null segmenter, an unknown kind, a null text with a length");
    size_t offset = 0;
    CHECK(mlocInitSegmenter(&s, &s_data, &locale, mloc_segmentSentence, "a", 1, false) ==
                  mloc_success &&
              mlocFeedSegmenter(&s, "b", 1, false) == mloc_errorInvalid &&
              mlocSetSegmenterComplexBreaker(&s, nullptr, nullptr) == mloc_errorInvalid &&
              mlocNextSegmentBreak(&s, &offset, nullptr) == mloc_success && offset == 1 &&
              mlocNextSegmentBreak(&s, &offset, nullptr) == mloc_done,
          "a piece not asked for, a sentence segmenter's complex breaker");
    mlocSegmenter zero = {0};
    CHECK(mlocNextSegmentBreak(&zero, &offset, nullptr) == mloc_errorInvalid &&
              mlocNextSegmentBreak(&s, nullptr, nullptr) == mloc_errorInvalid,
          "a segmenter the library did not make, a null offset");
}

int main(void)
{
    FILE* f = fopen(MLOC_SEGMENTS_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (size == 0 || size == sizeof(s_file) ||
        mlocOpenData(&s_sources[1], s_file, size) != mloc_success)
    {
        fputs("test_segment: cannot open " MLOC_SEGMENTS_FILE "\n", stderr);
        return 2;
    }
    TestSentences();
    TestWords();
    TestLines();
    TestRefusals();
    TestOracle();
    return s_failures == 0 ? 0 : 1;
}
