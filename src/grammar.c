// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The inflector (maul-locale/grammar.h): a text's end found and read (a
// number by its spoken form, capitals by their letters' names, a word as
// written or as the hint says), and the language's suffixes written after
// it with the boundary mark its spelling asks for.

#include "maul-locale/grammar.h"

#include "grammar_end.h"
#include "grammar_fi.h"
#include "grammar_hu.h"
#include "grammar_hu_sound.h"
#include "grammar_ko.h"
#include "grammar_lexicon.h"
#include "grammar_turkic.h"
#include "invariant.h"
#include "number_core.h"
#include "number_format.h"
#include "number_output.h"

#include "maul-locale/spellout.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"

#include <stddef.h>
#include <string.h>

enum
{
    // Marks an inflector the library made.
    inflectorStamp = 0x49,
    languageTurkish = 1,
    languageKazakh = 2,
    languageKyrgyz = 3,
    languageAzerbaijani = 4,
    languageUzbek = 5,
    languageTurkmen = 6,
    languageHungarian = 7,
    languageFinnish = 8,
    languageKorean = 9,
    // The room for a spoken number's text and a formatted number's.
    spokenRoom = MLOC_MAX_SPOKEN_NUMBER,
    numberRoom = MLOC_MAX_INFLECTED_NUMBER,
};

// An inflector as its value stores it.
typedef struct StoredInflector
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
} StoredInflector;

static_assert(sizeof(StoredInflector) <= sizeof(mlocInflector), "an inflector fits its value");

// An inflector as a call reads it: its few bytes copied, its formatters
// read in place (each is a value of uint64_t words, as its home is).
typedef struct Inflector
{
    const mlocNumberFormatter* number;
    const mlocSpelloutFormatter* cardinal;
    const mlocSpelloutFormatter* ordinal;
    // The value a number's text shows, when the caller gave it
    // (mlocInflector_InflectNumber): read instead of the text.
    const mlocDecimal* shown;
    uint8_t stamp;
    uint8_t language;
    uint8_t spoken;
} Inflector;

// The language whose rules a locale reads, in the script they read; 0
// for none.
static uint8_t LanguageOfLocale(const mlocLocale* locale)
{
    static const struct
    {
        char language[4];
        // The scripts the rules read, the first the default's.
        char scripts[2][5];
        uint8_t id;
    } s_languages[9] = {
        {"tr", {"Latn", ""}, languageTurkish},    {"kk", {"Cyrl", ""}, languageKazakh},
        {"ky", {"Cyrl", ""}, languageKyrgyz},     {"az", {"Latn", ""}, languageAzerbaijani},
        {"uz", {"Latn", ""}, languageUzbek},      {"tk", {"Latn", ""}, languageTurkmen},
        {"hu", {"Latn", ""}, languageHungarian},  {"fi", {"Latn", ""}, languageFinnish},
        {"ko", {"Kore", "Hang"}, languageKorean},
    };
    for (size_t i = 0; i < 9; i++)
    {
        if (strcmp(locale->language, s_languages[i].language) != 0)
        {
            continue;
        }
        bool script = locale->script[0] == '\0';
        for (size_t k = 0; k < 2 && !script; k++)
        {
            script = s_languages[i].scripts[k][0] != '\0' &&
                     memcmp(locale->script, s_languages[i].scripts[k], 4) == 0;
        }
        return script ? s_languages[i].id : 0;
    }
    return 0;
}

