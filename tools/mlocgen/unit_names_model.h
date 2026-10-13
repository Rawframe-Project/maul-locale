// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names as ICU's data gives them (unit_names_gen.h): the
// identifiers the locale files name, and per locale, width, identifier
// and field the value ICU reads. ICU reads CLDR's production data, built
// from the development data as GenerateProductionData builds it, along
// the chain. A power of a gender is ICU's (InflectedPluralSink): per
// plural category, the first locale along the chain with the gender's
// pattern, neuter's or the plain one's.

#ifndef MLOCGEN_UNIT_NAMES_MODEL_H
#define MLOCGEN_UNIT_NAMES_MODEL_H

#include "map.h"
#include "table_gen.h"
#include "unit_names_gen.h"
#include "unit_tables.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    maxKey = 64,
    maxIds = 512,
    maxChain = 512,
    widthLong = 0,
    widthShort = 1,
    widthCount = 3,
    // A unit's gender, looked up as a field.
    fieldGender = unitNameFields,
    categoryCount = 6,
};

typedef struct UnitName
{
    // The identifier the library knows ("meter", "power2:feminine").
    char* id;
    // CLDR's type ("length-meter", "power2").
    char* type;
    // The type whose data it takes: ICU's "-person" durations take the
    // plain ones' (number_longnames.cpp, ICU-20400).
    char* source;
    // A power's gender, or null.
    char* gender;
    // A power of a gender: the plain power's index.
    int plain;
    // Its unitId kind.
    uint8_t kind;
} UnitName;

typedef struct Names
{
    const UnitNamesInput* input;
    UnitName names[maxIds];
    int count;
    // Identifier to its UnitName while collecting, then to its index + 1.
    Map byId;
    // Locale id to the plural categories of its rules (a bit per
    // category, plus 1 so none is null).
    Map categories;
    // Locale id to the fields it files, once taken (unit_names_gen.c).
    Map filed;
    // Root's unit aliases: a type to the type whose data it takes.
    Map aliases;
    Table table;
} Names;

// A copy of text, NUL-terminated.
char* NamesCopy(const char* text, size_t length);

// Collects the identifiers from every locale file and root's aliases,
// sorted.
void NamesCollect(Names* n);

// Reads the cardinal rules' categories per locale. Returns 0 and a
// message on failure.
int NamesReadCategories(Names* n, char* errorOut, size_t errorCapacity);

// A locale's plural categories and one (a bit per category).
unsigned NamesCategoriesOf(const Names* n, const char* locale);

// The chain of a name's field in a width, for a type and a gender, or
// false where the field does not apply.
bool NamesChainOf(const UnitName* name, const char* type, const char* gender, int width, int field,
                  char* out, size_t capacity);

// CLDR's resolution of a chain at a locale, or null.
const char* NamesResolved(const Names* n, const char* locale, const char* chain);

// Whether ICU's data has a plural category's own pattern.
bool NamesGiven(const Names* n, const char* locale, const UnitName* name, int width, int field);

// A field's value as ICU reads it, or null.
const char* NamesTarget(const Names* n, const char* locale, int id, int width, int field);

#endif
