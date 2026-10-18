// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Finnish suffixes (record mloc-0049). A word's type and gradation come
// from Kotus's word list (src/generated/finnish_types.h: the words its
// endings do not decide, then the endings) for a common noun, a
// compound by its last word, and from the ending alone for a name; each
// type is a row of patterns below, read on the word's base (the word
// less the letters the type drops). Gradation weakens the stem's last
// strong consonants before a closed syllable (katu, kadun), and in the
// types that end in a consonant strengthens the vowel stem (rengas,
// renkaan). Digits and capitals take a colon and the ending as the
// Kielenhuollon käsikirja writes it (5:een, 3:a, 100:aa, EU:ssa).

#include "grammar_fi.h"

#include "grammar_fi_word.h"

#include "generated/finnish_types.h"
#include "maul-unicode/encoding.h"

#include <string.h>

// A type's row: the letters its base drops from the word; the vowel
// stem's ending on the base (_ the dropped letters again, V the base's
// last vowel again); the singular partitive (after the vowel stem when
// it begins with +, else on the base; - drops one more letter); the
// singular illative after the vowel stem; the plural stem's ending on
// the base; the plural partitive (j: the stem's i as j and A; else
// after the stem), genitive (~ on the base) and illative; a default
// gradation; whether the plural stem is always weak.
typedef struct FinnishType
{
    uint8_t drop;
    const char* vowel;
    const char* partitive;
    const char* illative;
    const char* plural;
    const char* pluralPartitive;
    const char* pluralGenitive;
    const char* pluralIllative;
    char grade;
    bool weakPlural;
} FinnishType;

static const FinnishType s_types[50] = {
    [1] = {0, "", "+A", "Vn", "i", "j", "jen", "hin", 0, false},
    [2] = {0, "", "+A", "Vn", "i", "j", "jen", "hin", 0, false},
    [3] = {0, "", "+tA", "Vn", "i", "tA", "den", "hin", 0, false},
    [4] = {0, "", "+A", "Vn", "i", "tA", "den", "hin", 0, true},
    [5] = {1, "i", "+A", "Vn", "ei", "j", "~ien", "hin", 0, false},
    [6] = {1, "i", "+A", "Vn", "ei", "j", "~ien", "hin", 0, false},
    [7] = {1, "e", "+A", "Vn", "i", "A", "en", "in", 0, false},
    [8] = {0, "", "+A", "Vn", "i", "j", "jen", "hin", 0, false},
    [9] = {1, "_", "+A", "Vn", "Oi", "j", "jen", "hin", 0, false},
    [10] = {1, "_", "+A", "Vn", "i", "A", "en", "in", 0, false},
    [11] = {1, "_", "+A", "Vn", "Oi", "tA", "den", "hin", 0, false},
    [12] = {1, "_", "+A", "Vn", "Oi", "tA", "den", "hin", 0, false},
    [13] = {1, "_", "+A", "Vn", "Oi", "j", "jen", "hin", 0, false},
    [14] = {1, "_", "+A", "Vn", "Oi", "tA", "den", "hin", 0, true},
    [15] = {1, "_", "+A", "Vn", "i", "tA", "den", "siin", 0, false},
    [16] = {1, "A", "+A", "Vn", "i", "A", "en", "in", 'H', false},
    [17] = {1, "_", "+tA", "seen", "i", "tA", "den", "siin", 0, false},
    [18] = {1, "_", "+tA", "hVn", "i", "tA", "den", "hin", 0, false},
    [19] = {2, "_", "+tA", "hVn", "Li", "tA", "den", "hin", 0, false},
    [20] = {1, "_", "+tA", "hVn", "i", "tA", "den", "hin", 0, false},
    [21] = {0, "", "+tA", "hVn", "i", "tA", "den", "hin", 0, false},
    [22] = {0, "", "+tA", "hVn", "i", "tA", "den", "hin", 0, false},
    [23] = {1, "e", "tA", "Vn", "i", "A", "en", "in", 0, false},
    [24] = {1, "e", "tA", "Vn", "i", "A", "en", "in", 0, false},
    [25] = {1, "e", "+A", "Vn", "i", "A", "en", "in", 0, false},
    [26] = {1, "e", "tA", "Vn", "i", "A", "en", "in", 0, false},
    [27] = {2, "te", "ttA", "Vn", "si", "A", "en", "in", 'F', false},
    [28] = {2, "te", "ttA", "Vn", "si", "A", "en", "in", 'J', false},
    [29] = {2, "se", "-stA", "Vn", "si", "A", "en", "in", 0, false},
    [30] = {2, "se", "-stA", "Vn", "si", "A", "en", "in", 0, false},
    [31] = {3, "hte", "htA", "Vn", "ksi", "A", "en", "in", 'F', false},
    [32] = {0, "e", "tA", "Vn", "i", "A", "en", "in", 0, false},
    [33] = {1, "me", "ntA", "Vn", "mi", "A", "en", "in", 0, false},
    [34] = {2, "OmA", "OntA", "Vn", "Omi", "A", "en", "in", 0, false},
    [35] = {2, "imA", "intA", "Vn", "imi", "A", "en", "in", 'H', false},
    [36] = {2, "immA", "intA", "Vn", "immi", "A", "en", "in", 0, false},
    [37] = {2, "emmA", "entA", "Vn", "emmi", "A", "en", "in", 0, false},
    [38] = {3, "se", "stA", "Vn", "si", "A", "en", "in", 0, false},
    [39] = {1, "kse", "stA", "Vn", "ksi", "A", "en", "in", 0, false},
    [40] = {1, "te", "ttA", "Vn", "ksi", "A", "en", "in", 'F', false},
    [41] = {1, "V", "stA", "seen", "i", "tA", "den", "siin", 0, false},
    [42] = {1, "he", "stA", "Vn", "hi", "A", "en", "in", 0, false},
    [43] = {1, "e", "ttA", "Vn", "i", "tA", "den", "hin", 0, false},
    [44] = {1, "V", "ttA", "seen", "i", "tA", "den", "siin", 0, false},
    [45] = {1, "nte", "ttA", "Vn", "nsi", "A", "en", "in", 'J', false},
    [46] = {1, "nte", "ttA", "Vn", "nsi", "A", "en", "in", 'J', false},
    [47] = {2, "ee", "UttA", "seen", "ei", "tA", "den", "siin", 0, false},
    [48] = {0, "e", "ttA", "seen", "i", "tA", "den", "siin", 0, false},
    [49] = {0, "e", "tA", "Vn", "i", "A", "en", "in", 0, false},
};

