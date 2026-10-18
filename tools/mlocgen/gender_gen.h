// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The gender of a list of persons, from supplemental/genderList.xml's
// personList rules, as the source of src/generated/list_genders.c
// (record mloc-0051).

#ifndef MLOCGEN_GENDER_GEN_H
#define MLOCGEN_GENDER_GEN_H

#include <stddef.h>

// The source of src/generated/list_genders.c in *sourceOut, which the
// caller frees. Returns 0 and a message on failure: an unreadable file,
// an unknown rule, a locale ID too long or listed twice, or no locale.
int GenerateListGenders(const char* commonDirectory, char** sourceOut, size_t* sourceSizeOut,
                        char* errorOut, size_t errorCapacity);

#endif
