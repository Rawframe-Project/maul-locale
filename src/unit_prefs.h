// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The units a locale prefers for a usage, for the unit formatter: the
// route of an input unit, every output unit the value may choose, and the
// precision an output takes.

#ifndef MLOC_UNIT_PREFS_H
#define MLOC_UNIT_PREFS_H

#include "unit_convert.h"
#include "unit_data.h"
#include "unit_id.h"

#include "maul-locale/unit.h"

enum
{
    // The most output units a route holds (CLDR 49's longest run has 7).
    unitMaxOutputs = 8,
    // The longest base unit identifier written, and the longest usage.
    unitMaxBaseId = 256,
    unitMaxUsage = 64,
};

// An output unit: its identifier, and the threshold a value must reach in
// its (largest part's) unit, threshold / 10^places; the last one takes
// any value. The preference's rounding increment in units of the last
// part, 0 for none.
typedef struct UnitRouteEntry
{
    const char* id;
    uint32_t threshold;
    uint16_t increment;
    uint8_t length;
    uint8_t places;
} UnitRouteEntry;

// The outputs of an input unit, in order: the -u-mu unit, else the
// preferences of its quantity, usage and region, else its base unit.
typedef struct UnitRoute
{
    UnitRouteEntry entries[unitMaxOutputs];
    uint32_t count;
    // The base unit's identifier, which an entry names.
    char base[unitMaxBaseId];
} UnitRoute;

// Whether a usage is well-formed: parts of 3 to 8 letters or digits.
bool mlocValidUsage(const char* usage, size_t length);

// The route of an input unit for a usage (empty for "default"), as
// mlocGetUnitPreference chooses. Returns mloc_success; mloc_errorNotFound
// for data without the preferences; mloc_errorFormat for preferences the
// tables do not read; mloc_errorInvalid for a base unit past the room.
mlocResult mlocRouteUnit(const mlocDataSet* data, const mlocLocale* locale, const UnitTable* t,
                         const UnitId* input, const UnitScale* inputScale, const char* usage,
                         size_t usageLength, UnitRoute* out);

// The precision an output's last amount takes: the one given when it has
// a kind, else the preference's increment, else an integer of at least
// two significant digits (ICU's UnitsRouter).
const mlocPrecision* mlocRoutePrecision(const mlocPrecision* given, uint32_t increment,
                                        mlocPrecision* own);

#endif
