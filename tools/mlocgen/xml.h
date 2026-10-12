// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A reader for the XML CLDR publishes: elements, attributes, character
// data, CDATA sections, comments, the five predefined entities and
// character references. A DOCTYPE is skipped, never read; nothing is
// fetched. Anything else is refused with its line.

#ifndef MLOCGEN_XML_H
#define MLOCGEN_XML_H

#include <stddef.h>

typedef struct XmlAttribute
{
    char* name;
    char* value;
} XmlAttribute;

typedef struct XmlNode XmlNode;
struct XmlNode
{
    char* name;
    XmlAttribute* attributes;
    int attributeCount;
    XmlNode** children;
    int childCount;
    // The element's character data, CDATA included, in document order and
    // without its children's; empty, never null.
    char* text;
    size_t textLength;
    XmlNode* parent;
    // The line of the element's start tag, from 1.
    int line;
};

// Parses a whole document held in memory. Returns its root element, or
// null with a message naming the line and the reason in errorOut.
XmlNode* XmlParse(const char* bytes, size_t length, char* errorOut, size_t errorCapacity);

// Reads and parses a file; null with a message on failure.
XmlNode* XmlParseFile(const char* path, char* errorOut, size_t errorCapacity);

// The value of an attribute, or null when the element has none.
const char* XmlGetAttribute(const XmlNode* node, const char* name);

// The first child with the given name, or null.
XmlNode* XmlFindChild(const XmlNode* node, const char* name);

// Frees a tree XmlParse returned.
void XmlFree(XmlNode* root);

#endif
