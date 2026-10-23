// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A message resource written back as text, in one form: comments as "# "
// lines, metadata as "@key value", entries as "id = value" with later
// lines indented two spaces, sections after an empty line. Whatever the
// data model holds is written so that reading it gives the same model:
// characters the grammar does not allow unescaped are escaped.

#ifndef MLOCCAT_RESOURCE_WRITE_H
#define MLOCCAT_RESOURCE_WRITE_H

#include "resource.h"

#include <stdio.h>

void ResourceWrite(const Resource* resource, FILE* out);

#endif