// The types whose nominative ends in a consonant: gradation strengthens
// their vowel stem (rengas, renkaan); not type 40, whose -uus grades its
// vowel stem as a vowel-final word does (kalleuden, kalleuteen).
static bool IsInverse(int type)
{
    return type >= 32 && type != 40;
}

// A word's type, gradation letter (0 for none) and where its last part
// begins (a compound's last word, found in the table; 0 for the word).
typedef struct Kind
{
    int type;
    char grade;
    size_t from;
} Kind;

// Finds a key in a table blob of records (length, bytes, type, grade).
static bool Find(const uint8_t* blob, const uint16_t* offsets, size_t count, const uint8_t* key,
                 size_t length, Kind* out)
{
    size_t low = 0;
    size_t high = count;
    while (low < high)
    {
        size_t mid = low + (high - low) / 2;
        const uint8_t* record = blob + offsets[mid];
        size_t n = record[0];
        int order = memcmp(record + 1, key, n < length ? n : length);
        order = order != 0 ? order : n < length ? -1 : n > length ? 1 : 0;
        if (order == 0)
        {
            out->type = record[n + 1];
            out->grade = (char)record[n + 2];
            return true;
        }
        if (order < 0)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }
    return false;
}

// Encodes the small letters w[from, count) as UTF-8; the length, or 0.
static size_t Key(const FinnishWord* w, size_t from, uint8_t* key, size_t room)
{
    size_t n = 0;
    for (size_t i = from; i < w->count; i++)
    {
        char bytes[4];
        size_t used = 0;
        if (muniEncodeUtf8(mlocFinnishSmall(w->c[i]), bytes, &used) != muni_success ||
            n + used > room)
        {
            return 0;
        }
        memcpy(key + n, bytes, used);
        n += used;
    }
    return n;
}

