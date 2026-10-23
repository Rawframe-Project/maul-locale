// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time durations and the arithmetic of times of day (maul-locale/zone.h):
// the check the other parts share.

#ifndef MLOC_TIME_ARITH_H
#define MLOC_TIME_ARITH_H

#include "maul-locale/zone.h"

// Whether a time duration is one the library would make: of one sign,
// nanoseconds within a second, seconds below 2^53.
bool mlocTimeDurationValid(const mlocTimeDuration* duration);

#endif
