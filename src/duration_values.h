// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration's fields read and checked as IsValidDuration checks them, for
// the writers that need only their values.

#ifndef MLOC_DURATION_VALUES_H
#define MLOC_DURATION_VALUES_H

#include "binary64.h"

#include "maul-locale/zone.h"

// Reads a duration's ten fields as magnitudes, and whether it is
// negative (a field below 0), when mlocMakeDuration takes it; false for
// a duration it refuses.
bool mlocReadValidDuration(const mlocDurationValues* values, Wide* magnitudes, bool* negativeOut);

#endif