// A common noun's kind by Kotus: the word, a compound's last word of
// three letters or more after two or more, else the longest ending.
static bool EndsWith(const FinnishWord* w, const char* s);
static Kind NameKind(const FinnishWord* w);

static Kind CommonKind(const FinnishWord* w)
{
    uint8_t key[finnishLetterRoom * 4];
    Kind kind = {finnishDefaultType, (char)finnishDefaultGrade, 0};
    size_t words = sizeof(s_finnishWordsOffsets) / sizeof(s_finnishWordsOffsets[0]);
    size_t endings = sizeof(s_finnishEndingsOffsets) / sizeof(s_finnishEndingsOffsets[0]);
    for (size_t from = 0; from + 3 <= w->count; from = from == 0 ? 2 : from + 1)
    {
        size_t n = Key(w, from, key, sizeof(key));
        if (n > 0 && Find(s_finnishWords, s_finnishWordsOffsets, words, key, n, &kind))
        {
            kind.from = from;
            return kind;
        }
    }
    size_t longest = w->count < finnishLongestEnding ? w->count : finnishLongestEnding;
    for (size_t k = longest; k > 0; k--)
    {
        size_t n = Key(w, w->count - k, key, sizeof(key));
        if (n > 0 && Find(s_finnishEndings, s_finnishEndingsOffsets, endings, key, n, &kind))
        {
            return kind;
        }
    }
    // No ending the list knows: -nen as the list's most words, another
    // word as a name ending so (a loan, letters the list lacks).
    return EndsWith(w, "nen") ? kind : NameKind(w);
}

// Whether the letters before end are the ASCII string s.
static bool EndsWith(const FinnishWord* w, const char* s)
{
    size_t n = strlen(s);
    if (w->count < n)
    {
        return false;
    }
    for (size_t i = 0; i < n; i++)
    {
        if (mlocFinnishSmall(w->c[w->count - n + i]) != (uint8_t)s[i])
        {
            return false;
        }
    }
    return true;
}

// A name's kind by its ending: -nen 38, -s after a vowel 39, another
// consonant 5 with an i, -i 5, -e 48 in three syllables or more and 8
// in two, a long vowel 17 or 18, a diphthong in one syllable 19, -a or
// -ä 9 or 12 after i, -o, -u, -y, -ö 1; a double stop or nk before the
// last vowel grades (Pekan, Matin, Helsingin).
static size_t Syllables(const FinnishWord* w)
{
    size_t vowels = 0;
    for (size_t i = 0; i < w->count; i++)
    {
        vowels +=
            mlocFinnishIsVowel(w->c[i]) && (i == 0 || !mlocFinnishIsVowel(w->c[i - 1])) ? 1 : 0;
    }
    return vowels;
}

static int NameType(const FinnishWord* w)
{
    size_t n = w->count;
    size_t vowels = Syllables(w);
    uint32_t last = n > 0 ? mlocFinnishSmall(w->c[n - 1]) : 0;
    uint32_t before = n > 1 ? mlocFinnishSmall(w->c[n - 2]) : 0;
    if (EndsWith(w, "nen"))
    {
        return 38;
    }
    if (!mlocFinnishIsVowel(last))
    {
        return last == 's' && mlocFinnishIsVowel(before) && vowels >= 2 ? 39 : 5;
    }
    if (mlocFinnishIsVowel(before) && (before == last || vowels == 1))
    {
        return before != last ? 19 : vowels == 1 ? 18 : 17;
    }
    if (last == 'e')
    {
        return vowels >= 3 ? 48 : 8;
    }
    if (last == 'a' || last == 0xE4)
    {
        return before == 'i' ? 12 : 9;
    }
    return last == 'i' ? 5 : 1;
}

static char NameGrade(const FinnishWord* w)
{
    size_t n = w->count;
    if (n < 3 || !mlocFinnishIsVowel(w->c[n - 1]) || mlocFinnishIsVowel(w->c[n - 2]))
    {
        return 0;
    }
    uint32_t before = mlocFinnishSmall(w->c[n - 2]);
    uint32_t third = mlocFinnishSmall(w->c[n - 3]);
    if (third == before)
    {
        return before == 'k' ? 'A' : before == 'p' ? 'B' : before == 't' ? 'C' : 0;
    }
    return third == 'n' && before == 'k' ? 'G' : 0;
}

