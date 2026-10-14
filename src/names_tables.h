// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The names part's units (record mloc-0036), shared by the library and
// the generator. They ship as a data file of their own, names.mloc, and
// the subdivision names as another, subdivisions.mloc, with a codes unit
// and names units of its own (record mloc-0039). All integers are
// little-endian.
//
// The codes, a unit of no locale: a header (u16 count, u16 0, u32 text
// size), the codes' text offsets (u32 each, count + 1, from 0 to the
// text size, ascending), then the text: codes strictly ascending by
// their bytes, each of ASCII letters, digits, '-', ':' and '%', at most
// nameMaxId bytes. A code is a namespace letter, ':' and what it names,
// in BCP 47's case:
//
//   l:  a language id CLDR names: "l:en", "l:en-GB", "l:be-tarask"
//   s:  a script ("s:Latn"); r: a region ("r:US", "r:419"); v: a variant
//       ("v:valencia")
//   k:  a key by its BCP 47 id ("k:ca"; "k:t" and "k:x" the extensions)
//   t:  a key and a value by their BCP 47 ids ("t:ca-gregory")
//   p:  a display pattern: "p:localePattern", "p:localeSeparator",
//       "p:localeKeyTypePattern"
//   c:  a code pattern: "c:language", "c:script", "c:region"
//   m:  a measurement system by its BCP 47 value: "m:metric",
//       "m:uksystem", "m:ussystem"
//   b:  a key's value of yes or no: "b:yes", "b:no" (CLDR's typeValues)
//   d:  a subdivision by its code, in subdivisions.mloc: "d:gbsct"
//
// A name whose alt is not one of the fields below ("biot", "chagos" and
// Finnish's "secondary") has a code of its own, the alt after '%':
// "r:IO%chagos". At most nameMaxCodes codes; a code's index names it in
// the names units.
//
// The names, units per locale key, one kind for each of languages,
// scripts, regions, variants, keys with their values, and the patterns
// with the rest (p:, c:, m: and b:), and the subdivisions (d:, the name
// field alone): a header (u16 entry count, u16 text
// size), the entries' keys (u16 each, a code's index times 8 plus a
// NameField, strictly ascending), their text offsets (u16 each, count +
// 1, from 0 to the text size, strictly ascending: no text is empty), then
// the text, UTF-8 with no byte below 0x20. An entry is CLDR's value of a
// code's field (resolved along the locale's chain, as CLDR resolves it),
// filed at a key where the library's lookup would give another: the
// field along the chain, else the name along the chain.
//
// The titlecase of the lowercase letters that begin a name, a unit of no
// locale in spellout_tables.h's titlecase layout, for the capitalization
// contexts.

#ifndef MLOC_NAMES_TABLES_H
#define MLOC_NAMES_TABLES_H

#include "kinds.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    nameIdsHeaderSize = 8,
    nameMaxId = 48,
    // The most codes, whose keys fit 16 bits.
    nameMaxCodes = 8192,
    nameUnitHeaderSize = 4,
};

// The fields of a name: CLDR's name and its alternates.
typedef enum NameField
{
    nameFieldName = 0,
    // alt="short", alt="long", alt="variant", alt="menu".
    nameFieldShort = 1,
    nameFieldLong = 2,
    nameFieldVariant = 3,
    nameFieldMenu = 4,
    // A language's menu="core" or a value's scope="core": the name a
    // menu or a key's own name goes with ("Kurdish", "Gregorian").
    nameFieldCore = 5,
    // A language's menu="extension", what follows the core in a menu.
    nameFieldExtension = 6,
    // A script's alt="stand-alone", its name outside a locale's name.
    nameFieldStandAlone = 7,
    nameFieldCount = 8,
} NameField;

// The kind of the names units that hold a code, by its namespace; 0 for
// a code of no namespace.
static inline uint32_t mlocNameKind(const char* code, size_t length)
{
    if (length < 2 || code[1] != ':')
    {
        return 0;
    }
    switch (code[0])
    {
    case 'l':
        return kindNamesLanguages;
    case 's':
        return kindNamesScripts;
    case 'r':
        return kindNamesRegions;
    case 'v':
        return kindNamesVariants;
    case 'k':
    case 't':
        return kindNamesKeys;
    case 'p':
    case 'c':
    case 'm':
    case 'b':
        return kindNamePatterns;
    case 'd':
        return kindNamesSubdivisions;
    default:
        return 0;
    }
}

#endif
