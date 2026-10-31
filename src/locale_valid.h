// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Whether a locale the caller gives is one in canonical syntax, for every
// entry that reads one.

#ifndef MLOC_LOCALE_VALID_H
#define MLOC_LOCALE_VALID_H

#include "maul-locale/locale.h"

#include <stdbool.h>

// Whether the locale is not null and each field is NUL-padded text of its
// syntax (mlocLocale): a language of 2, 3 or 5 to 8 lowercase letters, a
// title case script of 4 letters or none, a region of 2 uppercase letters
// or 3 digits or none, and a tail of lowercase subtags of 1 to 8 letters
// and digits joined by single hyphens. Syntax only: an alias or an
// unsorted tail passes.
bool mlocLocaleValid(const mlocLocale* locale);

#endif