static Kind NameKind(const FinnishWord* w)
{
    Kind kind = {NameType(w), NameGrade(w), 0};
    return kind;
}

// The stems of a word of a kind: the base, the vowel stem in each grade,
// the plural stem in each grade.
typedef struct Stems
{
    const FinnishType* type;
    int number;
    char grade;
    bool back;
    uint32_t last;
    FinnishWord base;
    FinnishWord strong;
    FinnishWord weak;
    FinnishWord pluralStrong;
    FinnishWord pluralWeak;
} Stems;

static void MakeStems(const FinnishWord* word, Kind kind, bool back, Stems* s)
{
    int type = kind.type >= 1 && kind.type <= 49 ? kind.type : 5;
    s->type = &s_types[type];
    s->number = type;
    s->grade = kind.grade != 0 ? kind.grade : s->type->grade;
    s->back = back;
    s->last = word->count > 0 ? mlocFinnishSmall(word->c[word->count - 1]) : 'a';
    s->base = *word;
    // A type 5 or 6 word that ends in a consonant takes an i (Bush, Bushin).
    bool consonant = (type == 5 || type == 6) && !mlocFinnishIsVowel(s->last);
    size_t drop = consonant ? 0 : s->type->drop;
    // Type 18's long vowel shortens before the plural's i (maa: maissa), a
    // diphthong stays (tau: tauissa).
    if (type == 18 && word->count >= 2 &&
        mlocFinnishSmall(word->c[word->count - 1]) != mlocFinnishSmall(word->c[word->count - 2]))
    {
        drop = 0;
    }
    s->base.count = word->count > drop ? word->count - drop : 0;
    s->strong = s->base;
    if (strcmp(s->type->vowel, "_") == 0)
    {
        s->strong = *word;
    }
    else
    {
        mlocFinnishAppend(&s->strong, s->type->vowel, back, s->last);
    }
    s->pluralStrong = s->base;
    mlocFinnishAppend(&s->pluralStrong, s->type->plural, back, s->last);
    s->weak = s->strong;
    s->pluralWeak = s->pluralStrong;
    if (IsInverse(type))
    {
        mlocFinnishGrade(&s->strong, s->grade, false);
        mlocFinnishGrade(&s->pluralStrong, s->grade, false);
        s->weak = s->strong;
        s->pluralWeak = s->pluralStrong;
        return;
    }
    mlocFinnishGrade(&s->weak, s->grade, true);
    mlocFinnishGrade(&s->pluralWeak, s->grade, true);
    if (s->type->weakPlural)
    {
        s->pluralStrong = s->pluralWeak;
    }
}

// The endings of the cases that join a stem the same way in both
// numbers, by grade: weak before the closed syllable they make.
typedef struct CaseEnding
{
    mlocGrammaticalCase grammaticalCase;
    const char* ending;
    bool strong;
} CaseEnding;

static const CaseEnding s_endings[] = {
    {mloc_caseGenitive, "n", false},   {mloc_caseInessive, "ssA", false},
    {mloc_caseElative, "stA", false},  {mloc_caseAdessive, "llA", false},
    {mloc_caseAblative, "ltA", false}, {mloc_caseAllative, "lle", false},
    {mloc_caseEssive, "nA", true},     {mloc_caseTranslative, "ksi", false},
    {mloc_caseAbessive, "ttA", false}, {mloc_caseInstrumental, "n", false},
    {mloc_caseComitative, "ne", true},
};

static const CaseEnding* EndingOf(mlocGrammaticalCase c)
{
    for (size_t i = 0; i < sizeof(s_endings) / sizeof(s_endings[0]); i++)
    {
        if (s_endings[i].grammaticalCase == c)
        {
            return &s_endings[i];
        }
    }
    return nullptr;
}

bool mlocFinnishRequestValid(const mlocInflection* request)
{
    mlocGrammaticalCase c = request->grammaticalCase;
    bool hasCase = c == mloc_caseNominative || c == mloc_casePartitive || c == mloc_caseIllative ||
                   EndingOf(c) != nullptr;
    return hasCase && request->possessorPerson <= 3 &&
           (request->possessorPerson > 0 || !request->possessorPlural) && !request->copula;
}

