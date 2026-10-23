// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Hungarian suffixes by the Academy's rules (AkH 12), on a word's sound
// (grammar_hu_sound.h): each suffix in order, its linking vowel (o, e, ö;
// a and e after a lowering stem or a suffix; u and ü in the possessives'
// plural), a final a or e lengthened before it (AkH 29, 216), the v of
// -val, -vel, -vá, -vé assimilated to the last consonant (AkH 82, 160). A
// joined word's stem changes as a table says (vizet, bokrot, lovat), a
// digraph doubles in its first letter (Kodállyal), three same letters are
// written two in a common noun (tollal) and keep a hyphen in a name
// (Papp-pal).

#include "grammar_hu.h"

#include "grammar_hu_sound.h"

#include "maul-unicode/encoding.h"

#include <string.h>

// The suffixes being written, and how their end sounds.
typedef struct Builder
{
    char text[hungarianSuffixRoom];
    uint8_t length;
    HungarianSound sound;
    // The linking vowel is a or e (a lowering stem, or after a suffix).
    bool lowering;
    // The table's accusative takes a linking vowel.
    bool linking;
    // Whether the stem's final vowel is lengthened.
    bool lengthen;
    // The third person's possessive is -ja on the word as it is (útja), or
    // -a where the rule says -ja (tolla).
    bool ownJ;
    bool noJ;
    // The word is a name.
    bool name;
    bool started;
} Builder;

static void Put(Builder* b, const char* s)
{
    size_t n = strlen(s);
    if (b->length + n < sizeof(b->text))
    {
        memcpy(b->text + b->length, s, n);
        b->length = (uint8_t)(b->length + n);
    }
}

// Before a suffix: a short a or e that ends what is written lengthens
// (the stem's in the builder's flag), except before -ként.
static void Lengthen(Builder* b, bool keeps)
{
    if (!b->started)
    {
        b->lengthen = b->sound.vowelEnd && b->sound.vowel != 0 && !keeps;
        return;
    }
    if (keeps || b->length == 0)
    {
        return;
    }
    char last = b->text[b->length - 1];
    if (last == 'a' || last == 'e')
    {
        b->length--;
        Put(b, last == 'a' ? "\xC3\xA1" : "\xC3\xA9");
    }
}

// Notes that a suffix is written: the rest are after one.
static void Start(Builder* b)
{
    b->started = true;
}

// The sound after the builder's text ends: a vowel or a consonant, read
// from its last letters.
static void Ended(Builder* b)
{
    HungarianLetters w;
    mlocHungarianReadLetters((const uint8_t*)b->text, b->length, &w);
    HungarianHarmony harmony = b->sound.harmony;
    mlocHungarianReadSound(&w, &b->sound);
    b->sound.harmony = harmony;
    b->sound.hasVowel = true;
    b->lowering = true;
    b->linking = false;
}

// The form of a suffix by harmony: back, front, front rounded.
static const char* ByHarmony(HungarianHarmony h, const char* back, const char* front,
                             const char* round)
{
    return h == hungarianHarmonyBack ? back : h == hungarianHarmonyFront ? front : round;
}

typedef enum Link
{
    // o, e, ö; a and e after a lowering stem.
    linkLow,
    // o, e, ö always (the superessive).
    linkMid,
    // u, ü (the possessives' first and third plural).
    linkHigh,
} Link;

static const char* LinkingVowel(const Builder* b, Link link)
{
    HungarianHarmony h = b->sound.harmony;
    if (link == linkHigh)
    {
        return h == hungarianHarmonyBack ? "u" : "\xC3\xBC";
    }
    if (link == linkLow && b->lowering)
    {
        return h == hungarianHarmonyBack ? "a" : "e";
    }
    return ByHarmony(h, "o", "e", "\xC3\xB6");
}

// A suffix that takes a linking vowel after a consonant.
static void Linked(Builder* b, const char* consonants, Link link)
{
    Lengthen(b, false);
    if (b->sound.vowelEnd)
    {
        Start(b);
        Put(b, consonants);
    }
    else
    {
        const char* vowel = LinkingVowel(b, link);
        Start(b);
        Put(b, vowel);
        Put(b, consonants);
    }
    Ended(b);
}

// A suffix of fixed letters by harmony.
static void Plain(Builder* b, const char* back, const char* front, const char* round, bool keeps)
{
    Lengthen(b, keeps);
    const char* s = ByHarmony(b->sound.harmony, back, front, round);
    Start(b);
    Put(b, s);
    Ended(b);
}

