// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The maximized language, script and region language matching compares,
// made as ICU's LikelySubtags.makeMaximizedLsr makes them, and the likely
// script and region of a language for its ties.

#ifndef MLOC_LOCALE_LSR_H
#define MLOC_LOCALE_LSR_H

#include "locale_likely.h"

#include "maul-locale/locale.h"

#include <stdint.h>

// The subtags a locale gave, as bits; they make the micro distance.
enum
{
    lsrExplicitRegion = 1,
    lsrExplicitScript = 2,
    lsrExplicitLanguage = 4,
    lsrExplicit = 7,
};

// What a locale is to matching: an ordinary one, a pseudo locale
// (accents, bidi or cracked, which ICU marks with a prefix on the
// language and script) or a private use tag, which is its whole tail.
enum
{
    lsrNormal = 0,
    lsrAccents = 1,
    lsrBidi = 2,
    lsrCracked = 3,
    lsrPrivate = 4,
};

typedef struct Lsr
{
    // The language's 8 bytes as the locale holds them.
    uint64_t language;
    // Packed as locale_tables.h packs them; 0 for none.
    uint32_t script;
    uint32_t region;
    // The language packed when it has 2 or 3 letters and the kind is
    // normal, else 0.
    uint16_t packedLanguage;
    uint8_t flags;
    uint8_t kind;
    // The index of the locale it was made from.
    uint32_t index;
} Lsr;

// Makes a locale's LSR: an exact "und" stays itself, a private use tag
// and the pseudo locales (region XA, XB or XC, or a single variant
// psaccent, psbidi or pscrack) are kept as they are, any other locale
// gets likely subtags with ICU's lookup (a macro region, by the match
// table's flags, takes the likely region). False when the table has no
// answer, which only a table without "und" lacks.
bool mlocMakeLsr(const Likely* likely, const uint8_t* regionGroups, const mlocLocale* locale,
                 uint32_t index, Lsr* lsrOut);

// The likely script and region of an LSR's language alone, as ICU's
// compareLikely reads them: "und"'s for a language the table lacks.
bool mlocLikelyOfLanguage(const Likely* likely, const Lsr* lsr, uint32_t* scriptOut,
                          uint32_t* regionOut);

#endif
