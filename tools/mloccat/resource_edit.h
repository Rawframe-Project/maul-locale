// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog source changed by the tool: an entry's metadata set, an entry
// found or added by its section's and its own identifier, and the file
// written again in the writer's form through a temporary file that
// replaces it at once.

#ifndef MLOCCAT_RESOURCE_EDIT_H
#define MLOCCAT_RESOURCE_EDIT_H

#include "resource.h"

// Sets the item's metadata of key to value, replacing it or adding it.
void ResourceSetMeta(ResItem* item, const char* key, const char* value);

// The resource's entry of the section and entry identifiers, added at the
// end of its section (the section added at the end) when it has none.
// Adding moves a section's items, so pointers to them are not kept.
ResItem* ResourceEntryFor(Resource* r, const ResId* section, const ResId* entry);

// Replaces the item's value with a copy of text.
void ResourceSetValue(ResItem* item, const char* text, size_t length);

// Writes the resource over the file at path; 0 when it could not.
int ResourceWriteFile(const char* path, const Resource* r);

#endif