// -val, -vel, -vá, -vé: v after a vowel, else the last consonant again.
static void Assimilated(Builder* b, const char* back, const char* front)
{
    Lengthen(b, false);
    const char* rest = b->sound.harmony == hungarianHarmonyBack ? back : front;
    if (b->sound.vowelEnd)
    {
        Start(b);
        Put(b, "v");
    }
    else
    {
        Start(b);
        Put(b, b->sound.sound);
    }
    Put(b, rest);
    Ended(b);
}

// The accusative: t after a vowel or one of the consonants it joins,
// else a linking vowel first.
static void Accusative(Builder* b)
{
    bool direct = b->sound.directT && (!b->sound.isLong || b->name);
    if (!b->sound.vowelEnd && direct && !b->linking && !b->lowering)
    {
        Lengthen(b, false);
        Start(b);
        Put(b, "t");
        Ended(b);
        return;
    }
    Linked(b, "t", linkLow);
}

// Whether the third person's possessive takes j (-ja, -je), by the end
// of the word, as the oracle counts it: after a vowel; never after a
// sibilant, a palatal, j, h or v; after two consonants; after one
// consonant n, d, b or p, and in a word of one syllable also m, t, k or
// l (napja but háza, asztala; kertje but könyve).
static bool TakesJ(const HungarianSound* s)
{
    static const char* const s_never[] = {"s",  "sz", "z",  "zs", "c", "cs", "dz", "dzs",
                                          "gy", "ny", "ty", "ly", "j", "h",  "v"};
    if (s->vowelEnd)
    {
        return true;
    }
    for (size_t i = 0; i < sizeof(s_never) / sizeof(s_never[0]); i++)
    {
        if (strcmp(s->sound, s_never[i]) == 0)
        {
            return false;
        }
    }
    if (s->cluster)
    {
        return true;
    }
    const char* often = s->vowels == 1 ? "ndbpmtkl" : "ndbp";
    return s->soundLength == 1 && strchr(often, s->sound[0]) != nullptr;
}

// The third person's possessive: -(j)a, -(j)e; -(j)uk, -(j)ük.
static void ThirdPossessive(Builder* b, bool owners)
{
    static const char* const s_forms[2][2][2] = {{{"a", "e"}, {"uk", "\xC3\xBCk"}},
                                                 {{"ja", "je"}, {"juk", "j\xC3\xBCk"}}};
    bool j = b->ownJ || (!b->noJ && TakesJ(&b->sound));
    bool front = b->sound.harmony != hungarianHarmonyBack;
    Lengthen(b, false);
    Start(b);
    Put(b, s_forms[j ? 1 : 0][owners ? 1 : 0][front ? 1 : 0]);
    Ended(b);
}

// The possessive of a singular thing: -m, -d, -(j)a, -nk, -tok, -(j)uk.
static void Possessive(Builder* b, const mlocInflection* r)
{
    bool owners = r->possessorPlural;
    if (r->possessorPerson == 3)
    {
        ThirdPossessive(b, owners);
    }
    else if (r->possessorPerson == 1)
    {
        Linked(b, owners ? "nk" : "m", owners ? linkHigh : linkLow);
    }
    else if (owners)
    {
        // After a lowering stem's e the suffix is unrounded too (könyvetek).
        bool unrounded = b->lowering && !b->sound.vowelEnd;
        Linked(b, ByHarmony(b->sound.harmony, "tok", "tek", unrounded ? "tek" : "t\xC3\xB6k"),
               linkLow);
    }
    else
    {
        Linked(b, "d", linkLow);
    }
}

// The possessive of plural things: -(j)ai or -i, then the person.
static void PluralPossessive(Builder* b, const mlocInflection* r)
{
    HungarianHarmony h = b->sound.harmony;
    bool vowel = b->sound.vowelEnd;
    bool j = !vowel && (b->ownJ || (!b->noJ && TakesJ(&b->sound)));
    Lengthen(b, false);
    const char* s = vowel ? "i"
                    : j   ? (h == hungarianHarmonyBack ? "jai" : "jei")
                          : (h == hungarianHarmonyBack ? "ai" : "ei");
    Start(b);
    Put(b, s);
    static const char* const s_persons[2][3] = {{"m", "d", ""}, {"nk", "", "k"}};
    const char* person = s_persons[r->possessorPlural ? 1 : 0][r->possessorPerson - 1];
    if (r->possessorPerson == 2 && r->possessorPlural)
    {
        // After the i the suffix is unrounded (könyveitek).
        person = h == hungarianHarmonyBack ? "tok" : "tek";
    }
    Put(b, person);
    Ended(b);
}

