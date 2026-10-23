// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The region partitions of language matching, as ICU's cldr-to-icu tool
// builds them (TerritoryContainment and PartitionInfo): the leaf regions
// of the territory containment, every group counted; each match variable
// and its inverse ("$!name") as sets of leaves; one partition per distinct
// set of variables a leaf is in; and each region mapped to its partitions
// (a macro region to those of its leaves). A partition is one character
// from '0'. A region with leaves is a macro region unless a group marks it
// as a grouping (EU, 419), as ICU's Region types them.

#ifndef MLOCGEN_MATCH_REGIONS_H
#define MLOCGEN_MATCH_REGIONS_H

#include "xml.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    // Packed regions (locale_tables.h) are below this.
    regionSlots = 2048,
    maxVariables = 64,
    maxGroups = 128,
    maxPartitions = 78,
};

typedef struct Partitions
{
    // The leaves below each packed region, as bitsets of regionSlots
    // bits; all zero for a leaf or a region outside the containment.
    uint64_t (*leavesOf)[regionSlots / 64];
    int variableCount;
    char variables[maxVariables][16];
    // The variables each leaf is in, as bits.
    uint64_t variablesOf[regionSlots];
    // Filled by PartitionsBuild: each region's group of partitions (0 is
    // ".", which no rule names), the groups' characters, and the
    // partitions of each variable.
    uint8_t groupOf[regionSlots];
    // A world, continent or subcontinent, as ICU's Region types them: a
    // region with leaves that no group marks as a grouping.
    bool macro[regionSlots];
    bool grouping[regionSlots];
    int groupCount;
    char groups[maxGroups][maxPartitions + 1];
    char partitionsOf[maxVariables][maxPartitions + 1];
} Partitions;

// Reads the territory containment of supplementalData.xml's root.
// Returns 0 and a message on failure.
int PartitionsOpen(Partitions* partitions, const XmlNode* supplementalData, char* errorOut,
                   size_t errorCapacity);

// Adds a match variable ("$enUS") and its inverse from an expression of
// regions joined by "+" and "-", applied in order, a macro region
// standing for its leaves.
void PartitionsAddVariable(Partitions* partitions, const char* variable, const char* expression);

// A rule's region: itself when it is "*" or a variable, else a variable
// made for that one region ("GB" gives "$GB").
const char* PartitionsEnsureVariable(Partitions* partitions, const char* region);

// Numbers the partitions and groups once every variable is in.
void PartitionsBuild(Partitions* partitions);

// The partitions of a variable as characters, or "*" for "*".
const char* PartitionsOf(const Partitions* partitions, const char* variable);

void PartitionsClose(Partitions* partitions);

#endif
