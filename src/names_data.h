// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The names part's units (names_tables.h): their validators, and a code's
// name found along a locale's chain. A names unit's codes index the codes
// unit of its own source, so each unit of the walk is read with its
// source's.

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

enum
{
    // The steps a names chain keeps; a longer one is walked per lookup.
    nameChainMost = 24,
};

// A locale's chain resolved once for many lookups: each step that a
// source with a codes unit holds, in the walk's order.
typedef struct NameStep
{
    DataView view;
    DataSpan ids;
    uint32_t locale;
} NameStep;

typedef struct NameChain
{
    const mlocDataSet* data;
    const mlocLocale* locale;
    // nameChainMost + 1 when the chain did not fit.
    uint32_t count;
    NameStep steps[nameChainMost];
} NameChain;

// Resolves a locale's chain; false when it cannot be walked.
bool mlocOpenNameChain(const mlocDataSet* data, const mlocLocale* locale, NameChain* out);

// mlocFindName along an opened chain.
bool mlocFindChainName(const NameChain* chain, const char* code, size_t length, NameField field,
                       NameText* out);

#endif