// How a case suffix joins: by harmony alone, so before -ként which
// lengthens nothing, the v assimilated, after a linking vowel, or as
// the accusative.
typedef enum Joining
{
    joinPlain,
    joinKeeps,
    joinAssimilated,
    joinLinked,
    joinAccusative,
} Joining;

typedef struct CaseSuffix
{
    mlocGrammaticalCase grammaticalCase;
    Joining joining;
    // Back, front and front rounded.
    const char* forms[3];
} CaseSuffix;

static const CaseSuffix s_caseSuffixes[] = {
    {mloc_caseAccusative, joinAccusative, {"t", "t", "t"}},
    {mloc_caseDative, joinPlain, {"nak", "nek", "nek"}},
    {mloc_caseInstrumental, joinAssimilated, {"al", "el", "el"}},
    {mloc_caseCausal, joinPlain, {"\xC3\xA9rt", "\xC3\xA9rt", "\xC3\xA9rt"}},
    {mloc_caseTranslative, joinAssimilated, {"\xC3\xA1", "\xC3\xA9", "\xC3\xA9"}},
    {mloc_caseTerminative, joinPlain, {"ig", "ig", "ig"}},
    {mloc_caseEssive, joinKeeps, {"k\xC3\xA9nt", "k\xC3\xA9nt", "k\xC3\xA9nt"}},
    {mloc_caseInessive, joinPlain, {"ban", "ben", "ben"}},
    {mloc_caseSuperessive, joinLinked, {"n", "n", "n"}},
    {mloc_caseAdessive, joinPlain, {"n\xC3\xA1l", "n\xC3\xA9l", "n\xC3\xA9l"}},
    {mloc_caseIllative, joinPlain, {"ba", "be", "be"}},
    {mloc_caseSublative, joinPlain, {"ra", "re", "re"}},
    {mloc_caseDelative, joinPlain, {"r\xC3\xB3l", "r\xC5\x91l", "r\xC5\x91l"}},
    {mloc_caseElative, joinPlain, {"b\xC3\xB3l", "b\xC5\x91l", "b\xC5\x91l"}},
    {mloc_caseAblative, joinPlain, {"t\xC3\xB3l", "t\xC5\x91l", "t\xC5\x91l"}},
    {mloc_caseAllative, joinPlain, {"hoz", "hez", "h\xC3\xB6z"}},
};

// A case's suffix, or null for one Hungarian has not (the nominative's
// is none).
static const CaseSuffix* CaseSuffixOf(mlocGrammaticalCase c)
{
    for (size_t i = 0; i < sizeof(s_caseSuffixes) / sizeof(s_caseSuffixes[0]); i++)
    {
        if (s_caseSuffixes[i].grammaticalCase == c)
        {
            return &s_caseSuffixes[i];
        }
    }
    return nullptr;
}

static void Case(Builder* b, const CaseSuffix* c)
{
    switch (c->joining)
    {
    case joinAccusative:
        Accusative(b);
        break;
    case joinAssimilated:
        Assimilated(b, c->forms[0], c->forms[1]);
        break;
    case joinLinked:
        Linked(b, c->forms[0], linkMid);
        break;
    default:
        Plain(b, c->forms[0], c->forms[1], c->forms[2], c->joining == joinKeeps);
        break;
    }
}

bool mlocHungarianRequestValid(const mlocInflection* request)
{
    bool hasCase = request->grammaticalCase == mloc_caseNominative ||
                   CaseSuffixOf(request->grammaticalCase) != nullptr;
    return hasCase && request->possessorPerson <= 3 &&
           (request->possessorPerson > 0 || !request->possessorPlural) && !request->copula &&
           !request->question;
}

