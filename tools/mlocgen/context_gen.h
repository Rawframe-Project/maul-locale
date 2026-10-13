// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context transforms (src/locale_context.h): per locale key, for each
// usage, the contexts in which CLDR titlecases its first word, filed
// where the key's chain would give others. They are compiled in.

#ifndef MLOCGEN_CONTEXT_GEN_H
#define MLOCGEN_CONTEXT_GEN_H

#include "list_gen.h"

// Adds the context transforms units to a file; the input is the lists'.
void GenerateContextUnits(const ListInput* input, DataFile* file);

#endif