mlocResult mlocInitInflector(const mlocDataSet* data, const mlocLocale* locale,
                             mlocInflector* inflectorOut)
{
    if (inflectorOut == nullptr || data == nullptr || locale == nullptr)
    {
        return mloc_errorInvalid;
    }
    StoredInflector f;
    memset(&f, 0, sizeof(f));
    f.language = LanguageOfLocale(locale);
    if (f.language == 0)
    {
        return mloc_errorNotFound;
    }
    if (mlocInitNumberFormatter(data, locale, nullptr, &f.number) < 0)
    {
        return mloc_errorNotFound;
    }
    static const char s_cardinal[] = "%spellout-numbering";
    static const char s_ordinal[] = "%spellout-ordinal";
    bool cardinal = mlocInitSpelloutFormatter(data, locale, s_cardinal, sizeof(s_cardinal) - 1,
                                              nullptr, &f.cardinal) == mloc_success;
    bool ordinal = mlocInitSpelloutFormatter(data, locale, s_ordinal, sizeof(s_ordinal) - 1,
                                             nullptr, &f.ordinal) == mloc_success;
    // Korean writes no ordinal with a period, so it reads only cardinals.
    f.spoken = cardinal && (ordinal || f.language == languageKorean) ? 1 : 0;
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
    const uint8_t* stored = (const uint8_t*)opaque;
    memcpy(&f->stamp, stored + offsetof(StoredInflector, stamp), 1);
    memcpy(&f->language, stored + offsetof(StoredInflector, language), 1);
    memcpy(&f->spoken, stored + offsetof(StoredInflector, spoken), 1);
    f->number = (const mlocNumberFormatter*)(stored + offsetof(StoredInflector, number));
    f->cardinal = (const mlocSpelloutFormatter*)(stored + offsetof(StoredInflector, cardinal));
    f->ordinal = (const mlocSpelloutFormatter*)(stored + offsetof(StoredInflector, ordinal));
    f->shown = nullptr;
    return f->stamp == inflectorStamp && f->language >= languageTurkish &&
           f->language <= languageKorean && f->spoken <= 1;
}

static bool IsTurkic(const Inflector* f)
{
    return f->language <= languageTurkmen;
}

