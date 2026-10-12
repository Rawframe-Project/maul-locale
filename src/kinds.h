// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The kinds of data units (record mloc-0014): the part in the high 16
// bits, the table in the low 16. Shared by the library and the
// generator; a kind's number never changes meaning within a data
// version.

#ifndef MLOC_KINDS_H
#define MLOC_KINDS_H

#include <stdint.h>

// The locale part: likely subtags, a unit of no locale.
constexpr uint32_t kindLocaleLikely = 0x00010001;
// The locale part: the aliases canonicalization replaces, a unit of no
// locale.
constexpr uint32_t kindLocaleAliases = 0x00010002;

#endif
