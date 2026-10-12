// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A -u keyword's value in a canonical locale, for the parts that read
// one (regional preferences, numbering systems, calendars).

#ifndef MLOC_LOCALE_KEYWORD_H
#define MLOC_LOCALE_KEYWORD_H

#include "maul-locale/locale.h"

#include <stddef.h>

// The value of the -u key (two letters), and its length; an empty value
// of length 0 when the locale has none. The value is not NUL-ended.
size_t mlocFindKeyword(const mlocLocale* locale, const char* key, const char** valueOut);

#endif
