// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Region partitions (match_regions.h). Regions are visited in the order
// of their codes' text, three digits before two letters, as ICU's sorted
// maps visit them, so the partitions get the same characters as ICU's.

#include "match_regions.h"

#include "locale_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    world = 1024 + 1,
    words = regionSlots / 64,
};

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

static uint32_t Slot(const char* code, size_t length)
{
    uint32_t slot = mlocPackRegion(code, (uint32_t)length);
    if (slot == 0)
    {
        char text[16];
        snprintf(text, sizeof(text), "%.*s", (int)(length < 8 ? length : 8), code);
        Die("not a region code", text);
    }
    return slot;
}

// The k-th slot in the order of the codes' text.
static uint32_t InOrder(uint32_t k)
{
    return k < 1024 ? 1024 + k : k - 1024;
}

static bool Has(const uint64_t* set, uint32_t slot)
{
    return (set[slot / 64] >> (slot % 64) & 1) != 0;
}

static void Set(uint64_t* set, uint32_t slot, bool on)
{
    if (on)
    {
        set[slot / 64] |= 1ull << (slot % 64);
    }
    else
    {
        set[slot / 64] &= ~(1ull << (slot % 64));
    }
}

static bool Empty(const uint64_t* set)
{
    for (int w = 0; w < words; w++)
    {
        if (set[w] != 0)
        {
            return false;
        }
    }
    return true;
}

// Fills leavesOf[slot] from the direct contents, depth first; done marks
// the regions already resolved.
static void Resolve(Partitions* p, const uint64_t (*children)[words], bool* done, uint32_t slot,
                    int depth)
{
    if (done[slot])
    {
        return;
    }
    if (depth > 16)
    {
        Die("the territory containment loops at", "a region");
    }
    for (uint32_t c = 0; c < regionSlots; c++)
    {
        if (!Has(children[slot], c))
        {
            continue;
        }
        if (Empty(children[c]))
        {
            Set(p->leavesOf[slot], c, true);
            continue;
        }
        Resolve(p, children, done, c, depth + 1);
        for (int w = 0; w < words; w++)
        {
            p->leavesOf[slot][w] |= p->leavesOf[c][w];
        }
    }
    done[slot] = true;
}

static bool Is(const XmlNode* node, const char* attribute, const char* value)
{
    const char* text = XmlGetAttribute(node, attribute);
    return text != nullptr && strcmp(text, value) == 0;
}

// Adds a group's contents; a group marked grouping="true" is a grouping,
// and so is each region of a group of status "grouping".
static void AddGroup(Partitions* p, uint64_t (*children)[words], uint64_t* contained,
                     const XmlNode* group)
{
    const char* type = XmlGetAttribute(group, "type");
    const char* contains = XmlGetAttribute(group, "contains");
    if (type == nullptr || contains == nullptr)
    {
        return;
    }
    uint32_t parent = Slot(type, strlen(type));
    p->grouping[parent] |= Is(group, "grouping", "true");
    bool groupings = Is(group, "status", "grouping");
    for (const char* at = contains; *at != '\0';)
    {
        size_t length = strcspn(at, " ");
        if (length > 0)
        {
            uint32_t child = Slot(at, length);
            Set(children[parent], child, true);
            p->grouping[child] |= groupings;
            Set(contained, child, true);
        }
        at += length;
        at += strspn(at, " ");
    }
}

