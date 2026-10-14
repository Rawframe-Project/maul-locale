// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The names part's units (record mloc-0036, names_tables.h): their
// validators, and a code's name found along a locale's chain. A names
// unit's codes index the codes unit of its own source, so each unit of
// the walk is read with its source's.

#ifndef MLOC_NAMES_DATA_H
#define MLOC_NAMES_DATA_H

#include "data_view.h"
#include "names_tables.h"

#include "maul-locale/locale.h"

#include <stddef.h>
#include <stdint.h>

bool mlocValidNameIds(const DataView* view, DataSpan payload);
bool mlocValidNames(const DataView* view, DataSpan payload);

// The index of a code in a valid codes unit; false when the unit lacks
// it.
bool mlocFindNameId(DataSpan ids, const char* code, size_t length, uint16_t* indexOut);

// A name's UTF-8 text, in the data.
typedef struct NameText
{
    const uint8_t* text;
    uint16_t length;
} NameText;

// Finds a code's field along a locale's chain, else its name along the
// chain (a NameField; nameFieldName for the name alone). False when
// neither is found or the chain cannot be walked.
bool mlocFindName(const mlocDataSet* data, const mlocLocale* locale, const char* code,
                  size_t length, NameField field, NameText* out);

#endif
