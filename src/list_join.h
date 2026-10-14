// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Items joined by a locale's list patterns, as ICU's ListFormatter joins
// them: one item as it is, two by the "two" pattern, more by the start
// pattern, the middle ones and the end one, with the language's
// contextual changes.

#ifndef MLOC_LIST_JOIN_H
#define MLOC_LIST_JOIN_H

#include "list_data.h"
#include "number_core.h"

#include "maul-locale/list.h"

#include <stddef.h>
#include <stdint.h>

// An item's text, which the contextual changes read: the public list
// item, so that a caller's array is joined as it is.
typedef mlocListItem ListItem;

// Writes an item, given its index (to report its parts).
typedef void (*ListItemWriter)(void* context, uint32_t index, NumberWriter* writer);

// Writes count items (at least one) joined by the patterns; each item by
// write when it is set, else as its text, literal.
void mlocWriteList(const ListPatterns* patterns, const ListItem* items, uint32_t count,
                   ListItemWriter write, void* context, NumberWriter* writer);

#endif
