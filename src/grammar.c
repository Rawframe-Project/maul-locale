// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The inflector (maul-locale/grammar.h, records mloc-0046 and
// mloc-0047): a text's end found and read (a number by its spoken form,
// capitals by their letters' names, a word as written or as the hint
// says), and the language's suffixes written after it with the boundary
// mark its spelling asks for.

#include "maul-locale/grammar.h"

#include "grammar_lexicon.h"
#include "grammar_turkic.h"
#include "number_core.h"

#include "maul-locale/spellout.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"

#include <string.h>

enum
{
    // Marks an inflector the library made.
    inflectorStamp = 0x49,
    languageTurkish = 1,
    languageKazakh = 2,
    // The room for a spoken number's text and a formatted number's.
    spokenRoom = 256,
    numberRoom = 1024,
};

typedef struct Inflector
{
    // The locale's default number formatter: numbers written and read.
    mlocNumberFormatter number;
    // Numbers in words, and ordinals.
    mlocSpelloutFormatter cardinal;
    mlocSpelloutFormatter ordinal;
    uint8_t stamp;
    uint8_t language;
    // Whether the spell-out formatters were made, 1 or 0.
    uint8_t spoken;
} Inflector;

static_assert(sizeof(Inflector) <= sizeof(mlocInflector), "an inflector fits its value");

mlocResult mlocInitInflector(mlocInflector* inflectorOut, const mlocDataSet* data,
                             const mlocLocale* locale)
{
    if (inflectorOut == nullptr || data == nullptr || locale == nullptr)
    {
        return mloc_errorInvalid;
    }
    Inflector f;
    memset(&f, 0, sizeof(f));
    // Each language in the script its rules read.
    bool latin = locale->script[0] == '\0' || memcmp(locale->script, "Latn", 4) == 0;
    bool cyrillic = locale->script[0] == '\0' || memcmp(locale->script, "Cyrl", 4) == 0;
    if (strcmp(locale->language, "tr") == 0 && latin)
    {
        f.language = languageTurkish;
    }
    else if (strcmp(locale->language, "kk") == 0 && cyrillic)
    {
        f.language = languageKazakh;
    }
    else
    {
        return mloc_errorNotFound;
    }
    if (mlocInitNumberFormatter(&f.number, data, locale, nullptr) < 0)
    {
        return mloc_errorNotFound;
    }
    static const char s_cardinal[] = "%spellout-numbering";
    static const char s_ordinal[] = "%spellout-ordinal";
    bool cardinal = mlocInitSpelloutFormatter(&f.cardinal, data, locale, s_cardinal,
                                              sizeof(s_cardinal) - 1, nullptr) == mloc_success;
    bool ordinal = mlocInitSpelloutFormatter(&f.ordinal, data, locale, s_ordinal,
                                             sizeof(s_ordinal) - 1, nullptr) == mloc_success;
    f.spoken = cardinal && ordinal ? 1 : 0;
    f.stamp = inflectorStamp;
    memset(inflectorOut, 0, sizeof(*inflectorOut));
    memcpy(inflectorOut, &f, sizeof(f));
    return mloc_success;
}

// An inflector the library made, copied out; false for any other.
static bool ReadInflector(const mlocInflector* opaque, Inflector* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return f->stamp == inflectorStamp &&
           (f->language == languageTurkish || f->language == languageKazakh) && f->spoken <= 1;
}

// An inflector's language.
static const TurkicLanguage* LanguageOf(const Inflector* f)
{
    return f->language == languageKazakh ? &mlocKazakh : &mlocTurkish;
}

// What a text's end is.
typedef enum EndKind
{
    endWord,
    endLetters,
    endNumber,
    endOrdinal,
} EndKind;

// A text's end: the span read, its kind, and what stands after it.
typedef struct End
{
    size_t start;
    size_t end;
    EndKind kind;
    // A closing quotation mark or bracket after it, or a period after
    // letters: no apostrophe then.
    bool closed;
    bool period;
} End;

// Decodes the code point that ends at text[end], valid UTF-8; its first
// byte's index in startOut.
static uint32_t Before(const uint8_t* text, size_t end, size_t* startOut)
{
    size_t at = end - 1;
    while (at > 0 && (text[at] & 0xC0) == 0x80)
    {
        at--;
    }
    *startOut = at;
    uint32_t c = text[at];
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
    for (size_t i = 1; i <= extra; i++)
    {
        c = c << 6 | (text[at + i] & 0x3F);
    }
    return c;
}

