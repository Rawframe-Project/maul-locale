// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The scripts written right to left (record mloc-0045), from CLDR's
// properties/scriptMetadata.txt, which src/generated/script_directions.c
// defines.

#ifndef MLOC_SCRIPT_DIRECTIONS_H
#define MLOC_SCRIPT_DIRECTIONS_H

#include <stdint.h>

// Script codes ("Arab"), sorted by their bytes.
extern const char mlocRightToLeftScripts[][4];
extern const uint32_t mlocRightToLeftScriptCount;

#endif
