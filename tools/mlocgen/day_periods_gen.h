// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's day period rules (supplemental/dayPeriods.xml, the format rule
// set), compiled to the names unit's rules (src/date_tables.h): each
// hour's period, and whether noon and midnight are named at their
// instant (record mloc-0042).

#ifndef MLOCGEN_DAY_PERIODS_GEN_H
#define MLOCGEN_DAY_PERIODS_GEN_H

#include "map.h"

#include <stddef.h>

// Reads the format rule set into rules: each locale id it lists to its
// dateRuleSize bytes. Returns 0 with a message for a file that does not
// parse, a rule naming an unknown period or a time off the hour, or a
// locale whose rules leave an hour without a period.
int DayPeriodRulesLoad(Map* rules, const char* commonDirectory, char* errorOut,
                       size_t errorCapacity);

void DayPeriodRulesFree(Map* rules);

#endif