// A Turkic inflector's language.
static const TurkicLanguage* LanguageOf(const Inflector* f)
{
    static const TurkicLanguage* const s_languages[] = {
        &mlocTurkish, &mlocKazakh, &mlocKyrgyz, &mlocAzerbaijani, &mlocUzbek, &mlocTurkmen};
    // ReadInflector checked the language, and its callers ask a Turkic one.
    MLOC_ASSERT(f->language >= languageTurkish && f->language <= languageTurkmen);
    return s_languages[f->language - languageTurkish];
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
// The last word of an integer by a language's own number words (a
// language without spell-out rules); false when it has none for it.
static bool NumberWord(const TurkicLanguage* language, const mlocDecimal* value, char* spoken,
                       size_t* spokenLength)
{
    const char* word = value->count == 0 ? language->numberWord(0, 0)
                       : value->exponent < 0
                           ? nullptr
                           : language->numberWord(value->digits[value->count - 1], value->exponent);
    if (word == nullptr)
    {
        return false;
    }
    *spokenLength = strlen(word);
    // The languages' number words are a few letters (milliard, kvintillion).
    MLOC_ASSERT(*spokenLength < spokenRoom);
    memcpy(spoken, word, *spokenLength);
    return true;
}

// Reads digits as a decimal: mloc_errorLimit past its bounds, else
// mloc_errorInvalid when they are not a number.
static mlocResult ReadDigits(const uint8_t* text, size_t length, mlocDecimal* value)
{
    mlocResult r = mlocParseDecimal((const char*)text, length, value).status;
    return r == mloc_success || r == mloc_errorLimit ? r : mloc_errorInvalid;
}

// Reads the number a text's end writes, by the locale's lenient parse, as
// ReadDigits does.
static mlocResult ReadNumber(const Inflector* f, const uint8_t* text, const End* e,
                             mlocDecimal* value)
{
    size_t used = 0;
    size_t length = e->end - e->start;
    const char* digits = (const char*)text + e->start;
    if (mlocNumberFormatter_Parse(f->number, digits, length, mloc_parseLenient, value, &used) ==
            mloc_success &&
        used == length)
    {
        return mloc_success;
    }
    // Separators the locale does not read: the last run of digits.
    size_t run = e->end;
    while (run > e->start && IsDigit(text[run - 1]))
    {
        run--;
    }
    return ReadDigits(text + run, e->end - run, value);
}

// Reads a number's fraction digits as the integer they are read as, as
// ReadDigits does; mloc_errorInvalid when the number that ends the text
// has fewer: data the loader accepts may give a numbering system digits
// that are not ASCII ones, or an affix that ends in a digit, so a shown
// value's fraction need not end the text.
static mlocResult ReadFraction(const uint8_t* text, const End* e, mlocDecimal* value)
{
    size_t shown = (size_t)value->fractionDigits;
    if (shown > e->end - e->start)
    {
        return mloc_errorInvalid;
    }
    return ReadDigits(text + e->end - shown, shown, value);
}

static mlocResult SpokenNumber(const Inflector* f, const uint8_t* text, const End* e, char* spoken,
                               size_t* spokenLength)
{
    const TurkicLanguage* language = IsTurkic(f) ? LanguageOf(f) : nullptr;
    bool coded = language != nullptr && language->numberWord != nullptr;
    if (f->spoken == 0 && !coded)
    {
        return mloc_errorNotFound;
    }
    // A number the caller gave is read from its value, the text from its
    // characters.
    mlocDecimal value;
    if (f->shown != nullptr && e->kind == endNumber)
    {
        value = *f->shown;
    }
    else
    {
        mlocResult read = ReadNumber(f, text, e, &value);
        if (read != mloc_success)
        {
            return read;
        }
    }
    if (value.kind == mloc_decimalFinite && value.fractionDigits > 0 && e->kind == endNumber)
    {
        mlocResult read = ReadFraction(text, e, &value);
        if (read != mloc_success)
        {
            return read;
        }
    }
    if (coded)
    {
        return NumberWord(language, &value, spoken, spokenLength) ? mloc_success
                                                                  : mloc_errorNotFound;
    }
    char words[spokenRoom];
    mlocOutput out = {words, sizeof(words), 0, nullptr, nullptr};
    mlocResult r = mlocSpelloutFormatter_Format(e->kind == endOrdinal ? f->ordinal : f->cardinal,
                                                &value, &out);
    if (r != mloc_success || !LastWord(words, out.length, spoken, spokenLength))
    {
        return r == mloc_errorCapacity ? mloc_errorLimit
               : r == mloc_success     ? mloc_errorInvalid
                                       : r;
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
        const char* name = IsTurkic(f) ? LanguageOf(f)->letterName(Before(text, e->end, &previous))
                           : f->language == languageFinnish ? mlocFinnishLetterName(text, e->end)
                           : f->language == languageKorean  ? mlocKoreanLetterName(text, e->end)
                                                            : mlocHungarianLetterName(text, e->end);
        if (name == nullptr)
        {
            return mloc_errorInvalid;
        }
        *spokenLength = strlen(name);
        // The letters' names are a few letters (kaksoisvee, elipszilon).
        MLOC_ASSERT(*spokenLength < spokenRoom);
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
        // Valid UTF-8 ends with a code point's last byte, so the letters
        // kept are not empty.
        MLOC_ASSERT(start < e->end);
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
    // A Turkic stem's tail, a Hungarian one or a Finnish stem's change,
    // each written into a room of its own that the asserts below keep
    // within this one (finnishTailRoom the largest), so that copying it
    // here fits in every build; or a Korean pronoun's stem, one syllable.
    char tail[finnishTailRoom];
    uint8_t tailLength;
} Inflected;

static_assert(sizeof(((TurkicStem*)nullptr)->tail) <= sizeof(((Inflected*)nullptr)->tail),
              "a Turkic stem's tail fits");
static_assert(hungarianTailRoom <= sizeof(((Inflected*)nullptr)->tail), "a Hungarian tail fits");
static_assert(finnishTailRoom <= sizeof(((Inflected*)nullptr)->tail), "a Finnish tail fits");
static_assert(hungarianSuffixRoom <= sizeof(((Inflected*)nullptr)->suffixes.text),
              "Hungarian suffixes fit");
static_assert(finnishSuffixRoom <= sizeof(((Inflected*)nullptr)->suffixes.text),
              "Finnish suffixes fit");

static bool ValidRequest(const Inflector* f, const mlocInflection* r)
{
    return (uint32_t)r->reading <= mloc_readingLetters &&
           (r->hint != nullptr || r->hintLength == 0) &&
           (f->language == languageKorean || r->particle == mloc_particleNone) &&
           (IsTurkic(f)                      ? mlocTurkicRequestValid(LanguageOf(f), r)
            : f->language == languageFinnish ? mlocFinnishRequestValid(r)
            : f->language == languageKorean  ? mlocKoreanRequestValid(r)
                                             : mlocHungarianRequestValid(r));
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
// language's after a number or letters, and after a name, a word in
// another script or a hinted (foreign) name where it marks them; none
// on a common noun, after a closing mark or a period, or on a native
// Turkish name's plural (a name joins its plural, Ahmetler, unless it
// is foreign, which a hint, its spoken form, says: Nice'ler).
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
    bool marked = name || ((language->markOn & turkicMarkForeign) != 0 && foreign) ||
                  ((language->markOn & turkicMarkHinted) != 0 && hinted);
    return marked ? language->mark : 0;
}

// A text's end and how it is said: the hint's end when there is one.
typedef struct Reading
{
    End e;
    End spokenEnd;
    const uint8_t* spokenText;
    char spoken[spokenRoom];
    size_t spokenLength;
} Reading;

static mlocResult Read(const Inflector* f, const uint8_t* text, size_t length,
                       const mlocInflection* r, Reading* out)
{
    bool periodOrdinals =
        IsTurkic(f) ? LanguageOf(f)->periodOrdinals : f->language != languageKorean;
    if (!FindEnd(periodOrdinals, text, length, r->reading, &out->e))
    {
        return mloc_errorInvalid;
    }
    out->spokenEnd = out->e;
    out->spokenText = text;
    if (r->hint != nullptr && r->hintLength > 0)
    {
        out->spokenText = (const uint8_t*)r->hint;
        if (muniValidateUtf8(r->hint, r->hintLength).status != muni_success ||
            !FindEnd(periodOrdinals, out->spokenText, r->hintLength, r->reading, &out->spokenEnd))
        {
            return mloc_errorInvalid;
        }
    }
    return Spoken(f, out->spokenText, &out->spokenEnd, out->spoken, &out->spokenLength);
}

// Hungarian: the end read as the Turkic languages read it, then the
// suffixes after a hyphen (digits, capitals, a period or a closing mark)
// or joined to the word, whose end they may change.
static mlocResult InflectHungarian(const Inflector* f, const uint8_t* text, size_t length,
                                   const mlocInflection* r, Inflected* out)
{
    Reading reading;
    mlocResult result = Read(f, text, length, r, &reading);
    if (result != mloc_success)
    {
        return result;
    }
    const End* e = &reading.e;
    HungarianText t = {
        .word = text + e->start,
        .wordLength = e->end - e->start,
        .spoken = (const uint8_t*)reading.spoken,
        .spokenLength = reading.spokenLength,
        .marked = e->kind != endWord || e->closed || e->period || e->capitals || e->end != length,
        .name = !r->commonNoun,
        .hinted = reading.spokenText != text,
    };
    HungarianForm form;
    if (!mlocHungarianInflect(&t, r, &form))
    {
        // A word without a vowel is read by its letters (Brr, PwC).
        End letters = reading.spokenEnd;
        letters.kind = endLetters;
        if (reading.spokenEnd.kind != endWord ||
            Spoken(f, reading.spokenText, &letters, reading.spoken, &reading.spokenLength) !=
                mloc_success)
        {
            return mloc_errorInvalid;
        }
        t.spokenLength = reading.spokenLength;
        t.marked = true;
        if (!mlocHungarianInflect(&t, r, &form))
        {
            return mloc_errorInvalid;
        }
    }
    // A hyphen or no suffix keeps the whole text; a joined suffix may
    // change the word's end, which is then the text's end.
    bool hyphen = form.hyphen && form.length > 0;
    bool whole = hyphen || form.length == 0;
    out->kept = whole ? length : e->start + form.kept;
    MLOC_ASSERT(form.tailLength <= sizeof(out->tail));
    memcpy(out->tail, form.tail, form.tailLength);
    out->tailLength = whole ? 0 : form.tailLength;
    out->mark = hyphen ? '-' : 0;
    out->suffixes = (TurkicSuffixes){0};
    memcpy(out->suffixes.text, form.suffixes, form.length);
    out->suffixes.length = form.length;
    out->suffixes.particle = form.length;
    return mloc_success;
}

// Finnish: the end read as the others read it, then the endings after a
// colon (digits, capitals, a period or a closing mark), after an
// apostrophe (a hinted name's unsaid end), or joined to the word's
// changed stem.
static mlocResult InflectFinnish(const Inflector* f, const uint8_t* text, size_t length,
                                 const mlocInflection* r, Inflected* out)
{
    Reading reading;
    mlocResult result = Read(f, text, length, r, &reading);
    if (result != mloc_success)
    {
        return result;
    }
    const End* e = &reading.e;
    FinnishText t = {
        .word = text + e->start,
        .wordLength = e->end - e->start,
        .spoken = (const uint8_t*)reading.spoken,
        .spokenLength = reading.spokenLength,
        .marked = e->kind != endWord || e->closed || e->period || e->capitals || e->end != length,
        .number = reading.spokenEnd.kind == endNumber || reading.spokenEnd.kind == endOrdinal,
        .ordinal = reading.spokenEnd.kind == endOrdinal,
        .name = !r->commonNoun,
        .hinted = reading.spokenText != text,
    };
    FinnishForm form;
    if (!mlocFinnishInflect(&t, r, &form))
    {
        return mloc_errorInvalid;
    }
    // An ordinal's endings follow its digits, not its period (5:nnen).
    bool whole = form.mark != 0 || form.length + form.tailLength == 0;
    // An ordinal's period gives way to the colon when it ends the text
    // ("5:nnen"); after a closing mark the text stays whole.
    bool period = t.ordinal && form.length > 0 && e->end + 1 == length;
    out->kept = !whole ? e->start + form.kept : period ? e->end : length;
    MLOC_ASSERT(form.tailLength <= sizeof(out->tail));
    memcpy(out->tail, form.tail, form.tailLength);
    out->tailLength = whole ? 0 : form.tailLength;
    out->mark = form.mark;
    out->suffixes = (TurkicSuffixes){0};
    memcpy(out->suffixes.text, form.suffixes, form.length);
    out->suffixes.length = form.length;
    out->suffixes.particle = form.length;
    return mloc_success;
}

// The particle on a Korean text, by the final its spoken end is read
// with; a parenthesized gloss after a word leaves the word to read
// ("서울(Seoul)은"), a percent sign reads 퍼센트. The text stays whole.
static mlocResult InflectKorean(const Inflector* f, const uint8_t* text, size_t length,
                                const mlocInflection* r, Inflected* out)
{
    size_t read = length;
    if (length > 0 && text[length - 1] == ')')
    {
        size_t open = length - 1;
        while (open > 0 && text[open - 1] != '(')
        {
            open--;
        }
        read = open > 1 && text[open - 2] != ' ' ? open - 1 : length;
    }
    KoreanFinal final = koreanVowel;
    if (read == 0 || text[read - 1] != '%')
    {
        Reading reading;
        mlocResult result = Read(f, text, read, r, &reading);
        if (result != mloc_success)
        {
            return result;
        }
        final = mlocKoreanFinal((const uint8_t*)reading.spoken, reading.spokenLength);
    }
    if (final == koreanUnknown && r->exact)
    {
        return mloc_errorNotFound;
    }
    out->kept = length;
    out->tailLength = 0;
    out->mark = 0;
    out->suffixes = (TurkicSuffixes){0};
    // A pronoun's own stem before 가 (내가, 누가), the word alone after a
    // space or the text's start.
    size_t word = length;
    while (word > 0 && text[word - 1] != ' ')
    {
        word--;
    }
    const char* stem = r->particle == mloc_particleSubject && r->hint == nullptr
                           ? mlocKoreanSubjectStem(text + word, length - word)
                           : nullptr;
    if (stem != nullptr)
    {
        out->kept = word;
        out->tailLength = (uint8_t)strlen(stem);
        MLOC_ASSERT(out->tailLength <= sizeof(out->tail));
        memcpy(out->tail, stem, out->tailLength);
    }
    size_t n =
        mlocKoreanParticleText(r->particle, final, out->suffixes.text, sizeof(out->suffixes.text));
    out->suffixes.length = (uint8_t)n;
    out->suffixes.particle = (uint8_t)n;
    return mloc_success;
}

// Reads the spoken end (the hint's when there is one) and writes the
// suffixes; the text's end decides the boundary mark.
static mlocResult Inflect(const Inflector* f, const uint8_t* text, size_t length,
                          const mlocInflection* r, Inflected* out)
{
    if (f->language == languageKorean)
    {
        return InflectKorean(f, text, length, r, out);
    }
    if (f->language == languageFinnish)
    {
        return InflectFinnish(f, text, length, r, out);
    }
    if (!IsTurkic(f))
    {
        return InflectHungarian(f, text, length, r, out);
    }
    const TurkicLanguage* language = LanguageOf(f);
    Reading reading;
    mlocResult result = Read(f, text, length, r, &reading);
    if (result != mloc_success)
    {
        return result;
    }
    End e = reading.e;
    End spokenEnd = reading.spokenEnd;
    const uint8_t* spokenText = reading.spokenText;
    char* spoken = reading.spoken;
    size_t spokenLength = reading.spokenLength;
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
        if (stemmed && out->suffixes.particle > 0 &&
            language->stem(text + e.start, e.end - e.start, marks.marks, &out->suffixes, &stem))
        {
            out->kept = e.start + stem.kept;
            MLOC_ASSERT(stem.tailLength <= sizeof(out->tail));
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
    MLOC_ASSERT(s->particle <= s->length);
    mlocWritePart(w, mloc_inflectionPartSuffix, (const uint8_t*)s->text, s->particle);
    mlocWritePart(w, mloc_inflectionPartParticle, (const uint8_t*)s->text + s->particle,
                  (size_t)(s->length - s->particle));
}

typedef struct InflectedWriting
{
    const uint8_t* text;
    const Inflected* in;
} InflectedWriting;

static mlocResult WriteInflectedText(const void* context, NumberWriter* w)
{
    const InflectedWriting* g = context;
    Write(g->text, g->in, w);
    return mloc_success;
}

// Writes an inflected text to an output, its parts reported once it
// fits.
static mlocResult Output(const uint8_t* text, const Inflected* in, mlocOutput* output)
{
    InflectedWriting g = {text, in};
    return mlocWriteOutput(WriteInflectedText, &g, output);
}

static bool ValidOutput(const mlocOutput* output)
{
    return output != nullptr && (output->buffer != nullptr || output->capacity == 0);
}

mlocResult mlocInflector_InflectText(const mlocInflector* inflector, const char* text,
                                     size_t length, const mlocInflection* request,
                                     mlocOutput* textOut)
{
    Inflector f;
    if (!ReadInflector(inflector, &f) || text == nullptr || length == 0 || request == nullptr ||
        !ValidOutput(textOut) || !ValidRequest(&f, request) ||
        muniValidateUtf8(text, length).status != muni_success)
    {
        return mloc_errorInvalid;
    }
    Inflected in;
    mlocResult r = Inflect(&f, (const uint8_t*)text, length, request, &in);
    return r != mloc_success ? r : Output((const uint8_t*)text, &in, textOut);
}

// Reports a number's own parts inside the stem.
static void ReportNumberParts(const Inflector* f, const mlocNumberFormatter* formatter,
                              const mlocDecimal* decimal, mlocOutput* output)
{
    char scratch[numberRoom];
    mlocOutput parts = {scratch, sizeof(scratch), 0, output->part, output->context};
    (void)mlocNumberFormatter_Format(formatter != nullptr ? formatter : f->number, decimal, &parts);
}

mlocResult mlocInflector_InflectNumber(const mlocInflector* inflector,
                                       const mlocNumberFormatter* formatter,
                                       const mlocDecimal* decimal, const mlocInflection* request,
                                       mlocOutput* textOut)
{
    Inflector f;
    if (!ReadInflector(inflector, &f) || decimal == nullptr || request == nullptr ||
        !ValidOutput(textOut) || !ValidRequest(&f, request))
    {
        return mloc_errorInvalid;
    }
    // The formatter read once: it writes the text and gives the value the
    // text shows, which is spoken without parsing the text back.
    Formatter numberFormat;
    const mlocNumberFormatter* chosen = formatter != nullptr ? formatter : f.number;
    if (!mlocReadFormatter(chosen, sizeof(*chosen), numberFormatterStamp, &numberFormat))
    {
        return mloc_errorInvalid;
    }
    char text[numberRoom];
    mlocOutput written = {text, sizeof(text), 0, nullptr, nullptr};
    mlocResult r = mlocFormatWith(&numberFormat, nullptr, decimal, &written);
    mlocDecimal shown;
    f.shown =
        r == mloc_success && mlocShownDecimal(&numberFormat, decimal, &shown) ? &shown : nullptr;
    if (r != mloc_success)
    {
        // A number whose text passes the room is refused.
        return r == mloc_errorCapacity ? mloc_errorLimit : r;
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
    mlocOutput plain = *textOut;
    plain.part = nullptr;
    r = Output((const uint8_t*)text, &in, &plain);
    textOut->length = plain.length;
    if (r == mloc_success && textOut->part != nullptr)
    {
        // The stem, then the number's parts inside it, then the rest.
        Inflected stem = in;
        stem.suffixes = (TurkicSuffixes){0};
        stem.mark = 0;
        NumberWriter parts = {.parts = textOut};
        Write((const uint8_t*)text, &stem, &parts);
        mlocEndParts(&parts);
        ReportNumberParts(&f, formatter, decimal, textOut);
        Inflected rest = in;
        rest.kept = 0;
        NumberWriter after = {.parts = textOut, .length = written.length, .start = written.length};
        Write((const uint8_t*)text, &rest, &after);
        mlocEndParts(&after);
    }
    return r;
}
