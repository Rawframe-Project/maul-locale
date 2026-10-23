// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The rule strings of collation files: each file's types parsed and its
// default type, with [import] resolved among them. The generator reads
// CLDR's common/collation through it, as does the builder's conformance
// tool.

#ifndef MLOCGEN_COLLATION_CATALOG_H
#define MLOCGEN_COLLATION_CATALOG_H

#include "collation_rules.h"

#include <stddef.h>

typedef struct CollationType
{
    // The file's name without ".xml", a CLDR id ("root" for root).
    char file[64];
    // CLDR's type name and alt, "" for none.
    char type[64];
    char alt[32];
    // Its place among the types read, which keeps a file's order.
    size_t order;
    char* text;
    CollationRules rules;
    bool parsed;
    // Why the rule string did not parse.
    char error[160];
} CollationType;

typedef struct CollationFile
{
    char name[64];
    // The file's <defaultCollation>, "" for none.
    char defaultType[64];
} CollationFile;

typedef struct CollationCatalog
{
    CollationType* types;
    size_t count;
    size_t capacity;
    CollationFile* files;
    size_t fileCount;
    size_t fileCapacity;
} CollationCatalog;

// Reads a file's types and default; false with a message when the file
// does not read. A rule string that does not parse is kept unparsed.
bool CollationCatalogRead(CollationCatalog* catalog, const char* path, char* error,
                          size_t errorSize);

// Sorts the types by file name, a file's types in the order read.
void CollationCatalogSort(CollationCatalog* catalog);

// An importer for CollationBuilderInit (its context the catalog): a tag
// "language-u-co-type" names the language's file ("und" for root) and
// its type, in BCP 47's spelling; no -u-co- names "standard".
const CollationRules* CollationCatalogImport(void* catalog, const char* tag);

// A CLDR type name as BCP 47's co spells it ("phonebook" gives "phonebk").
const char* CollationTypeCode(const char* type);

void CollationCatalogFree(CollationCatalog* catalog);

#endif
