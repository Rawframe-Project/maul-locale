// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date-time at an instant and an offset, for a zone's offset read
// once.

#ifndef MLOC_ZONE_LOCAL_H
#define MLOC_ZONE_LOCAL_H

#include "maul-locale/zone.h"

#include <stdint.h>

// The date-time in a calendar at an instant in range and an offset in
// seconds from UTC.
mlocResult mlocLocalDateTime(const mlocInstant* instant, int32_t offset, mlocCalendar calendar,
                             mlocDateTime* dateTimeOut);

#endif
