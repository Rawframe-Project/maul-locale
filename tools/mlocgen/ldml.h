// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// One CLDR XML file as UTS #35 reads it: a set of element chains, each
// with its value. A chain names every element from the root with its
// distinguishing attributes in alphabetical order, as in
//   //ldml/numbers/symbols[@numberSystem="latn"]/decimal
// The value is the leaf's text and its value attributes. Aliases are
// kept apart, by the chain of the element that contains them.

#ifndef MLOCGEN_LDML_H
#define MLOCGEN_LDML_H

#include "dtd.h"
#include "map.h"

typedef enum Draft
{
    draftApproved,
    draftContributed,
    draftProvisional,
    draftUnconfirmed,
} Draft;

typedef struct Leaf
{
    char* text;
    // The value attributes, in document order.
    char** valueNames;
    char** values;
    int valueCount;
    // The draft status, its own or the nearest enclosing element's.
    Draft draft;
    int line;
} Leaf;

typedef struct LdmlFile
{
    // Chain to Leaf.
    Map leaves;
    // Chain of the containing element to the alias's path attribute
    // (empty when it has none).
    Map aliases;
} LdmlFile;

// Reads and flattens a file; null with a message on failure.
LdmlFile* LdmlLoad(const char* path, const Dtd* dtd, char* errorOut, size_t errorCapacity);

// The value of a value attribute of a leaf, or null.
const char* LeafValue(const Leaf* leaf, const char* name);

void LdmlFree(LdmlFile* file);

#endif
