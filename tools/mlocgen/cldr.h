// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's inheritance, resolved as CLDR's own tools resolve it
// (XMLSource.ResolvingSource.getPathLocation, which writes CLDR's JSON):
// a chain is looked up along the locale's parent chain (CLDR's parent
// locales per component, else truncation, then root), skipping the
// inheritance marker "↑↑↑" and values below the draft threshold. When no
// locale has it, root's alias for the chain, else the chain without its
// alt, else with count="other", is looked up the same way, again from the
// locale asked for. UTS #35 also names case and gender as lateral
// fallbacks; CLDR's resolution does not apply them, and the parts that
// read inflected forms apply them themselves.

#ifndef MLOCGEN_CLDR_H
#define MLOCGEN_CLDR_H

#include "dtd.h"
#include "ldml.h"
#include "map.h"

typedef enum Component
{
    // common/main and the parents for everything but collation.
    componentMain,
    // common/collation, with its own parent locales.
    componentCollation,
} Component;

typedef struct Cldr
{
    char* commonDirectory;
    Dtd dtd;
    // Locale id to its LdmlFile, per component, loaded when first asked
    // for; a missing file maps to a marker.
    Map files[2];
    // Explicit parent locales: the main ones and collation's own, which
    // stand alone.
    Map parents;
    Map collationParents;
    // A language alone to its likely script, from likelySubtags.xml.
    Map scripts;
    // The least acceptable draft status (contributed, as ICU takes).
    Draft threshold;
} Cldr;

// Opens CLDR's common directory: reads its DTD and parent locales.
// Returns 0 and a message on failure.
int CldrOpen(Cldr* cldr, const char* commonDirectory, char* errorOut, size_t errorCapacity);

// The parent of a locale id ("de_AT" to "de", "en_AU" to "en_001", "de"
// to "root"), or null for root: the component's own parent locales, else
// for the main component root when the id ends in a script that is not
// its language's likely one (the nonlikelyScript rule, applied to every
// id as CLDR's LocaleIDParser does, not only to the ones listed), else
// truncation.
const char* CldrParent(Cldr* cldr, const char* locale, Component component);

// Resolves a chain for a locale. Returns the leaf, or null when no
// locale up to root has it. A leaf whose text is "∅∅∅" means the value is
// explicitly empty. Fails the program on a file CLDR cannot read or on
// aliases that loop.
const Leaf* CldrResolve(Cldr* cldr, const char* locale, const char* chain, Component component);

void CldrClose(Cldr* cldr);

#endif