static bool IsLetter(uint32_t c)
{
    muniGeneralCategory g = muniGetGeneralCategory(c);
    return g == muni_gcLu || g == muni_gcLl || g == muni_gcLt || g == muni_gcLm || g == muni_gcLo ||
           g == muni_gcMn || g == muni_gcMc;
}

static bool IsDigit(uint32_t c)
{
    return c >= '0' && c <= '9';
}

// A closing quotation mark or bracket: what TDK writes suffixes after
// without an apostrophe.
static bool IsCloser(uint32_t c)
{
    muniGeneralCategory g = muniGetGeneralCategory(c);
    return g == muni_gcPe || g == muni_gcPf || c == '"' || c == '\'';
}

// The separators a written number holds between its digits.
static bool IsNumberSeparator(uint32_t c)
{
    return c == '.' || c == ',' || c == 0xA0 || c == 0x202F || c == '\'';
}

// Finds the digits that end a text at end: digits with separators
// between them.
static size_t NumberStart(const uint8_t* text, size_t end)
{
    size_t start = end;
    size_t at = end;
    while (at > 0)
    {
        size_t previous = 0;
        uint32_t c = Before(text, at, &previous);
        if (IsDigit(c))
        {
            start = previous;
        }
        else if (!IsNumberSeparator(c) || previous == 0 || at != start)
        {
            break;
        }
        at = previous;
    }
    return start;
}

// Finds the letters that end a text at end, and whether they are all
// capitals.
static size_t WordStart(const uint8_t* text, size_t end, bool* capitalsOut)
{
    size_t at = end;
    size_t letters = 0;
    bool capitals = true;
    while (at > 0)
    {
        size_t previous = 0;
        uint32_t c = Before(text, at, &previous);
        if (!IsLetter(c))
        {
            break;
        }
        muniGeneralCategory g = muniGetGeneralCategory(c);
        capitals = capitals && (g == muni_gcLu || g == muni_gcMn || g == muni_gcMc);
        letters++;
        at = previous;
    }
    *capitalsOut = capitals && letters > 0;
    return at;
}

// Finds and classifies a text's end; false when nothing ends it that can
// be read.
static bool FindEnd(const TurkicLanguage* language, const uint8_t* text, size_t length,
                    mlocInflectionReading reading, End* out)
{
    *out = (End){0};
    size_t end = length;
    size_t previous = 0;
    while (end > 0 && IsCloser(Before(text, end, &previous)))
    {
        out->closed = true;
        end = previous;
    }
    bool dot = end > 0 && text[end - 1] == '.';
    size_t last = dot ? end - 1 : end;
    if (last == 0)
    {
        return false;
    }
    uint32_t c = Before(text, last, &previous);
    out->end = last;
    if (IsDigit(c))
    {
        out->start = NumberStart(text, last);
        out->kind = dot && language->periodOrdinals ? endOrdinal : endNumber;
        out->period = dot && !language->periodOrdinals;
        return true;
    }
    if (!IsLetter(c))
    {
        return false;
    }
    bool capitals = false;
    out->start = WordStart(text, last, &capitals);
    out->period = dot;
    out->kind = reading == mloc_readingLetters                    ? endLetters
                : reading == mloc_readingAuto && capitals && !dot ? endLetters
                                                                  : endWord;
    return true;
}

// Copies the last word of a text into spoken; false when it does not
// fit.
static bool LastWord(const char* text, size_t length, char* spoken, size_t* spokenLength)
{
    size_t start = length;
    while (start > 0 && text[start - 1] != ' ')
    {
        start--;
    }
    if (length - start >= spokenRoom || start == length)
    {
        return false;
    }
    memcpy(spoken, text + start, length - start);
    *spokenLength = length - start;
    return true;
}