// The plural stem's i as j before a vowel (kaloja, kalojen).
static void Glide(FinnishWord* w)
{
    if (w->count > 0 && mlocFinnishSmall(w->c[w->count - 1]) == 'i')
    {
        w->c[w->count - 1] = 'j';
    }
}

// The ending of a case the table of endings holds; "" for none.
static const char* TableEnding(const CaseEnding* e)
{
    return e != nullptr ? e->ending : "";
}

// A singular case form: the partitive on the base where the type says,
// else the vowel stem in the grade the ending asks.
static void SingularForm(const Stems* s, mlocGrammaticalCase c, FinnishWord* out, size_t* start)
{
    const FinnishType* t = s->type;
    const CaseEnding* e = EndingOf(c);
    if (c == mloc_casePartitive && t->partitive[0] != '+')
    {
        *out = s->base;
        const char* p = t->partitive;
        if (p[0] == '-')
        {
            out->count = out->count > 0 ? out->count - 1 : 0;
            p++;
        }
        *start = out->count;
        mlocFinnishAppend(out, p, s->back, s->last);
        return;
    }
    bool strong = c == mloc_casePartitive || c == mloc_caseIllative || (e != nullptr && e->strong);
    *out = strong ? s->strong : s->weak;
    *start = out->count;
    const char* ending = c == mloc_casePartitive  ? t->partitive + 1
                         : c == mloc_caseIllative ? t->illative
                                                  : TableEnding(e);
    mlocFinnishAppend(out, ending, s->back, s->last);
}

// A plural case form: the plural stem in the grade the ending asks, or
// the base where the ending says (~); j as the stem's i where it says.
static void PluralForm(const Stems* s, mlocGrammaticalCase c, FinnishWord* out, size_t* start)
{
    const FinnishType* t = s->type;
    const CaseEnding* e = EndingOf(c);
    bool strong = c == mloc_casePartitive || c == mloc_caseGenitive || c == mloc_caseIllative ||
                  (e != nullptr && e->strong);
    const char* ending = c == mloc_casePartitive  ? t->pluralPartitive
                         : c == mloc_caseGenitive ? t->pluralGenitive
                         : c == mloc_caseIllative ? t->pluralIllative
                                                  : TableEnding(e);
    if (ending[0] == '~')
    {
        *out = s->base;
        ending++;
    }
    else
    {
        *out = strong ? s->pluralStrong : s->pluralWeak;
    }
    if (strcmp(ending, "j") == 0 || strcmp(ending, "jen") == 0)
    {
        Glide(out);
        ending = ending[1] == '\0' ? "A" : "en";
    }
    *start = out->count;
    mlocFinnishAppend(out, ending, s->back, s->last);
}

// Writes a case form into out; start receives where its stem ends. The
// instructive and comitative are plural forms (käsin, vaimoineen).
static void Form(const Stems* s, const FinnishWord* word, mlocGrammaticalCase c, bool plural,
                 FinnishWord* out, size_t* start)
{
    if (c == mloc_caseNominative)
    {
        *out = plural ? s->weak : *word;
        *start = out->count;
        mlocFinnishAppend(out, plural ? "t" : "", s->back, 0);
    }
    else if (plural || c == mloc_caseInstrumental || c == mloc_caseComitative)
    {
        PluralForm(s, c, out, start);
    }
    else
    {
        SingularForm(s, c, out, start);
    }
}

// The possessive suffix on a case form: -ni, -si, -mme, -nne, and the
// third person's -nsA after the nominative and genitive and a
// consonant, else the last vowel again and n (talossaan). The
// nominative and genitive take the strong vowel stem (katuni), the
// illative drops its n (katuuni), the translative's -ksi is -kse-.
static void Possessive(const Stems* s, const mlocInflection* r, FinnishWord* form, size_t* start)
{
    mlocGrammaticalCase c = r->grammaticalCase;
    bool bare = c == mloc_caseNominative || (c == mloc_caseGenitive && !r->plural);
    if (bare)
    {
        *form = s->strong;
        *start = form->count;
    }
    else if (c == mloc_caseIllative || (c == mloc_caseGenitive && r->plural))
    {
        form->count--;
    }
    else if (c == mloc_caseTranslative)
    {
        form->c[form->count - 1] = 'e';
    }
    else if (c == mloc_caseComitative)
    {
        // The comitative's -ine takes the suffix directly.
    }
    static const char* const s_suffixes[2][2] = {{"ni", "si"}, {"mme", "nne"}};
    if (r->possessorPerson < 3)
    {
        mlocFinnishAppend(form, s_suffixes[r->possessorPlural ? 1 : 0][r->possessorPerson - 1],
                          s->back, 0);
    }
    else if (!bare && form->count > 0 && mlocFinnishIsVowel(form->c[form->count - 1]) &&
             c != mloc_caseComitative)
    {
        mlocFinnishAppend(form, "Vn", s->back, 0);
    }
    else
    {
        mlocFinnishAppend(form, c == mloc_caseComitative ? "en" : "nsA", s->back, 0);
    }
}

