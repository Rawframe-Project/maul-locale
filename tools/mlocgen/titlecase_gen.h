// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A titlecase table (src/spellout_tables.h's layout): the lowercase
// letters a data file's text holds, each with its full titlecase, so the
// library can titlecase a first word without the Unicode tables. The
// spell-out file notes every letter of its rules, the names file the
// first letter of each name.

#ifndef MLOCGEN_TITLECASE_GEN_H
#define MLOCGEN_TITLECASE_GEN_H

#include "datafile.h"

#include <stddef.h>
#include <stdint.h>

// The letters noted, a bit per code point.
typedef struct Titlecase
{
    uint8_t lowercase[0x110000 / 8];
} Titlecase;

// Notes the lowercase letters of well-formed text: every one, or the
// first code point's alone.
void TitlecaseNote(Titlecase* t, const char* text, size_t length, bool firstOnly);

// Adds the table of the noted letters whose full titlecase differs, as a
// unit of a kind and of no locale.
void TitlecaseAdd(const Titlecase* t, uint32_t kind, DataFile* file);

#endif