// Whether a request's first suffix changes a table's stem: the plural,
// a possessive and the accusative, which take a linking vowel (vizet,
// bokrot, lovak); the superessive only where the table says (bokron,
// lovon, but vízen); the third person's possessive not where it takes
// -ja on the word as it is (útja).
static bool StemChanges(const mlocInflection* r, uint8_t flags)
{
    if (r->plural || (r->possessorPerson > 0 && r->possessorPerson < 3))
    {
        return true;
    }
    if (r->possessorPerson == 3)
    {
        return (flags & hungarianOwnJ) == 0;
    }
    return r->grammaticalCase == mloc_caseAccusative ||
           (r->grammaticalCase == mloc_caseSuperessive && (flags & hungarianSuperessive) != 0);
}

// Writes the request's suffixes after a sound.
static void Build(Builder* b, const mlocInflection* r)
{
    if (r->plural && r->possessorPerson > 0)
    {
        PluralPossessive(b, r);
    }
    else
    {
        if (r->plural)
        {
            Linked(b, "k", linkLow);
        }
        if (r->possessorPerson > 0)
        {
            Possessive(b, r);
        }
    }
    const CaseSuffix* c = CaseSuffixOf(r->grammaticalCase);
    if (c != nullptr)
    {
        Case(b, c);
    }
}

// Encodes the letters w[from, to) as UTF-8 into text; its length, or 0
// when they do not fit.
static size_t Encode(const HungarianLetters* w, size_t from, size_t to, char* text, size_t room)
{
    size_t n = 0;
    for (size_t i = from; i < to; i++)
    {
        char bytes[4];
        size_t used = 0;
        if (muniEncodeUtf8(w->c[i], bytes, &used) != muni_success || n + used > room)
        {
            return 0;
        }
        memcpy(text + n, bytes, used);
        n += used;
    }
    return n;
}

// A word's entry in the table: the whole word, or the longest last part
// of at least shortest letters (none for 0) that has one, after at least
// two letters (a compound's last word: színház, étterem; a number's:
// huszonhárom); fromOut receives where the part starts. The letters are
// encoded once, from the end, so each part is the key's tail; a part
// with a letter UTF-8 cannot hold is not looked up.
static const HungarianWord* Lookup(const HungarianLetters* w, size_t shortest, size_t* fromOut)
{
    char key[hungarianLetters * 4];
    size_t at[hungarianLetters];
    size_t first = w->count;
    size_t start = sizeof(key);
    while (first > 0)
    {
        char bytes[4];
        size_t used = 0;
        if (muniEncodeUtf8(w->c[first - 1], bytes, &used) != muni_success || used > start)
        {
            break;
        }
        start -= used;
        memcpy(key + start, bytes, used);
        first--;
        at[first] = start;
    }
    size_t last = shortest > 0 && w->count >= shortest ? w->count - shortest + 1 : 1;
    for (size_t from = first; from < last && from < w->count; from++)
    {
        if (from == 1)
        {
            continue;
        }
        const HungarianWord* word =
            mlocHungarianFindWord((const uint8_t*)key + at[from], sizeof(key) - at[from]);
        if (word != nullptr)
        {
            *fromOut = from;
            return word;
        }
    }
    return nullptr;
}

// The letters of a table word's changed stem: those before its part,
// then the part's stem.
static void ChangedLetters(const HungarianLetters* w, size_t from, const char* stem,
                           HungarianLetters* out)
{
    char text[hungarianLetters * 4 + hungarianTailRoom];
    size_t n = Encode(w, 0, from, text, hungarianLetters * 4);
    size_t s = 0;
    while (stem[s] != '\0' && s + 1 < hungarianTailRoom)
    {
        text[n + s] = stem[s];
        s++;
    }
    mlocHungarianReadLetters((const uint8_t*)text, n + s, out);
}

// What a word is before its suffixes: its sound, and how the table and
// the ordinals' -dik change it.
typedef struct Stem
{
    HungarianLetters letters;
    HungarianSound sound;
    const HungarianWord* word;
    // The changed stem of the letters from from on: the table's, or the
    // productive -alom and -elem's (nyugalmat, szerelmet).
    const char* changedStem;
    size_t from;
    // The changed stem is taken (the first suffix links).
    bool changed;
    // The table's flags, or the -alom rule's.
    uint8_t flags;
    HungarianLetters changedLetters;
} Stem;