// A numeral's written endings after a colon (the Kielenhuollon
// käsikirja's table: 3:a, 5:tä, 100:aa; 3:een, 7:ään, 100:aan).
typedef struct Numeral
{
    const char* word;
    bool back;
    const char* partitive;
    const char* illative;
} Numeral;

static const Numeral s_numerals[] = {
    {"nolla", true, "AA", "AAn"},         {"yksi", false, "tA", "een"},
    {"kaksi", true, "tA", "een"},         {"kolme", true, "A", "een"},
    {"nelj\xC3\xA4", false, "AA", "AAn"}, {"viisi", false, "tA", "een"},
    {"kuusi", true, "tA", "een"},         {"seitsem\xC3\xA4n", false, "AA", "AAn"},
    {"kahdeksan", true, "AA", "AAn"},     {"yhdeks\xC3\xA4n", false, "AA", "AAn"},
    {"kymmenen", false, "tA", "een"},     {"sata", true, "AA", "AAn"},
    {"tuhat", true, "tA", "een"},         {"miljoona", true, "AA", "AAn"},
    {"miljardi", true, "A", "iin"},       {"biljoona", true, "AA", "AAn"},
};

// The written forms of a number's last word, as CLDR spells it, that its
// endings follow: 11 to 19 by their first part (seitsemäntoista), and a
// multiple of ten, a hundred, a thousand or a million by its word
// (kaksikymmentä, kymmenen).
static const char* const s_multiples[][2] = {
    {"kymment\xC3\xA4", "kymmenen"}, {"sataa", "sata"},         {"tuhatta", "tuhat"},
    {"miljoonaa", "miljoona"},       {"miljardia", "miljardi"}, {"biljoonaa", "biljoona"},
};

static const Numeral* NumeralOf(const FinnishWord* spoken)
{
    uint8_t text[finnishLetterRoom * 4];
    size_t n = Key(spoken, 0, text, sizeof(text));
    static const char s_teen[] = "toista";
    if (n > sizeof(s_teen) - 1 && memcmp(text + n - (sizeof(s_teen) - 1), s_teen, 6) == 0)
    {
        n -= sizeof(s_teen) - 1;
    }
    for (size_t i = 0; i < sizeof(s_multiples) / sizeof(s_multiples[0]); i++)
    {
        size_t m = strlen(s_multiples[i][0]);
        size_t w = strlen(s_multiples[i][1]);
        if (n >= m && memcmp(text + n - m, s_multiples[i][0], m) == 0)
        {
            memcpy(text + n - m, s_multiples[i][1], w);
            n = n - m + w;
        }
    }
    const Numeral* best = nullptr;
    for (size_t i = 0; i < sizeof(s_numerals) / sizeof(s_numerals[0]); i++)
    {
        size_t m = strlen(s_numerals[i].word);
        if (n >= m && memcmp(text + n - m, s_numerals[i].word, m) == 0 &&
            (best == nullptr || m > strlen(best->word)))
        {
            best = &s_numerals[i];
        }
    }
    return best;
}