int PartitionsOpen(Partitions* partitions, const XmlNode* supplementalData, char* errorOut,
                   size_t errorCapacity)
{
    memset(partitions, 0, sizeof(*partitions));
    const XmlNode* containment = XmlFindChild(supplementalData, "territoryContainment");
    uint64_t (*children)[words] = calloc(regionSlots, sizeof(*children));
    partitions->leavesOf = calloc(regionSlots, sizeof(*partitions->leavesOf));
    bool* done = calloc(regionSlots, sizeof(bool));
    if (containment == nullptr || children == nullptr || partitions->leavesOf == nullptr ||
        done == nullptr)
    {
        snprintf(errorOut, errorCapacity, "no territory containment, or out of memory");
        free(children);
        free(done);
        return 0;
    }
    uint64_t contained[words] = {0};
    for (int i = 0; i < containment->childCount; i++)
    {
        AddGroup(partitions, children, contained, containment->children[i]);
    }
    for (uint32_t slot = 0; slot < regionSlots; slot++)
    {
        // The world is the containment's only root, as ICU checks.
        if (!Empty(children[slot]) && slot != world && !Has(contained, slot))
        {
            Die("a containment root other than the world", "region");
        }
    }
    Resolve(partitions, (const uint64_t (*)[words])children, done, world, 0);
    for (uint32_t slot = 0; slot < regionSlots; slot++)
    {
        Resolve(partitions, (const uint64_t (*)[words])children, done, slot, 0);
        partitions->macro[slot] = !Empty(partitions->leavesOf[slot]) && !partitions->grouping[slot];
    }
    free(children);
    free(done);
    return 1;
}

static int FindVariable(const Partitions* p, const char* variable)
{
    for (int v = 0; v < p->variableCount; v++)
    {
        if (strcmp(p->variables[v], variable) == 0)
        {
            return v;
        }
    }
    return -1;
}

static int NewVariable(Partitions* p, const char* variable)
{
    if (FindVariable(p, variable) >= 0 || p->variableCount == maxVariables ||
        strlen(variable) >= sizeof(p->variables[0]))
    {
        Die("a match variable twice, too long or one too many", variable);
    }
    snprintf(p->variables[p->variableCount], sizeof(p->variables[0]), "%s", variable);
    return p->variableCount++;
}

// Adds or removes a region, or the leaves of a macro region.
static void Apply(const Partitions* p, uint64_t* set, const char* code, size_t length, bool add)
{
    if (length == 0)
    {
        Die("a region expression with an empty region", code);
    }
    uint32_t slot = Slot(code, length);
    if (Empty(p->leavesOf[slot]))
    {
        Set(set, slot, add);
        return;
    }
    for (int w = 0; w < words; w++)
    {
        set[w] = add ? set[w] | p->leavesOf[slot][w] : set[w] & ~p->leavesOf[slot][w];
    }
}

void PartitionsAddVariable(Partitions* partitions, const char* variable, const char* expression)
{
    if (variable[0] != '$' || variable[1] == '!')
    {
        Die("a match variable not of the form $name", variable);
    }
    uint64_t set[words] = {0};
    bool add = true;
    for (const char* at = expression;;)
    {
        size_t length = strcspn(at, "+-");
        Apply(partitions, set, at, length, add);
        if (at[length] == '\0')
        {
            break;
        }
        add = at[length] == '+';
        at += length + 1;
    }
    char inverse[32];
    snprintf(inverse, sizeof(inverse), "$!%s", variable + 1);
    int v = NewVariable(partitions, variable);
    int notV = NewVariable(partitions, inverse);
    const uint64_t* leaves = partitions->leavesOf[world];
    for (uint32_t slot = 0; slot < regionSlots; slot++)
    {
        if (Has(set, slot))
        {
            partitions->variablesOf[slot] |= 1ull << v;
        }
        else if (Has(leaves, slot))
        {
            partitions->variablesOf[slot] |= 1ull << notV;
        }
    }
}

const char* PartitionsEnsureVariable(Partitions* partitions, const char* region)
{
    if (strcmp(region, "*") == 0)
    {
        return region;
    }
    int v = FindVariable(partitions, region);
    if (v >= 0)
    {
        return partitions->variables[v];
    }
    if (region[0] == '$')
    {
        Die("a match variable never defined", region);
    }
    char variable[16];
    snprintf(variable, sizeof(variable), "$%s", region);
    v = FindVariable(partitions, variable);
    if (v < 0)
    {
        v = NewVariable(partitions, variable);
        partitions->variablesOf[Slot(region, strlen(region))] |= 1ull << v;
    }
    return partitions->variables[v];
}

