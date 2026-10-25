// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A -u keyword's value in a canonical locale, for the parts that read
// one (regional preferences, numbering systems, calendars).

#ifndef MLOC_LOCALE_KEYWORD_H
#define MLOC_LOCALE_KEYWORD_H

#include "maul-locale/locale.h"

#include <stdbool.h>
#include <stddef.h>

// The value of the -u key (two letters), and its length: its type
// subtags joined by hyphens ("islamic-civil"); an empty value of length 0
// when the locale has none. The value is not NUL-ended.
size_t mlocFindKeyword(const mlocLocale* locale, const char* key, const char** valueOut);

// Whether the locale has the -u key, with a value or without one (a key
// alone stands for "true", which canonical form leaves out).
bool mlocHasKeyword(const mlocLocale* locale, const char* key);

// Sets the -u key (two letters) to a value (its type subtags, lowercase,
// joined by hyphens), replacing the key's value if it has one: false
// when the locale with it is no locale id or does not fit.
bool mlocSetKeyword(mlocLocale* locale, const char* key, const char* value, size_t length);

// Leaves the -u key and its value out, and the u extension when nothing
// is left in it: false when the locale without it does not parse.
bool mlocRemoveKeyword(mlocLocale* locale, const char* key);

#endif