// The endings after a colon: a numeral's from its table; an ordinal's as
// type 45 writes them (5:nnessä) or 38 for -nen (1:sessä); a letter's
// name by its end, a long vowel (EU:ta, EU:hun) or the vowel its
// consonant name takes (F:ää, F:ään).
static void OrdinalEndings(const FinnishWord* spoken, mlocGrammaticalCase c, bool back,
                           FinnishWord* out)
{
    // The cases whose ending joins the ordinal's stem otherwise: -nen's,
    // then type 45's.
    static const struct
    {
        mlocGrammaticalCase grammaticalCase;
        const char* endings[2];
    } s_own[4] = {
        {mloc_casePartitive, {"stA", "ttA"}},
        {mloc_caseIllative, {"seen", "nteen"}},
        {mloc_caseEssive, {"senA", "ntenA"}},
        {mloc_caseGenitive, {"sen", "nnen"}},
    };
    size_t which = EndsWith(spoken, "nen") ? 0 : 1;
    for (size_t i = 0; i < 4; i++)
    {
        if (s_own[i].grammaticalCase == c)
        {
            mlocFinnishAppend(out, s_own[i].endings[which], back, 0);
            return;
        }
    }
    if (EndingOf(c) != nullptr)
    {
        mlocFinnishAppend(out, which == 0 ? "se" : "nne", back, 0);
        mlocFinnishAppend(out, TableEnding(EndingOf(c)), back, 0);
    }
}

// A number's or a letter's endings: the partitive and illative as the
// numeral's table or the long vowel says, the rest from the table.
static void SaidEndings(const FinnishWord* spoken, const Numeral* numeral, mlocGrammaticalCase c,
                        bool back, FinnishWord* out)
{
    uint32_t last = spoken->count > 0 ? mlocFinnishSmall(spoken->c[spoken->count - 1]) : 'a';
    bool longVowel = spoken->count >= 2 && mlocFinnishIsVowel(last) &&
                     mlocFinnishSmall(spoken->c[spoken->count - 2]) == last;
    if (c == mloc_casePartitive)
    {
        mlocFinnishAppend(out,
                          numeral != nullptr ? numeral->partitive
                          : longVowel        ? "tA"
                                             : "AA",
                          back, 0);
    }
    else if (c == mloc_caseIllative && longVowel && numeral == nullptr)
    {
        // A long vowel's illative: h, the vowel, n (EU:hun, BBC:hen).
        mlocFinnishPush(out, 'h');
        mlocFinnishPush(out, last);
        mlocFinnishPush(out, 'n');
    }
    else if (c == mloc_caseIllative)
    {
        mlocFinnishAppend(out, numeral != nullptr ? numeral->illative : "AAn", back, 0);
    }
    else
    {
        mlocFinnishAppend(out, TableEnding(EndingOf(c)), back, 0);
    }
}

static bool MarkedEndings(const FinnishText* t, const FinnishWord* spoken, mlocGrammaticalCase c,
                          FinnishWord* out)
{
    const Numeral* numeral = t->number && !t->ordinal ? NumeralOf(spoken) : nullptr;
    if (t->number && !t->ordinal && numeral == nullptr)
    {
        return false;
    }
    bool back = numeral != nullptr ? numeral->back : mlocFinnishIsBack(spoken);
    out->count = 0;
    if (t->ordinal)
    {
        OrdinalEndings(spoken, c, back, out);
    }
    else
    {
        SaidEndings(spoken, numeral, c, back, out);
    }
    return true;
}

const char* mlocFinnishLetterName(const uint8_t* text, size_t end)
{
    static const char* const s_names[26] = {
        "aa",  "bee",        "see",        "dee",       "ee",        "\xC3\xA4\x66", "gee",
        "hoo", "ii",         "jii",        "koo",       "\xC3\xA4l", "\xC3\xA4m",    "\xC3\xA4n",
        "oo",  "pee",        "kuu",        "\xC3\xA4r", "\xC3\xA4s", "tee",          "uu",
        "vee", "kaksoisvee", "\xC3\xA4ks", "yy",        "tset"};
    if (end == 0)
    {
        return nullptr;
    }
    size_t start = 0;
    uint32_t c = mlocFinnishSmall(mlocFinnishPrevious(text, end, &start));
    if (c >= 'a' && c <= 'z')
    {
        return s_names[c - 'a'];
    }
    return c == 0xE5   ? "oo"
           : c == 0xE4 ? "\xC3\xA4\xC3\xA4"
           : c == 0xF6 ? "\xC3\xB6\xC3\xB6"
                       : nullptr;
}