static bool ReadStem(const HungarianText* t, const mlocInflection* r, Stem* out)
{
    out->from = 0;
    mlocHungarianReadLetters(t->spoken, t->spokenLength, &out->letters);
    mlocHungarianReadSound(&out->letters, &out->sound);
    if (!out->sound.hasVowel)
    {
        return false;
    }
    // A number's words, and a common noun's compound by its last word of
    // three letters or more after two or more; a name only as a whole
    // (Héraklész is not ész).
    out->word = Lookup(&out->letters, t->marked ? 1 : t->name ? 0 : 3, &out->from);
    if (out->word != nullptr && out->from > 0)
    {
        // A compound's harmony is its last word's (számítógépek).
        HungarianLetters head = out->letters;
        head.count -= out->from;
        memmove(head.c, head.c + out->from, head.count * sizeof(head.c[0]));
        out->sound.harmony = mlocHungarianReadHarmony(&head);
    }
    out->changedStem = out->word != nullptr ? out->word->stem : nullptr;
    uint8_t flags = out->word != nullptr ? out->word->flags : hungarianSuperessive;
    if (out->word == nullptr && out->sound.vowels >= 3 &&
        (mlocHungarianEndsWith(&out->letters, out->letters.count, "alom") ||
         mlocHungarianEndsWith(&out->letters, out->letters.count, "elem")))
    {
        out->from = out->letters.count - 4;
        out->changedStem = out->letters.c[out->from] == 'a' ? "alm" : "elm";
        flags |= hungarianLowering;
    }
    out->changed = out->changedStem != nullptr && StemChanges(r, flags);
    out->flags = out->word != nullptr || out->changedStem != nullptr ? flags : 0;
    if (out->changed)
    {
        HungarianHarmony harmony = out->sound.harmony;
        ChangedLetters(&out->letters, out->from, out->changedStem, &out->changedLetters);
        mlocHungarianReadSound(&out->changedLetters, &out->sound);
        out->sound.harmony = harmony;
    }
    if ((out->flags & hungarianBack) != 0)
    {
        out->sound.harmony = hungarianHarmonyBack;
    }
    return true;
}

// Whether a hinted name's written end is not said as written, which
// takes a hyphen (Rousseau-val, Molière-rel, Peugeot-t; AkH 217): a
// vowel letter said as another sound, or a consonant letter said as a
// vowel. A consonant said as a consonant joins (Balzackal, AkH 216).
static bool Unsaid(const HungarianText* t, const HungarianSound* spoken)
{
    HungarianLetters written;
    mlocHungarianReadLetters(t->word, t->wordLength, &written);
    if (written.count == 0)
    {
        return true;
    }
    uint32_t last = written.c[written.count - 1];
    bool vowel = (mlocHungarianLetterVowel(&written, written.count - 1) & hungarianVowelAny) != 0;
    if (!vowel)
    {
        return spoken->vowelEnd;
    }
    HungarianLetters said;
    mlocHungarianReadLetters(t->spoken, t->spokenLength, &said);
    uint32_t end = said.count > 0 ? said.c[said.count - 1] : 0;
    bool same = end == last || (end != 0 && end == mlocHungarianLongVowel(last));
    return !spoken->vowelEnd || !same;
}

// Replaces a table word's letters from the first that its stem changes
// (víz: v kept, iz written), a capital written as a capital (Út, Utat).
static void ChangedEnd(const HungarianText* t, const Stem* stem, const HungarianLetters* written,
                       HungarianForm* out)
{
    HungarianLetters changed;
    size_t length = strlen(stem->changedStem);
    mlocHungarianReadLetters((const uint8_t*)stem->changedStem, length, &changed);
    size_t part = stem->letters.count - stem->from;
    size_t same = 0;
    while (same < part && same < changed.count &&
           stem->letters.c[stem->from + same] == changed.c[same])
    {
        same++;
    }
    size_t letter = written->count - part + same;
    out->kept = letter < written->count ? written->at[letter] : t->wordLength;
    size_t from = same < changed.count ? changed.at[same] : length;
    size_t n = length - from < hungarianTailRoom ? length - from : hungarianTailRoom;
    memcpy(out->tail, stem->changedStem + from, n);
    out->tailLength = (uint8_t)n;
    if (letter < written->count && same < changed.count)
    {
        size_t next = letter + 1 < written->count ? written->at[letter + 1] : t->wordLength;
        size_t start = 0;
        uint32_t original = mlocHungarianPrevious(t->word, next, &start);
        char capital[4];
        size_t used = 0;
        size_t rest = (same + 1 < changed.count ? changed.at[same + 1] : length) - from;
        if (mlocHungarianSmall(original) != original &&
            muniEncodeUtf8(mlocHungarianCapital(changed.c[same]), capital, &used) == muni_success &&
            used + n - rest <= hungarianTailRoom)
        {
            memmove(out->tail + used, out->tail + rest, n - rest);
            memcpy(out->tail, capital, used);
            out->tailLength = (uint8_t)(used + n - rest);
        }
    }
}

