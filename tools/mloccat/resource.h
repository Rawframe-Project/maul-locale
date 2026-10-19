// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A message resource read as the W3C Internationalization WG's draft
// writes it (record mloc-0053; tools/resource-release.txt pins the
// grammar): frontmatter, sections, entries whose values are MF2 text,
// metadata and comments attached to what follows them. The data model
// is the draft's: escapes processed, whitespace and empty lines not
// kept. Only what the grammar allows is read.

#ifndef MLOCCAT_RESOURCE_H
#define MLOCCAT_RESOURCE_H

#include <stddef.h>

// A text that may hold any byte, a NUL included (an escaped \x00).
typedef struct ResText
{
    char* bytes;
    size_t length;
} ResText;

// An identifier: one or more non-empty parts.
typedef struct ResId
{
    ResText* parts;
    size_t count;
} ResId;

typedef struct ResMeta
{
    ResText key;
    ResText value;
} ResMeta;

// What a section holds, in order: entries and the comments that attach
// to none.
typedef enum ResKind
{
    resEntry,
    resComment,
} ResKind;

typedef struct ResItem
{
    ResKind kind;
    // An entry's identifier within its section; none for a comment.
    ResId id;
    // An entry's comment, or a standalone comment's text; lines joined
    // by LF.
    ResText comment;
    ResMeta* meta;
    size_t metaCount;
    // An entry's value, MF2 text with the escapes MF2 reads (\\, \{, \|,
    // \}) kept as written.
    ResText value;
    // The line of the entry's identifier, from 1.
    size_t line;
} ResItem;

typedef struct ResSection
{
    // Empty for the first, anonymous section.
    ResId id;
    ResText comment;
    ResMeta* meta;
    size_t metaCount;
    ResItem* items;
    size_t itemCount;
    size_t line;
} ResSection;

typedef struct Resource
{
    // The comment and metadata before the frontmatter.
    ResText comment;
    ResMeta* meta;
    size_t metaCount;
    // Whether the resource has a frontmatter line.
    int hasFrontmatter;
    ResSection* sections;
    size_t sectionCount;
} Resource;

// Where reading stopped, and why.
typedef struct ResError
{
    size_t line;
    size_t column;
    const char* reason;
} ResError;

// Reads a resource; 0 and the first error where the text is not one.
// The resource is freed with ResourceFree in either case.
int ResourceRead(const char* text, size_t length, Resource* out, ResError* error);

void ResourceFree(Resource* resource);

#endif