// Encodes code points into a buffer; its length (what fits).
static uint8_t Encode(const FinnishWord* w, size_t from, size_t to, char* buffer, size_t room)
{
    size_t n = 0;
    for (size_t i = from; i < to; i++)
    {
        char bytes[4];
        size_t used = 0;
        if (muniEncodeUtf8(w->c[i], bytes, &used) != muni_success || n + used > room)
        {
            break;
        }
        memcpy(buffer + n, bytes, used);
        n += used;
    }
    return (uint8_t)n;
}

static bool HasVowel(const FinnishWord* w)
{
    for (size_t i = 0; i < w->count; i++)
    {
        if (mlocFinnishIsVowel(w->c[i]))
        {
            return true;
        }
    }
    return false;
}

// Digits, capitals, a period or a closing mark: a colon and the endings.
static bool InflectMarked(const FinnishText* t, const FinnishWord* spoken, const mlocInflection* r,
                          FinnishForm* out)
{
    mlocGrammaticalCase c = r->grammaticalCase;
    if (r->plural || r->possessorPerson > 0 || c == mloc_caseInstrumental ||
        c == mloc_caseComitative)
    {
        return false;
    }
    FinnishWord endings = {0};
    if (c != mloc_caseNominative && !MarkedEndings(t, spoken, c, &endings))
    {
        return false;
    }
    if (r->question)
    {
        mlocFinnishAppend(&endings, "kO", mlocFinnishIsBack(spoken), 0);
    }
    out->length = Encode(&endings, 0, endings.count, out->suffixes, sizeof(out->suffixes));
    out->mark = out->length > 0 ? ':' : 0;
    return true;
}

bool mlocFinnishInflect(const FinnishText* t, const mlocInflection* r, FinnishForm* out)
{
    *out = (FinnishForm){.kept = t->wordLength};
    FinnishWord spoken;
    size_t spokenFrom = 0;
    mlocFinnishReadWord(t->spoken, t->spokenLength, &spoken, &spokenFrom);
    if (!HasVowel(&spoken))
    {
        return false;
    }
    if (t->marked)
    {
        return InflectMarked(t, &spoken, r, out);
    }
    FinnishWord word;
    size_t wordFrom = 0;
    mlocFinnishReadWord(t->word, t->wordLength, &word, &wordFrom);
    if (word.count == 0)
    {
        return false;
    }
    // A hinted name whose written end is not said takes an apostrophe and
    // the endings of how it is said (Bordeaux'ssa).
    bool unsaid = t->hinted && mlocFinnishSmall(word.c[word.count - 1]) !=
                                   mlocFinnishSmall(spoken.c[spoken.count - 1]);
    const FinnishWord* stem = unsaid ? &spoken : &word;
    Kind kind = t->name ? NameKind(&spoken) : CommonKind(&spoken);
    Stems s;
    // A compound's harmony is its last word's (meriherkkusienessä).
    FinnishWord last = {.count = 0};
    for (size_t i = kind.from; i < spoken.count; i++)
    {
        last.c[last.count++] = spoken.c[i];
    }
    MakeStems(stem, kind, mlocFinnishIsBack(&last), &s);
    FinnishWord form;
    size_t start = 0;
    Form(&s, stem, r->grammaticalCase, r->plural, &form, &start);
    if (r->possessorPerson > 0)
    {
        Possessive(&s, r, &form, &start);
    }
    else if (r->grammaticalCase == mloc_caseComitative)
    {
        mlocFinnishAppend(&form, "en", s.back, 0);
    }
    if (r->question)
    {
        mlocFinnishAppend(&form, "kO", s.back, 0);
    }
    size_t same = 0;
    while (same < form.count && same < stem->count && form.c[same] == stem->c[same])
    {
        same++;
    }
    if (unsaid)
    {
        out->mark = same < form.count ? '\'' : 0;
        out->length = Encode(&form, same, form.count, out->suffixes, sizeof(out->suffixes));
        return true;
    }
    char kept[finnishLetterRoom * 4];
    out->kept = wordFrom + Encode(&word, 0, same, kept, sizeof(kept));
    size_t split = start > same ? start : same;
    out->tailLength = Encode(&form, same, split, out->tail, sizeof(out->tail));
    out->length = Encode(&form, split, form.count, out->suffixes, sizeof(out->suffixes));
    return true;
}