// The spoken form of a written number: its last word in words, read from
// the fraction digits it shows when it has them ("2,20" is iki virgül
// yirmi), from the ordinal when a period follows it.
static mlocResult SpokenNumber(const Inflector* f, const uint8_t* text, const End* e, char* spoken,
                               size_t* spokenLength)
{
    if (f->spoken == 0)
    {
        return mloc_errorNotFound;
    }
    mlocDecimal value;
    size_t used = 0;
    size_t length = e->end - e->start;
    const char* digits = (const char*)text + e->start;
    if (mlocParseNumber(&f->number, digits, length, mloc_parseLenient, &value, &used) !=
            mloc_success ||
        used != length)
    {
        // Separators the locale does not read: the last run of digits.
        size_t run = e->end;
        while (run > e->start && IsDigit(text[run - 1]))
        {
            run--;
        }
        digits = (const char*)text + run;
        length = e->end - run;
        if (mlocParseDecimal(digits, length, &value).status != mloc_success)
        {
            return mloc_errorInvalid;
        }
    }
    if (value.kind == mloc_decimalFinite && value.fractionDigits > 0 && e->kind == endNumber)
    {
        // The fraction's digits as the integer they are read as.
        size_t shown = (size_t)value.fractionDigits;
        if (mlocParseDecimal((const char*)text + e->end - shown, shown, &value).status !=
            mloc_success)
        {
            return mloc_errorInvalid;
        }
    }
    char words[spokenRoom];
    mlocOutput out = {words, sizeof(words), 0, nullptr, nullptr};
    mlocResult r =
        mlocFormatSpellout(e->kind == endOrdinal ? &f->ordinal : &f->cardinal, &value, &out);
    if (r != mloc_success || !LastWord(words, out.length, spoken, spokenLength))
    {
        return r == mloc_success || r == mloc_errorCapacity ? mloc_errorInvalid : r;
    }
    return mloc_success;
}

// The spoken form of a text's end, as its kind reads it.
static mlocResult Spoken(const Inflector* f, const uint8_t* text, const End* e, char* spoken,
                         size_t* spokenLength)
{
    if (e->kind == endNumber || e->kind == endOrdinal)
    {
        return SpokenNumber(f, text, e, spoken, spokenLength);
    }
    if (e->kind == endLetters)
    {
        size_t previous = 0;
        const char* name = LanguageOf(f)->letterName(Before(text, e->end, &previous));
        if (name == nullptr)
        {
            return mloc_errorInvalid;
        }
        *spokenLength = strlen(name);
        memcpy(spoken, name, *spokenLength);
        return mloc_success;
    }
    size_t length = e->end - e->start;
    if (length >= spokenRoom)
    {
        // A word that long is read by its last letters alone.
        size_t start = e->end - (spokenRoom - 4);
        while ((text[start] & 0xC0) == 0x80)
        {
            start++;
        }
        length = e->end - start;
        memcpy(spoken, text + start, length);
    }
    else
    {
        memcpy(spoken, text + e->start, length);
    }
    *spokenLength = length;
    return mloc_success;
}

// What a request writes after a text: its suffixes, the boundary mark
// before them, and the stem's changed end (the bytes kept, then a tail).
typedef struct Inflected
{
    TurkicSuffixes suffixes;
    // The mark, or 0 for none.
    char mark;
    size_t kept;
    char tail[12];
    uint8_t tailLength;
} Inflected;

static bool ValidRequest(const Inflector* f, const mlocInflection* r)
{
    return (uint32_t)r->reading <= mloc_readingLetters &&
           (r->hint != nullptr || r->hintLength == 0) && mlocTurkicRequestValid(LanguageOf(f), r);
}

// A compound that ends in a third person's possessive (zeytinyağı): its
// cases take the pronominal n (zeytinyağına); a plural or another
// possessive goes on its roots, the plural keeping the third person's
// (zeytinyağları, zeytinyağım).
static void InflectCompound(const uint8_t* text, size_t length, const mlocInflection* r,
                            const TurkicMarks* marks, TurkicSound sound, Inflected* out)
{
    if (!r->plural && r->possessorPerson == 0)
    {
        mlocTurkicSuffixes(&mlocTurkish, sound, r, true, &out->suffixes);
        return;
    }
    // The possessive is counted in code points: the text's spelling may
    // differ from the lexicon's key in bytes (ZEYTİNYAĞI, zeytinyâğı).
    size_t roots = length;
    for (size_t i = 0; i < marks->possessive && roots > 0; i++)
    {
        (void)Before(text, roots, &roots);
    }
    mlocInflection onRoots = *r;
    onRoots.possessorPerson = r->possessorPerson == 0 ? 3 : r->possessorPerson;
    TurkicSound rootSound;
    if (!mlocTurkicSound(&mlocTurkish, text, roots, (marks->marks & turkicInverse) != 0,
                         &rootSound))
    {
        rootSound = sound;
    }
    mlocTurkicSuffixes(&mlocTurkish, rootSound, &onRoots, false, &out->suffixes);
    out->kept = roots;
}