// Sets a joined word's changed end: the table's stem, or its last vowel
// lengthened.
static void JoinedStem(const HungarianText* t, const Stem* stem, const Builder* b,
                       HungarianForm* out)
{
    HungarianLetters written;
    mlocHungarianReadLetters(t->word, t->wordLength, &written);
    if (stem->changed && written.count >= stem->letters.count - stem->from)
    {
        ChangedEnd(t, stem, &written, out);
        return;
    }
    if (b->lengthen && t->wordLength > 0)
    {
        // The last letter as written, its case kept.
        size_t start = 0;
        uint32_t longer =
            mlocHungarianLongVowel(mlocHungarianPrevious(t->word, t->wordLength, &start));
        size_t used = 0;
        if (longer != 0 && muniEncodeUtf8(longer, out->tail, &used) == muni_success)
        {
            out->kept = start;
            out->tailLength = (uint8_t)used;
        }
    }
}

// A written long consonant meets a suffix that begins with it: a name
// keeps it with a hyphen (Papp-pal, Mann-nak; AkH 160, 217), a common
// noun writes the three letters as two (tollal, orra; AkH 93); a short
// modern digraph doubles in its first letter (Kodállyal; AkH 92).
static void Meeting(const HungarianText* t, const HungarianSound* s, HungarianForm* out)
{
    size_t n = s->soundLength;
    bool begins = n > 0 && out->length >= n && memcmp(out->suffixes, s->sound, n) == 0;
    if (!begins || !s->modern || out->kept != t->wordLength || out->tailLength != 0)
    {
        return;
    }
    if (s->isLong)
    {
        if (t->name)
        {
            out->hyphen = true;
        }
        else
        {
            memmove(out->suffixes, out->suffixes + n, out->length - n);
            out->length = (uint8_t)(out->length - n);
        }
        return;
    }
    if (n >= 2 && s->writtenLength == n)
    {
        // The digraph doubles in its first letter: Kodá + lly + al.
        out->kept = t->wordLength - n;
        out->tail[0] = s->sound[0];
        memcpy(out->tail + 1, s->sound, n);
        out->tailLength = (uint8_t)(n + 1);
        memmove(out->suffixes, out->suffixes + n, out->length - n);
        out->length = (uint8_t)(out->length - n);
    }
}

bool mlocHungarianInflect(const HungarianText* t, const mlocInflection* r, HungarianForm* out)
{
    *out = (HungarianForm){.kept = t->wordLength};
    Stem stem;
    if (!ReadStem(t, r, &stem))
    {
        return false;
    }
    Builder b = {.sound = stem.sound, .name = t->name};
    b.lowering = (stem.flags & hungarianLowering) != 0;
    // Ordinals in -dik and participles in -ott, -ött, -ult, -ült, -ölt
    // lower (harmadikat, halottak, felnőttek, múltak, sérültek).
    static const char* const s_lowering[] = {"dik",        "ott",        "\xC3\xB6tt", "ult",
                                             "\xC3\xBAlt", "\xC3\xBClt", "\xC3\xB6lt"};
    for (size_t i = 0; i < sizeof(s_lowering) / sizeof(s_lowering[0]); i++)
    {
        b.lowering =
            b.lowering || mlocHungarianEndsWith(&stem.letters, stem.letters.count, s_lowering[i]);
    }
    b.linking = stem.changed || (stem.flags & hungarianLinking) != 0;
    b.ownJ = (stem.flags & hungarianOwnJ) != 0;
    b.noJ = (stem.flags & hungarianNoJ) != 0;
    if (stem.changed)
    {
        // A changed stem takes the third person without j (vize, lova).
        b.sound.cluster = false;
    }
    Build(&b, r);
    memcpy(out->suffixes, b.text, b.length);
    out->length = b.length;
    if (b.length == 0)
    {
        return true;
    }
    out->hyphen = t->marked || (t->hinted && Unsaid(t, &stem.sound));
    if (out->hyphen)
    {
        return true;
    }
    JoinedStem(t, &stem, &b, out);
    if (!t->hinted)
    {
        Meeting(t, &stem.sound, out);
    }
    return true;
}
