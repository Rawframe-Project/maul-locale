// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A translation accepted: each of its entries records the hash of the
// source message it now answers (@mloc-source, stale.h), so that a later
// change of the source marks it stale. The keys given are accepted, or
// with none every entry the source has; entries the source keeps
// (@translate no) and keys the source lacks are left as they are. The file
// is written again in the writer's form, through a temporary file renamed
// over it.

#ifndef MLOCCAT_ACCEPT_H
#define MLOCCAT_ACCEPT_H

#include <stddef.h>
#include <stdio.h>

// Accepts; writes problems and the count accepted to report. Returns 0
// when a resource did not read or check, a key given is not an entry of
// both, or the file could not be written.
int CatalogAccept(const char* source, const char* translation, char* const* keys, size_t keyCount,
                  FILE* report);

#endif