// The boundary mark a text's end takes before its suffixes, or 0: the
// language's after a number or letters, and after a name or a word in
// another script where it marks them; none on a common noun, after a
// closing mark or a period, or on a native Turkish name's plural (a
// name joins its plural, Ahmetler, unless it is foreign, which a hint,
// its spoken form, says: Nice'ler).
static char Mark(const TurkicLanguage* language, const uint8_t* text, const End* e,
                 const mlocInflection* r, bool hinted, const TurkicSuffixes* suffixes)
{
    if (r->commonNoun || e->closed || e->period || suffixes->particle == 0)
    {
        return 0;
    }
    if (e->kind != endWord)
    {
        return language->mark;
    }
    size_t previous = 0;
    uint32_t last = Before(text, e->end, &previous);
    bool foreign = last < language->scriptFirst || last > language->scriptLast;
    bool name = (language->markOn & turkicMarkNames) != 0 && !(r->plural && !hinted);
    return name || ((language->markOn & turkicMarkForeign) != 0 && foreign) ? language->mark : 0;
}

// Reads the spoken end (the hint's when there is one) and writes the
// suffixes; the text's end decides the boundary mark.
static mlocResult Inflect(const Inflector* f, const uint8_t* text, size_t length,
                          const mlocInflection* r, Inflected* out)
{
    const TurkicLanguage* language = LanguageOf(f);
    End e;
    if (!FindEnd(language, text, length, r->reading, &e))
    {
        return mloc_errorInvalid;
    }
    End spokenEnd = e;
    const uint8_t* spokenText = text;
    if (r->hint != nullptr && r->hintLength > 0)
    {
        spokenText = (const uint8_t*)r->hint;
        if (muniValidateUtf8(r->hint, r->hintLength).status != muni_success ||
            !FindEnd(language, spokenText, r->hintLength, r->reading, &spokenEnd))
        {
            return mloc_errorInvalid;
        }
    }
    char spoken[spokenRoom];
    size_t spokenLength = 0;
    mlocResult result = Spoken(f, spokenText, &spokenEnd, spoken, &spokenLength);
    if (result != mloc_success)
    {
        return result;
    }
    TurkicMarks marks = {0};
    if (spokenEnd.kind == endWord && language->lexicon)
    {
        (void)mlocTurkicMarks((const uint8_t*)spoken, spokenLength, &marks);
    }
    bool inverse = (marks.marks & turkicInverse) != 0;
    TurkicSound sound;
    if (!mlocTurkicSound(language, (const uint8_t*)spoken, spokenLength, inverse, &sound))
    {
        // A word without a vowel is read by its letters (PwC, Brr).
        End letters = spokenEnd;
        letters.kind = endLetters;
        if (spokenEnd.kind != endWord ||
            Spoken(f, spokenText, &letters, spoken, &spokenLength) != mloc_success ||
            !mlocTurkicSound(language, (const uint8_t*)spoken, spokenLength, false, &sound))
        {
            return mloc_errorInvalid;
        }
        marks = (TurkicMarks){0};
    }
    bool stemmed = (r->commonNoun || language->stemNames) && e.kind == endWord && e.end == length &&
                   spokenText == text;
    out->kept = length;
    out->tailLength = 0;
    if (stemmed && (marks.marks & turkicCompound) != 0)
    {
        InflectCompound(text, length, r, &marks, sound, out);
    }
    else
    {
        mlocTurkicSuffixes(language, sound, r, false, &out->suffixes);
        TurkicStem stem;
        if (stemmed && out->suffixes.vowelFirst &&
            language->stem(text + e.start, e.end - e.start, marks.marks, &stem))
        {
            out->kept = e.start + stem.kept;
            memcpy(out->tail, stem.tail, stem.tailLength);
            out->tailLength = stem.tailLength;
        }
    }
    out->mark = Mark(language, text, &e, r, spokenText != text, &out->suffixes);
    return mloc_success;
}

