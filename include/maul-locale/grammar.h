// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Grammatical suffixes on runtime text and numbers (records mloc-0046 to
// mloc-0048): a word, a name, an abbreviation or a number inflected for
// case, number, possessor and copula by the rules of its language, read
// from how its end is spoken. Turkish, Kazakh, Kyrgyz, Azerbaijani,
// Uzbek, Turkmen, Hungarian, Finnish and Korean; the rules are code
// (Finnish's inflection types from Kotus's word list, mloc-0049; Hanja's
// readings from Unihan, mloc-0050), the spoken form of a
// number comes from the spell-out rules (spellout.mloc) or, for Uzbek and
// Turkmen, which have none, from their number words in code.

#ifndef MAUL_LOCALE_GRAMMAR_H
#define MAUL_LOCALE_GRAMMAR_H

#include "maul-locale/locale.h"
#include "maul-locale/number.h"
#include "maul-locale/unit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // A locale's inflector: a value that points into the data it was made
    // from, which must outlive it. Opaque; make it with mlocInitInflector.
    typedef struct mlocInflector
    {
        uint64_t opaque[100];
    } mlocInflector;

    // How an inflected text's end is read.
    typedef enum mlocInflectionReading
    {
        // By its look: a number at its end by how the number is spoken,
        // a word of capitals by its letters' names, else as written.
        mloc_readingAuto = 0,
        // As a word, written as it is spoken (NATO, a name).
        mloc_readingWord = 1,
        // By its letters' names (TDK, ABD).
        mloc_readingLetters = 2,
    } mlocInflectionReading;

    // A Korean particle, each written in its form after a final consonant
    // or after a vowel (the (으)로 family's after a vowel also after ㄹ).
    typedef enum mlocKoreanParticle
    {
        mloc_particleNone = 0,
        // 은/는.
        mloc_particleTopic = 1,
        // 이/가.
        mloc_particleSubject = 2,
        // 을/를.
        mloc_particleObject = 3,
        // 과/와.
        mloc_particleComitative = 4,
        // 이랑/랑, the spoken comitative.
        mloc_particleComitativeSpoken = 5,
        // 아/야.
        mloc_particleVocative = 6,
        // 이에요/예요.
        mloc_particleCopulaPolite = 7,
        // 이다/다.
        mloc_particleCopula = 8,
        // 으로/로.
        mloc_particleDirection = 9,
        // 으로서/로서.
        mloc_particleCapacity = 10,
        // 으로써/로써.
        mloc_particleMeans = 11,
        // 으로부터/로부터.
        mloc_particleSource = 12,
        // 이나/나.
        mloc_particleChoice = 13,
        // 이든/든.
        mloc_particleAny = 14,
        // 이라/라.
        mloc_particleQuotation = 15,
        // 이란/란.
        mloc_particleDefinition = 16,
    } mlocKoreanParticle;

    // What a text is inflected for; zero is the text as it is.
    typedef struct mlocInflection
    {
        // The case; nominative adds none. A case the language does not
        // have is refused.
        mlocGrammaticalCase grammaticalCase;
        // The plural.
        bool plural;
        // The possessor's person, 1 to 3, or 0 for none; and whether the
        // possessor is plural.
        uint8_t possessorPerson;
        bool possessorPlural;
        // The copula ("is": Turkish -DIr), after the case; refused in a
        // language without the suffix (Kazakh, Kyrgyz).
        bool copula;
        // The question particle (Turkish mI), a word of its own after the
        // suffixes.
        bool question;
        // A common noun: it takes no boundary mark before its suffixes
        // and its final stop softens (kitap, kitabı). A name, a number or
        // an abbreviation otherwise.
        bool commonNoun;
        // Korean's particle; refused in another language, and Korean
        // refuses the fields above.
        mlocKoreanParticle particle;
        // Refuse (mloc_errorNotFound) a text whose end cannot be read,
        // where Korean would write both forms ("은(는)").
        bool exact;
        // How the text's end is read.
        mlocInflectionReading reading;
        // The text as it is spoken, read instead of the text where they
        // differ ("jak" for Jacques, "nato" for NATO); null for the text.
        const char* hint;
        size_t hintLength;
    } mlocInflection;

    // The parts of an inflected text: the text as given, the boundary
    // mark (Turkish's apostrophe, Kazakh's, Azerbaijani's, Turkmen's and
    // Hungarian's hyphen, Finnish's colon or apostrophe, none in Kyrgyz
    // and Uzbek; the part keeps the
    // name apostrophe), the suffixes, and the space
    // and particle of a question. Their values differ from a number's parts (mlocNumberPart), which
    // a number's stem holds inside it.
    typedef enum mlocInflectionPart
    {
        mloc_inflectionPartStem = 0x60,
        mloc_inflectionPartApostrophe = 0x61,
        mloc_inflectionPartSuffix = 0x62,
        mloc_inflectionPartParticle = 0x63,
    } mlocInflectionPart;

    /// Makes a locale's inflector. Its numbers are read by the locale's
    /// spell-out rules when the data has them (spellout.mloc); without
    /// them the inflector is made, and a number it meets is refused.
    ///
    /// @param inflectorOut  Receives the inflector; left unchanged on
    ///                      failure.
    /// @param data          The data with the number units and, for
    ///                      numbers, the spell-out rules.
    /// @param locale        The locale, canonicalized.
    /// @return mloc_success; mloc_errorNotFound for a language the library
    ///         has no rules for (Turkish, Kazakh, Kyrgyz, Azerbaijani,
    ///         Uzbek, Turkmen, Hungarian, Finnish and Korean now, Kazakh and Kyrgyz in
    ///         their Cyrillic script, the others in their Latin one) or
    ///         data without the number units;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitInflector(mlocInflector* inflectorOut,
                                                         const mlocDataSet* data,
                                                         const mlocLocale* locale);

    /// Writes a text inflected as asked: in Turkish its suffixes in
    /// order (plural, possessive, case, copula) by vowel harmony, the
    /// assimilation of d to t after a voiceless consonant and the buffer
    /// consonants, after an apostrophe on a name, a number or an
    /// abbreviation (none after a final period or closing quotation
    /// mark; none on a name's plural, which TDK joins, "Ahmetlere",
    /// unless the name is foreign, which a hint says, "Nice'ler"), then the
    /// question particle as a word of its own. The last word decides:
    /// its digits by how the number is spoken ("1985'te", bin dokuz yüz
    /// seksen beş; "2,5'e"; "5.'ye", beşinci), capitals by their
    /// letters' names ("TDK'ye"), otherwise its letters as written, with
    /// the loanwords that break harmony in their spoken form ("saate").
    /// In Kazakh the suffixes' first consonants follow the last sound (a
    /// vowel, a glide, l, a nasal, a voiceless consonant: "Алматыға",
    /// "Адамның", "кітапқа"), a final stop softens before a vowel in any
    /// word ("кітабы", "Айтбегі"), and a hyphen goes after an
    /// abbreviation read by its letters, digits and a word in another
    /// script ("ТМД-ның", "5-ке", "Google-ге"), a name joined. Kyrgyz
    /// rounds A after о and ө but not у ("тоолор", "кулдар"), joins
    /// every suffix and the question particle ("АКШнын", "350дөн",
    /// "китеппи") and has no instrumental case. Azerbaijani writes no d
    /// to t ("kitabda"), a hyphen after letters, digits and a name a hint
    /// says is foreign ("BMT-yə", "3-də", "Microsoft-un"), the question
    /// particle joined, and has no instrumental case. Uzbek has no
    /// harmony, writes -ka after k and -qa after q ("tilakka",
    /// "qishloqqa", "50ka"), joins every suffix ("ToshDUning", "5ga")
    /// and has no instrumental case. Turkmen rounds a suffix's first
    /// vowel after a rounded one in the possessives and, after ö and ü,
    /// the genitive ("ýolum", "gözüň", "gözi"), fuses the dative with a
    /// final vowel ("enä"), voices a final p, ç or k after a vowel in a
    /// word of two syllables or more ("kitaby", "baýlygy"), writes a
    /// hyphen after letters and digits ("BMG-niň", "1700-den"), joins a
    /// name ("Aşgabada") and has no instrumental case. Hungarian builds
    /// its 16 case suffixes, plural and possessives on the word's harmony
    /// and end as the Academy's rules write them: a linking vowel, a final
    /// a or e lengthened ("almát"), the v of -val and -vá assimilated
    /// ("Kodállyal", "Kossuthtal"), a stem the table changes ("vizet",
    /// "bokrot", "lovak"), a common noun's compound by its last word
    /// ("számítógépek"); a hyphen after digits, capitals, a period, a
    /// name's doubled final letter met by the same letter ("5-tel",
    /// "3.-at", "ENSZ-szel", "Papp-pal") and an end the hint says is not
    /// said ("Rousseau-val"); no genitive, copula or question suffix.
    /// Finnish builds its 14 cases in both numbers, possessives and the
    /// question clitic on a word's inflection type and consonant
    /// gradation (Kotus's types 1 to 49: the word list's table, else the
    /// ending; a compound by its last word in the table, a name by its
    /// ending): "kadun", "käteen", "renkaan", "kalleuden", "vaa'an",
    /// "Helsingissä", "Virtasen"; a colon after digits, capitals and an
    /// ordinal's period, the ending as said ("5:een", "EU:hun",
    /// "5:nnen"), refused in the plural and with a possessor; an
    /// apostrophe after an end the hint says is not said
    /// ("Bordeaux'ssa"); no copula.
    /// Korean appends the request's particle (mlocKoreanParticle) in its
    /// form after the final of the text's spoken end: a Hangul
    /// syllable's ("책은", "사과는"; ㄹ a vowel to the (으)로 family:
    /// "지하철로"), a number's Sino-Korean reading ("1은", "2는"), a
    /// capital's Korean name ("URL로", "KBS는"), a Hanja's reading
    /// ("學校는"), a hint's; a parenthesized gloss leaves the word before
    /// it to read ("서울(Seoul)은"), and 나, 너, 저 and 누구 take their
    /// own stems before 가 ("내가"). An end it cannot read (a Latin word
    /// without a hint, kana, a Hanja of readings that differ) takes both
    /// forms ("Apple은(는)", "(으)로"), or is refused when the request is
    /// exact. Korean refuses a case, the plural, a possessor, the copula
    /// and the question; other languages refuse a particle.
    /// What the text cannot say (a foreign name, an abbreviation read as
    /// a word) the request's hint or reading says.
    ///
    /// @param inflector  An inflector from mlocInitInflector.
    /// @param text       The text, UTF-8.
    /// @param length     Its length in bytes.
    /// @param request    What to inflect it for.
    /// @param output     Receives the text with its suffixes and, when
    ///                   output->part is set, its parts
    ///                   (mlocInflectionPart).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output->length is then the length needed);
    ///         mloc_errorNotFound for a number when the inflector has no
    ///         spell-out rules or no word for its size (Uzbek past
    ///         10^35, Turkmen past 10^20); mloc_errorInvalid for a null pointer, an
    ///         empty text or one that is not UTF-8, a request the
    ///         language does not have (a case, a person past 3), a text
    ///         with nothing to read (no letter or digit at its end), or a
    ///         number of more digits than a decimal holds (40).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInflectText(const mlocInflector* inflector,
                                                       const char* text, size_t length,
                                                       const mlocInflection* request,
                                                       mlocOutput* output);

    /// Writes a number as a formatter writes it, inflected as asked by
    /// how the number is spoken ("5'e", "1.985'te", "%50'ye").
    ///
    /// @param inflector  An inflector from mlocInitInflector.
    /// @param formatter  The number's formatter, or null for the locale's
    ///                   default one.
    /// @param decimal    The number.
    /// @param request    What to inflect it for (its reading and hint are
    ///                   not read).
    /// @param output     Receives the text and its parts, the number's own
    ///                   parts (mlocNumberPart) inside the stem.
    /// @return As mlocInflectText; mloc_errorInvalid also for a decimal
    ///         the library did not make or a formatter it did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInflectNumber(const mlocInflector* inflector,
                                                         const mlocNumberFormatter* formatter,
                                                         const mlocDecimal* decimal,
                                                         const mlocInflection* request,
                                                         mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
