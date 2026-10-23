// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale keys: a CLDR id with the script likely subtags would give it,
// left out when it is the language's own likely script, and the region as
// given; "root" for root. The library makes the same keys at run time.

#ifndef MLOCGEN_KEYS_H
#define MLOCGEN_KEYS_H

#include "map.h"

#include <stddef.h>

typedef struct Keys
{
    // likelySubtags.xml: from to to, CLDR ids.
    Map likely;
} Keys;

// Reads supplemental/likelySubtags.xml. Returns 0 and a message on
// failure.
int KeysOpen(Keys* keys, const char* commonDirectory, char* errorOut, size_t errorCapacity);

// Writes the key of a CLDR id ("sr_Cyrl_BA" gives "sr_BA", "zh_TW"
// gives "zh_Hant_TW"). Fails the program on an id it cannot read.
void KeyOf(const Keys* keys, const char* id, char* out, size_t capacity);

void KeysClose(Keys* keys);

#endif