// Writes the inflected text: the stem (kept, then its changed end), the
// boundary mark, the suffixes and the particle.
static void Write(const uint8_t* text, const Inflected* in, NumberWriter* w)
{
    mlocWritePart(w, mloc_inflectionPartStem, text, in->kept);
    mlocWritePart(w, mloc_inflectionPartStem, (const uint8_t*)in->tail, in->tailLength);
    if (in->mark != 0)
    {
        mlocWritePart(w, mloc_inflectionPartApostrophe, (const uint8_t*)&in->mark, 1);
    }
    const TurkicSuffixes* s = &in->suffixes;
    mlocWritePart(w, mloc_inflectionPartSuffix, (const uint8_t*)s->text, s->particle);
    mlocWritePart(w, mloc_inflectionPartParticle, (const uint8_t*)s->text + s->particle,
                  (size_t)(s->length - s->particle));
}

// Writes an inflected text to an output, its parts reported once it
// fits.
static mlocResult Output(const uint8_t* text, const Inflected* in, mlocOutput* output)
{
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    Write(text, in, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        Write(text, in, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

static bool ValidOutput(const mlocOutput* output)
{
    return output != nullptr && (output->buffer != nullptr || output->capacity == 0);
}

mlocResult mlocInflectText(const mlocInflector* inflector, const char* text, size_t length,
                           const mlocInflection* request, mlocOutput* output)
{
    Inflector f;
    if (!ReadInflector(inflector, &f) || text == nullptr || length == 0 || request == nullptr ||
        !ValidOutput(output) || !ValidRequest(&f, request) ||
        muniValidateUtf8(text, length).status != muni_success)
    {
        return mloc_errorInvalid;
    }
    Inflected in;
    mlocResult r = Inflect(&f, (const uint8_t*)text, length, request, &in);
    return r != mloc_success ? r : Output((const uint8_t*)text, &in, output);
}

// Reports a number's own parts inside the stem.
static void ReportNumberParts(const Inflector* f, const mlocNumberFormatter* formatter,
                              const mlocDecimal* decimal, mlocOutput* output)
{
    char scratch[numberRoom];
    mlocOutput parts = {scratch, sizeof(scratch), 0, output->part, output->context};
    (void)mlocFormatNumber(formatter != nullptr ? formatter : &f->number, decimal, &parts);
}

mlocResult mlocInflectNumber(const mlocInflector* inflector, const mlocNumberFormatter* formatter,
                             const mlocDecimal* decimal, const mlocInflection* request,
                             mlocOutput* output)
{
    Inflector f;
    if (!ReadInflector(inflector, &f) || decimal == nullptr || request == nullptr ||
        !ValidOutput(output) || !ValidRequest(&f, request))
    {
        return mloc_errorInvalid;
    }
    char text[numberRoom];
    mlocOutput written = {text, sizeof(text), 0, nullptr, nullptr};
    mlocResult r =
        mlocFormatNumber(formatter != nullptr ? formatter : &f.number, decimal, &written);
    if (r != mloc_success)
    {
        // A number whose text passes the room is refused.
        return r == mloc_errorCapacity ? mloc_errorInvalid : r;
    }
    mlocInflection number = *request;
    number.reading = mloc_readingAuto;
    number.hint = nullptr;
    number.hintLength = 0;
    number.commonNoun = false;
    Inflected in;
    r = Inflect(&f, (const uint8_t*)text, written.length, &number, &in);
    if (r != mloc_success)
    {
        return r;
    }
    mlocOutput plain = *output;
    plain.part = nullptr;
    r = Output((const uint8_t*)text, &in, &plain);
    output->length = plain.length;
    if (r == mloc_success && output->part != nullptr)
    {
        // The stem, then the number's parts inside it, then the rest.
        Inflected stem = in;
        stem.suffixes = (TurkicSuffixes){0};
        stem.mark = 0;
        NumberWriter parts = {.parts = output};
        Write((const uint8_t*)text, &stem, &parts);
        mlocEndParts(&parts);
        ReportNumberParts(&f, formatter, decimal, output);
        Inflected rest = in;
        rest.kept = 0;
        NumberWriter after = {.parts = output, .length = written.length, .start = written.length};
        Write((const uint8_t*)text, &rest, &after);
        mlocEndParts(&after);
    }
    return r;
}
