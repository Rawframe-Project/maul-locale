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
        mlocInitSegmenter(&s_data, &locale, KindOf(kind), text, fed, fed < length, &one) !=
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
        mlocResult r = mlocSegmenter_NextBreak(s, &offset, nullptr);
        if (r == mloc_done)
        {
            out[written] = '\0';
            return at == length;
        }
        if (r == mloc_needMoreText)
        {
            size_t piece = length - fed < pieceSize ? length - fed : pieceSize;
            if (mlocSegmenter_Feed(s, text + fed, piece, fed + piece < length) != mloc_success)
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
    const char* kept = "[\xE3\x81\x82\xE3\x81\x81][\xE3\x81\x82\xE3\x83\xBC][\xE3\x81\x84]";
    const char* apart = "[\xE3\x81\x82][\xE3\x81\x81][\xE3\x81\x82][\xE3\x83\xBC][\xE3\x81\x84]";
    CHECK(Is("ja", 'l', KANA, kept) && Is("ja-u-lb-normal", 'l', KANA, kept) &&
              Is("ja-u-lb-strict", 'l', KANA, kept) && Is("en-u-lb-normal", 'l', KANA, kept) &&
              Is("und", 'l', KANA, kept) && Is("zh", 'l', KANA, kept) &&
              Is("zh-Hant", 'l', KANA, kept),
          "CJ stays with what precedes it in normal and strict, as the CSS Text 3 draft of 14 "
          "August 2026 says (ICU4C's normal breaks before it)");
    CHECK(Is("ja-u-lb-loose", 'l', KANA, apart) && Is("en-u-lb-loose", 'l', KANA, apart),
          "loose breaks before CJ");
    CHECK(Is("en-u-lb-anywhere", 'l', "ab cd", "[ab ][cd]") &&
              Is("en-u-lb-anywhere", 'l', KANA, kept),
          "anywhere, a level of CSS that CLDR's key does not have, breaks as normal");
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
        mlocInitSegmenter(&s_data, &locale, KindOf(kind), text, fed, fed < length, &one) !=
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
        mlocResult r = mlocSegmenter_NextBreak(s, &offset, nullptr);
        if (r == mloc_done)
        {
            return true;
        }
        if (r == mloc_needMoreText)
        {
            size_t piece = length - fed < pieceSize ? length - fed : pieceSize;
            if (mlocSegmenter_Feed(s, text + fed, piece, fed + piece < length) != mloc_success)
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
              mlocInitSegmenter(&builtin, &locale, mloc_segmentWord, "a", 1, false, &s) ==
                  mloc_errorNotFound,
          "data without the segmentation file");
    CHECK(mlocInitSegmenter(&s_data, &locale, mloc_segmentWord, "a", 1, false, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitSegmenter(&s_data, &locale, (mlocSegmentKind)3, "a", 1, false, &s) ==
                  mloc_errorInvalid &&
              mlocInitSegmenter(&s_data, &locale, mloc_segmentWord, nullptr, 1, false, &s) ==
                  mloc_errorInvalid,
          "a null segmenter, an unknown kind, a null text with a length");
    size_t offset = 0;
    CHECK(mlocInitSegmenter(&s_data, &locale, mloc_segmentSentence, "a", 1, false, &s) ==
                  mloc_success &&
              mlocSegmenter_Feed(&s, "b", 1, false) == mloc_errorInvalid &&
              mlocSegmenter_SetComplexBreaker(&s, nullptr, nullptr) == mloc_errorInvalid &&
              mlocSegmenter_NextBreak(&s, &offset, nullptr) == mloc_success && offset == 1 &&
              mlocSegmenter_NextBreak(&s, &offset, nullptr) == mloc_done,
          "a piece not asked for, a sentence segmenter's complex breaker");
    mlocSegmenter zero = {0};
    CHECK(mlocSegmenter_NextBreak(&zero, &offset, nullptr) == mloc_errorInvalid &&
              mlocSegmenter_NextBreak(&s, nullptr, nullptr) == mloc_errorInvalid,
          "a segmenter the library did not make, a null offset");
}

// Whether a call that finds no boundary, by mloc_done or
// mloc_needMoreText, clears the outputs the caller set: for words, lines,
// and sentences with suppressions (filtered).
static bool ClearsAtTheEnd(const char* id, mlocSegmentKind kind, bool moreFollows)
{
    mlocLocale locale;
    mlocSegmenter s;
    const char* text = "Mr. Smith left. He did";
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        mlocInitSegmenter(&s_data, &locale, kind, text, strlen(text), moreFollows, &s) !=
            mloc_success)
    {
        return false;
    }
    for (int i = 0; i < 64; i++)
    {
        size_t offset = 12345;
        bool mandatory = true;
        mlocResult r = mlocSegmenter_NextBreak(&s, &offset, &mandatory);
        if (r != mloc_success)
        {
            return r == (moreFollows ? mloc_needMoreText : mloc_done) && offset == 0 && !mandatory;
        }
    }
    return false;
}