// The group of a set of partitions given as flags, added when new.
static uint8_t Group(Partitions* p, const bool* flags)
{
    char text[maxPartitions + 1];
    int n = 0;
    for (int i = 0; i < maxPartitions; i++)
    {
        if (flags[i])
        {
            text[n++] = (char)('0' + i);
        }
    }
    text[n] = '\0';
    for (int g = 0; g < p->groupCount; g++)
    {
        if (strcmp(p->groups[g], text) == 0)
        {
            return (uint8_t)g;
        }
    }
    if (p->groupCount == maxGroups)
    {
        Die("too many partition groups", text);
    }
    snprintf(p->groups[p->groupCount], sizeof(p->groups[0]), "%s", text);
    return (uint8_t)p->groupCount++;
}

// Numbers the partitions: one per distinct set of variables, in the
// order the leaves come. Returns each leaf's partition, -1 for others.
static void NumberPartitions(Partitions* p, int* partitionOf)
{
    uint64_t seen[maxPartitions];
    int count = 0;
    const uint64_t* leaves = p->leavesOf[world];
    for (uint32_t k = 0; k < regionSlots; k++)
    {
        uint32_t slot = InOrder(k);
        partitionOf[slot] = -1;
        if (Has(leaves, slot) != (p->variablesOf[slot] != 0))
        {
            Die("the match variables do not cover exactly the leaf regions", "partitions");
        }
        if (p->variablesOf[slot] == 0)
        {
            continue;
        }
        int i = 0;
        while (i < count && seen[i] != p->variablesOf[slot])
        {
            i++;
        }
        if (i == count)
        {
            if (count == maxPartitions)
            {
                Die("too many region partitions", "partitions");
            }
            seen[count++] = p->variablesOf[slot];
        }
        partitionOf[slot] = i;
    }
}

void PartitionsBuild(Partitions* partitions)
{
    int* partitionOf = calloc(regionSlots, sizeof(int));
    bool (*byVariable)[maxPartitions] = calloc(maxVariables, sizeof(*byVariable));
    if (partitionOf == nullptr || byVariable == nullptr)
    {
        Die("out of memory", "partitions");
    }
    NumberPartitions(partitions, partitionOf);
    bool none[maxPartitions] = {false};
    partitions->groupCount = 1;
    snprintf(partitions->groups[0], sizeof(partitions->groups[0]), ".");
    for (uint32_t k = 0; k < regionSlots; k++)
    {
        uint32_t slot = InOrder(k);
        if (partitionOf[slot] < 0)
        {
            continue;
        }
        bool flags[maxPartitions] = {false};
        flags[partitionOf[slot]] = true;
        partitions->groupOf[slot] = Group(partitions, flags);
        for (int v = 0; v < partitions->variableCount; v++)
        {
            byVariable[v][partitionOf[slot]] |= (partitions->variablesOf[slot] >> v & 1) != 0;
        }
    }
    for (uint32_t k = 0; k < regionSlots; k++)
    {
        uint32_t slot = InOrder(k);
        if (Empty(partitions->leavesOf[slot]))
        {
            continue;
        }
        bool flags[maxPartitions] = {false};
        for (uint32_t leaf = 0; leaf < regionSlots; leaf++)
        {
            if (Has(partitions->leavesOf[slot], leaf) && partitionOf[leaf] >= 0)
            {
                flags[partitionOf[leaf]] = true;
            }
        }
        partitions->groupOf[slot] =
            memcmp(flags, none, sizeof(none)) == 0 ? 0 : Group(partitions, flags);
    }
    for (int v = 0; v < partitions->variableCount; v++)
    {
        int n = 0;
        for (int i = 0; i < maxPartitions; i++)
        {
            if (byVariable[v][i])
            {
                partitions->partitionsOf[v][n++] = (char)('0' + i);
            }
        }
        partitions->partitionsOf[v][n] = '\0';
    }
    free(partitionOf);
    free(byVariable);
}

const char* PartitionsOf(const Partitions* partitions, const char* variable)
{
    if (strcmp(variable, "*") == 0)
    {
        return "*";
    }
    int v = FindVariable(partitions, variable);
    if (v < 0 || partitions->partitionsOf[v][0] == '\0')
    {
        Die("a match variable with no partitions", variable);
    }
    return partitions->partitionsOf[v];
}

void PartitionsClose(Partitions* partitions)
{
    free(partitions->leavesOf);
    partitions->leavesOf = nullptr;
}
