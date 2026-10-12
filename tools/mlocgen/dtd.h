// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What CLDR's DTDs say about each element and attribute, from their
// annotations: an attribute marked @VALUE or @METADATA does not
// distinguish elements; every other attribute does. An element marked
// @ORDERED keeps its children's order and is inherited whole.

#ifndef MLOCGEN_DTD_H
#define MLOCGEN_DTD_H

#include "map.h"

typedef enum AttributeKind
{
    attributeDistinguishing,
    attributeValue,
    attributeMetadata,
} AttributeKind;

typedef struct Dtd
{
    // Element name to its ElementInfo.
    Map elements;
} Dtd;

// Reads a DTD file into dtd, adding to what it holds; returns 0 and a
// message on failure.
int DtdLoad(Dtd* dtd, const char* path, char* errorOut, size_t errorCapacity);

// How an attribute of an element is used; an attribute the DTD does not
// declare counts as distinguishing.
AttributeKind DtdAttributeKind(const Dtd* dtd, const char* element, const char* attribute);

// Whether the DTD marks the element @ORDERED.
int DtdIsOrdered(const Dtd* dtd, const char* element);

void DtdFree(Dtd* dtd);

#endif