static void TestCleared(void)
{
    bool all = true;
    for (int more = 0; more <= 1; more++)
    {
        all = ClearsAtTheEnd("en", mloc_segmentWord, more) &&
              ClearsAtTheEnd("en", mloc_segmentLine, more) &&
              ClearsAtTheEnd("en-u-ss-standard", mloc_segmentSentence, more) && all;
    }
    CHECK(all, "no boundary found: the offset 0 and not mandatory");
    mlocSegmenter zero = {0};
    size_t offset = 12345;
    bool mandatory = true;
    CHECK(mlocSegmenter_NextBreak(&zero, &offset, &mandatory) == mloc_errorInvalid &&
              offset == 12345 && mandatory,
          "a segmenter the library did not make: the outputs left as they were");
    mlocLocale en;
    mlocSegmenter s;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitSegmenter(&s_data, &en, mloc_segmentLine, "a b", 3, false, &s) ==
                  mloc_success &&
              mlocSegmenter_NextBreak(&s, nullptr, &mandatory) == mloc_errorInvalid && mandatory,
          "a null offset: mandatoryOut left as it was");
}

// A complex breaker that calls back into the segmenter it serves: a break
// every three bytes of the run, and, when it calls back, each call it
// could make on its segmenter, counting those not refused.
typedef struct Reentry
{
    bool callBack;
    mlocSegmenter* segmenter;
    int calls;
    int allowed;
    int written;
} Reentry;

static size_t ReenteringBreaker(void* context, const char* run, size_t length, size_t from)
{
    (void)run;
    Reentry* r = context;
    if (r->callBack)
    {
        // Values the refused call must leave: the call being inside it may
        // be filling outputs of its own there.
        size_t offset = 12345;
        bool mandatory = true;
        r->calls++;
        r->allowed +=
            (mlocSegmenter_NextBreak(r->segmenter, &offset, &mandatory) != mloc_errorInvalid) +
            (mlocSegmenter_Feed(r->segmenter, "x", 1, false) != mloc_errorInvalid) +
            (mlocSegmenter_SetComplexBreaker(r->segmenter, nullptr, nullptr) != mloc_errorInvalid);
        r->written += offset != 12345 || !mandatory;
    }
    return from + 3 < length ? from + 3 : length;
}

// The breaks of a text fed in pieces of step bytes; their count, or -1.
static int BreaksInPieces(const char* id, mlocSegmentKind kind, const char* text, size_t step,
                          Reentry* reentry, size_t* breaks, int capacity)
{
    mlocLocale locale;
    mlocSegmenter s;
    size_t length = strlen(text);
    size_t fed = step < length ? step : length;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        mlocInitSegmenter(&s_data, &locale, kind, text, fed, fed < length, &s) != mloc_success ||
        mlocSegmenter_SetComplexBreaker(&s, ReenteringBreaker, reentry) != mloc_success)
    {
        return -1;
    }
    reentry->segmenter = &s;
    int count = 0;
    for (;;)
    {
        size_t offset = 0;
        mlocResult r = mlocSegmenter_NextBreak(&s, &offset, nullptr);
        if (r == mloc_success && count < capacity)
        {
            breaks[count++] = offset;
        }
        else if (r == mloc_needMoreText)
        {
            size_t more = length - fed < step ? length - fed : step;
            if (mlocSegmenter_Feed(&s, text + fed, more, fed + more < length) != mloc_success)
            {
                return -1;
            }
            fed += more;
        }
        else
        {
            return r == mloc_done ? count : -1;
        }
    }
}

static void TestReentry(void)
{
    static const char* const s_texts[] = {
        "ひらがなภาษาไทยかんじ漢字ภาษาひら",
        "かなภาษาไทยภาษาไทย。漢字ກະເຣີ",
        "ภาษาไทยひらがなภาษา",
    };
    static const char* const s_ids[] = {"ja", "th", "en"};
    int differ = 0;
    int calls = 0;
    int allowed = 0;
    int written = 0;
    for (size_t t = 0; t < sizeof(s_texts) / sizeof(s_texts[0]); t++)
    {
        for (size_t l = 0; l < sizeof(s_ids) / sizeof(s_ids[0]); l++)
        {
            for (int kind = mloc_segmentWord; kind <= mloc_segmentLine; kind += 2)
            {
                for (size_t step = 1; step <= 8; step++)
                {
                    size_t plain[64];
                    size_t reentered[64];
                    Reentry none = {0};
                    Reentry back = {.callBack = true};
                    int a = BreaksInPieces(s_ids[l], (mlocSegmentKind)kind, s_texts[t], step, &none,
                                           plain, 64);
                    int b = BreaksInPieces(s_ids[l], (mlocSegmentKind)kind, s_texts[t], step, &back,
                                           reentered, 64);
                    differ +=
                        a < 0 || a != b ||
                        memcmp(plain, reentered, (size_t)(a > 0 ? a : 0) * sizeof(size_t)) != 0;
                    calls += back.calls;
                    allowed += back.allowed;
                    written += back.written;
                }
            }
        }
    }
    CHECK(calls > 500 && allowed == 0 && differ == 0,
          "a complex breaker's calls back into its segmenter are refused and change no break");
    CHECK(calls > 500 && written == 0,
          "a complex breaker's call back into NextBreak leaves the outputs it was given");
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
        mlocOpenData(s_file, size, &s_sources[1]) != mloc_success)
    {
        fputs("test_segment: cannot open " MLOC_SEGMENTS_FILE "\n", stderr);
        return 2;
    }
    TestSentences();
    TestWords();
    TestLines();
    TestRefusals();
    TestCleared();
    TestReentry();
    TestOracle();
    return s_failures == 0 ? 0 : 1;
}
