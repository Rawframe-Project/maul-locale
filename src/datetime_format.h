// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date-time formatter made in a calendar other than its locale's
// (datetime_format.c), for a message's :datetime.

#ifndef MLOC_DATETIME_FORMAT_H
#define MLOC_DATETIME_FORMAT_H

#include "maul-locale/datetime.h"

// mlocInitDateTimeFormatter, in calendar (a BCP 47 calendar type) in place of
// the locale's -u-ca, or in its own for null.
mlocResult mlocInitDateTimeFormatterIn(const mlocDataSet* data, const mlocLocale* locale,
                                       const char* calendar, const mlocDateTimeOptions* options,
                                       mlocDateTimeFormatter* formatterOut);

#endif
