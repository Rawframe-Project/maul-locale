// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Grammatical suffixes on runtime text and numbers (record mloc-0046):
// a word, a name, an abbreviation or a number inflected for case,
// number, possessor and copula by the rules of its language, read from
// how its end is spoken. Turkish first; the rules are code, the spoken
// form of a number comes from the spell-out rules (spellout.mloc).

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
        // The copula ("is": Turkish -DIr), after the case.
        bool copula;
        // The question particle (Turkish mI), a word of its own after the
        // suffixes.
        bool question;
        // A common noun: Turkish writes no apostrophe before its suffixes
        // and softens its final stop (kitap, kitabı). A name, a number or
        // an abbreviation otherwise.
        bool commonNoun;
        // How the text's end is read.
        mlocInflectionReading reading;
        // The text as it is spoken, read instead of the text where they
        // differ ("jak" for Jacques, "nato" for NATO); null for the text.
        const char* hint;
        size_t hintLength;
    } mlocInflection;

    // The parts of an inflected text: the text as given, the apostrophe,
    // the suffixes, and the space and particle of a question. Their
    // values differ from a number's parts (mlocNumberPart), which a
    // number's stem holds inside it.
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
    ///         has no rules for (Turkish now) or data without the number
    ///         units; mloc_errorInvalid for a null pointer.
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
    ///         spell-out rules; mloc_errorInvalid for a null pointer, an
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
