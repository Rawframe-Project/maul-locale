// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder, ported from ICU4C's CollationBuilder: each reset
// finds its position's elements through the tailoring's mappings (or a
// special position, or [before n] a level back); each relation inserts a
// node after that position in a list per root primary and maps its string
// (NFD, with its prefix and extension) to temporary elements naming the
// node, with case bits from the root's elements of the string. At the end
// the weight allocator gives each run of tailored nodes weights in the gap
// between the root weights around it, and the temporary elements become
// those. No canonical closure: the runtime reads NFD. [import] reads the
// imported rules in place, through the caller.

#ifndef MLOCGEN_COLLATION_BUILDER_H
#define MLOCGEN_COLLATION_BUILDER_H

#include "collation_mappings.h"
#include "collation_root.h"
#include "collation_root_elements.h"
#include "collation_rules.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The rules an [import] tag names (a language tag, its collation type
// in -u-co-), or null when there are none; they must stay until the
// builder is done.
typedef const CollationRules* (*CollationImporter)(void* context, const char* tag);

typedef struct CollationBuilder
{
    const RootTable* root;
    RootElements elements;
    // The tailoring's mappings, and the root's alone (for case bits).
    CollationMappings mappings;
    CollationMappings base;
    // ICU's node list and its root primaries' nodes, sorted by primary.
    uint64_t* nodes;
    size_t nodeCount;
    size_t nodeCapacity;
    int32_t* rootPrimaryIndexes;
    size_t rootPrimaryCount;
    size_t rootPrimaryCapacity;
    // The current position's elements.
    uint64_t ces[collationMaxExpansion];
    int32_t cesLength;
    // The root's variable top (punctuation's last primary) and Han's
    // first primary, for the special positions.
    uint32_t variableTop;
    uint32_t hanFirstPrimary;
    // The settings of the rules added, an import's before those of the
    // rules naming it (CLDR writes settings after imports).
    CollationRuleSettings settings;
    CollationImporter importer;
    void* importContext;
    int importDepth;
    char* error;
    size_t errorSize;
    bool failed;
} CollationBuilder;

// Prepares a builder over the root, which must outlive it; 0 with a
// message on failure.
int CollationBuilderInit(CollationBuilder* b, const RootTable* root, CollationImporter importer,
                         void* importContext, char* error, size_t errorSize);

void CollationBuilderFree(CollationBuilder* b);

// Adds a rule list's resets and relations, its imports' in their place,
// and its [suppressContractions] sets where they stand; gathers its
// settings into b->settings. 0 with ICU's reason when a rule cannot be
// built.
int CollationBuilderAdd(CollationBuilder* b, const CollationRules* rules);

// Gives the tailored nodes their weights and the mappings their final
// elements; 0 with a message when a gap is too small.
int CollationBuilderFinish(CollationBuilder* b);

enum
{
    collationNfdMax = 128,
};

// The NFD code points of a string, at most collationNfdMax in out; -1
// when longer.
int32_t CollationNfd(const uint32_t* text, uint32_t length, uint32_t* out);

#endif
