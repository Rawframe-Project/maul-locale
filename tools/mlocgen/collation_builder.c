// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder (collation_builder.h), ported from ICU4C's
// CollationBuilder: addReset, getWeight16Before, getSpecialResetPosition,
// addRelation, the node list's finding and inserting, setCaseBits,
// makeTailoredCEs and finalizeCEs. Strengths inside are ICU's (primary
// 0 to quaternary 3, identical 15), as the node list stores them.

#include "collation_builder.h"

#include "collation_unit.h"
#include "collation_weights.h"

#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    sPrimary = 0,
    sSecondary = 1,
    sTertiary = 2,
    sQuaternary = 3,
    sIdentical = 15,
    // The longest string a rule may hold, in NFD code points.
    maxString = 128,
    maxImportDepth = 8,
};

constexpr uint32_t commonWeight16 = 0x0500;
constexpr uint32_t beforeWeight16 = 0x0100;
constexpr uint32_t firstTrailingPrimary = 0xFF020200;
constexpr uint64_t hasBefore2 = 0x40;
constexpr uint64_t hasBefore3 = 0x20;
constexpr uint64_t isTailored = 8;
constexpr int32_t maxIndex = 0xFFFFF;
constexpr uint64_t tempOffset = 0x4040000006002000ULL;

static int Fail(CollationBuilder* b, const char* format, ...)
{
    if (!b->failed)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(b->error, b->errorSize, format, args);
        va_end(args);
    }
    b->failed = true;
    return 0;
}

// The node list's fields (ICU's comment on CollationBuilder::nodes).
static uint64_t NodeFromWeight32(uint32_t w)
{
    return (uint64_t)w << 32;
}

static uint64_t NodeFromWeight16(uint32_t w)
{
    return (uint64_t)w << 48;
}

static uint32_t Weight32(uint64_t node)
{
    return (uint32_t)(node >> 32);
}

static uint32_t Weight16(uint64_t node)
{
    return (uint32_t)(node >> 48) & 0xFFFF;
}

static int32_t PreviousOf(uint64_t node)
{
    return (int32_t)((node >> 28) & maxIndex);
}

static int32_t NextOf(uint64_t node)
{
    return (int32_t)((node >> 8) & maxIndex);
}

static int32_t StrengthOf(uint64_t node)
{
    return (int32_t)(node & 3);
}

static bool IsTailored(uint64_t node)
{
    return (node & isTailored) != 0;
}

static uint64_t WithPrevious(uint64_t node, int32_t previous)
{
    return (node & 0xFFFF00000FFFFFFFULL) | (uint64_t)previous << 28;
}

static uint64_t WithNext(uint64_t node, int32_t next)
{
    return (node & 0xFFFFFFFFF00000FFULL) | (uint64_t)next << 8;
}

// A temporary element naming a node and a strength, with secondary bytes
// 06 to 45, which no root element has.
static uint64_t TempCe(int32_t index, int32_t strength)
{
    uint64_t i = (uint64_t)index;
    return tempOffset + ((i & 0xFE000) << 43) + ((i & 0x1FC0) << 42) + ((i & 0x3F) << 24) +
           ((uint64_t)strength << 8);
}

static int32_t IndexOfTemp(uint64_t ce)
{
    ce -= tempOffset;
    return (int32_t)(((uint32_t)(ce >> 43) & 0xFE000) | ((uint32_t)(ce >> 42) & 0x1FC0) |
                     ((uint32_t)(ce >> 24) & 0x3F));
}

static bool IsTemp(uint64_t ce)
{
    uint32_t secondary = (uint32_t)ce >> 24;
    return secondary >= 6 && secondary <= 0x45;
}

static int32_t CeStrength(uint64_t ce)
{
    return IsTemp(ce)                          ? (int32_t)(((uint32_t)ce >> 8) & 3)
           : (ce & 0xFF00000000000000ULL) != 0 ? sPrimary
           : ((uint32_t)ce & 0xFF000000) != 0  ? sSecondary
           : ce != 0                           ? sTertiary
                                               : sIdentical;
}

static uint64_t MakeCe(uint32_t p, uint32_t s, uint32_t t, uint32_t q)
{
    return (uint64_t)p << 32 | s << 16 | t | q << 6;
}

static bool Compressible(const CollationBuilder* b, uint32_t p)
{
    return b->root->compressible[p >> 24] || p >> 24 == collationUnassignedByte;
}

// ICU's addScriptStart: a start's top 16 bits, put back to its lead byte
// when it is that byte's lowest.
static uint32_t Start16(const RootTable* t, uint32_t p)
{
    p >>= 16;
    uint32_t lowest = t->compressible[p >> 8] || p >> 8 == collationUnassignedByte ? 4 : 2;
    return (p & 0xFF) == lowest ? p & 0xFF00 : p;
}

// The root's variable top (the last primary before the group after
// punctuation) and Han's first primary.
static void Groups(CollationBuilder* b)
{
    const RootTable* t = b->root;
    uint32_t punctuation = 0;
    for (size_t i = 0; i < t->scriptStartCount; i++)
    {
        uint32_t start = Start16(t, (uint32_t)(t->scriptStarts[i].element >> 32));
        uint32_t sample = t->scriptStarts[i].codePoint;
        punctuation = sample == 0x201C ? start : punctuation;
        if (sample != 0xFDD0 && muniGetScript(sample) == MUNI_MAKE_SCRIPT('H', 'a', 'n', 'i'))
        {
            b->hanFirstPrimary = start << 16;
        }
    }
    uint32_t next = 0;
    for (size_t i = 0; punctuation != 0 && i < t->scriptStartCount; i++)
    {
        uint32_t start = Start16(t, (uint32_t)(t->scriptStarts[i].element >> 32));
        next = start > punctuation && (next == 0 || start < next) ? start : next;
    }
    b->variableTop = next != 0 ? (next << 16) - 1 : 0;
}

int CollationBuilderInit(CollationBuilder* b, const RootTable* root, CollationImporter importer,
                         void* importContext, char* error, size_t errorSize)
{
    *b = (CollationBuilder){.root = root,
                            .importer = importer,
                            .importContext = importContext,
                            .error = error,
                            .errorSize = errorSize,
                            .settings = {.hanRadical = root->radicalOrder,
                                         .strength = ruleUnset,
                                         .alternate = ruleUnset,
                                         .maxVariable = ruleUnset,
                                         .caseFirst = ruleUnset,
                                         .caseLevel = ruleUnset,
                                         .numeric = ruleUnset,
                                         .backwards = ruleUnset}};
    if (!RootElementsBuild(root, &b->elements, error, errorSize))
    {
        return 0;
    }
    if (!CollationMappingsInit(&b->mappings, root) || !CollationMappingsInit(&b->base, root))
    {
        CollationBuilderFree(b);
        snprintf(error, errorSize, "out of memory");
        return 0;
    }
    Groups(b);
    return 1;
}

void CollationBuilderFree(CollationBuilder* b)
{
    RootElementsFree(&b->elements);
    CollationMappingsFree(&b->mappings);
    CollationMappingsFree(&b->base);
    free(b->nodes);
    free(b->rootPrimaryIndexes);
    b->nodes = nullptr;
    b->rootPrimaryIndexes = nullptr;
}

static int32_t AddNode(CollationBuilder* b, uint64_t node)
{
    if (b->nodeCount > (size_t)maxIndex)
    {
        return Fail(b, "too many tailoring nodes"), -1;
    }
    if (b->nodeCount == b->nodeCapacity)
    {
        size_t capacity = b->nodeCapacity == 0 ? 1024 : 2 * b->nodeCapacity;
        uint64_t* grown = realloc(b->nodes, capacity * sizeof *grown);
        if (grown == nullptr)
        {
            return Fail(b, "out of memory"), -1;
        }
        b->nodes = grown;
        b->nodeCapacity = capacity;
    }
    b->nodes[b->nodeCount] = node;
    return (int32_t)b->nodeCount++;
}

static int32_t InsertNodeBetween(CollationBuilder* b, int32_t index, int32_t next, uint64_t node)
{
    int32_t added = AddNode(b, node | (uint64_t)index << 28 | (uint64_t)next << 8);
    if (added < 0)
    {
        return 0;
    }
    b->nodes[index] = WithNext(b->nodes[index], added);
    if (next != 0)
    {
        b->nodes[next] = WithPrevious(b->nodes[next], added);
    }
    return added;
}

static int32_t FindOrInsertNodeForPrimary(CollationBuilder* b, uint32_t p)
{
    size_t lo = 0;
    size_t hi = b->rootPrimaryCount;
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        uint32_t q = Weight32(b->nodes[b->rootPrimaryIndexes[mid]]);
        if (q == p)
        {
            return b->rootPrimaryIndexes[mid];
        }
        if (q < p)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    int32_t index = AddNode(b, NodeFromWeight32(p));
    if (index < 0)
    {
        return 0;
    }
    if (b->rootPrimaryCount == b->rootPrimaryCapacity)
    {
        size_t capacity = b->rootPrimaryCapacity == 0 ? 256 : 2 * b->rootPrimaryCapacity;
        int32_t* grown = realloc(b->rootPrimaryIndexes, capacity * sizeof *grown);
        if (grown == nullptr)
        {
            return Fail(b, "out of memory");
        }
        b->rootPrimaryIndexes = grown;
        b->rootPrimaryCapacity = capacity;
    }
    memmove(b->rootPrimaryIndexes + lo + 1, b->rootPrimaryIndexes + lo,
            (b->rootPrimaryCount - lo) * sizeof *b->rootPrimaryIndexes);
    b->rootPrimaryIndexes[lo] = index;
    b->rootPrimaryCount++;
    return index;
}

// ICU's findCommonNode: the node with the level's explicit common weight
// when the node is stronger and has weights below it at the level.
static int32_t FindCommonNode(const CollationBuilder* b, int32_t index, int32_t strength)
{
    uint64_t node = b->nodes[index];
    if (StrengthOf(node) >= strength ||
        (node & (strength == sSecondary ? hasBefore2 : hasBefore3)) == 0)
    {
        return index;
    }
    index = NextOf(node);
    node = b->nodes[index];
    do
    {
        index = NextOf(node);
        node = b->nodes[index];
    } while (index != 0 &&
             (IsTailored(node) || StrengthOf(node) > strength || Weight16(node) < commonWeight16));
    return index;
}

// The first weight below the common one under a stronger node: that
// node flagged, then the weight's node and an explicit common one after
// it, the stronger node's tertiary flag moved onto the common
// secondary.
static int32_t InsertBelowCommon(CollationBuilder* b, int32_t index, uint32_t weight16,
                                 int32_t level)
{
    uint64_t node = b->nodes[index];
    uint64_t common = NodeFromWeight16(commonWeight16) | (uint64_t)level;
    if (level == sSecondary)
    {
        common |= node & hasBefore3;
        node &= ~hasBefore3;
    }
    b->nodes[index] = node | (level == sSecondary ? hasBefore2 : hasBefore3);
    int32_t next = NextOf(node);
    index = InsertNodeBetween(b, index, next, NodeFromWeight16(weight16) | (uint64_t)level);
    InsertNodeBetween(b, index, next, common);
    return index;
}

static int32_t FindOrInsertWeakNode(CollationBuilder* b, int32_t index, uint32_t weight16,
                                    int32_t level)
{
    if (weight16 == commonWeight16)
    {
        return FindCommonNode(b, index, level);
    }
    uint64_t node = b->nodes[index];
    uint64_t before = level == sSecondary ? hasBefore2 : hasBefore3;
    if (weight16 != 0 && weight16 < commonWeight16 && (node & before) == 0)
    {
        return InsertBelowCommon(b, index, weight16, level);
    }
    int32_t next = 0;
    while ((next = NextOf(node)) != 0)
    {
        node = b->nodes[next];
        int32_t strength = StrengthOf(node);
        if (strength <= level)
        {
            if (strength < level)
            {
                break;
            }
            if (!IsTailored(node))
            {
                uint32_t nextWeight16 = Weight16(node);
                if (nextWeight16 == weight16)
                {
                    return next;
                }
                if (nextWeight16 > weight16)
                {
                    break;
                }
            }
        }
        index = next;
    }
    return InsertNodeBetween(b, index, next, NodeFromWeight16(weight16) | (uint64_t)level);
}

static int32_t FindOrInsertNodeForRootCe(CollationBuilder* b, uint64_t ce, int32_t strength)
{
    int32_t index = FindOrInsertNodeForPrimary(b, (uint32_t)(ce >> 32));
    if (strength >= sSecondary)
    {
        uint32_t lower32 = (uint32_t)ce;
        index = FindOrInsertWeakNode(b, index, lower32 >> 16, sSecondary);
        if (strength >= sTertiary)
        {
            index = FindOrInsertWeakNode(b, index, lower32 & 0x3F3F, sTertiary);
        }
    }
    return index;
}

static int32_t FindOrInsertNodeForCes(CollationBuilder* b, int32_t strength)
{
    uint64_t ce = 0;
    for (;; --b->cesLength)
    {
        if (b->cesLength == 0)
        {
            ce = b->ces[0] = 0;
            b->cesLength = 1;
            break;
        }
        ce = b->ces[b->cesLength - 1];
        if (CeStrength(ce) <= strength)
        {
            break;
        }
    }
    if (IsTemp(ce))
    {
        return IndexOfTemp(ce);
    }
    if ((uint8_t)(ce >> 56) == collationUnassignedByte)
    {
        return Fail(b, "tailoring relative to an unassigned code point not supported");
    }
    return FindOrInsertNodeForRootCe(b, ce, strength);
}

static int32_t InsertTailoredNodeAfter(CollationBuilder* b, int32_t index, int32_t strength)
{
    if (strength >= sSecondary)
    {
        index = FindCommonNode(b, index, sSecondary);
        if (strength >= sTertiary)
        {
            index = FindCommonNode(b, index, sTertiary);
        }
    }
    uint64_t node = b->nodes[index];
    int32_t next = 0;
    while ((next = NextOf(node)) != 0)
    {
        node = b->nodes[next];
        if (StrengthOf(node) <= strength)
        {
            break;
        }
        index = next;
    }
    return InsertNodeBetween(b, index, next, isTailored | (uint64_t)strength);
}

// ICU's getWeight16Before: the root weight before the node's at the
// level, or 0100 when the node is under a tailored one.
static uint32_t GetWeight16Before(const CollationBuilder* b, int32_t index, uint64_t node,
                                  int32_t level)
{
    uint32_t t = StrengthOf(node) == sTertiary ? Weight16(node) : commonWeight16;
    while (StrengthOf(node) > sSecondary)
    {
        index = PreviousOf(node);
        node = b->nodes[index];
    }
    if (IsTailored(node))
    {
        return beforeWeight16;
    }
    uint32_t s = StrengthOf(node) == sSecondary ? Weight16(node) : commonWeight16;
    while (StrengthOf(node) > sPrimary)
    {
        index = PreviousOf(node);
        node = b->nodes[index];
    }
    if (IsTailored(node))
    {
        return beforeWeight16;
    }
    uint32_t p = Weight32(node);
    return level == sSecondary ? RootElementsSecondaryBefore(&b->elements, p, s)
                               : RootElementsTertiaryBefore(&b->elements, p, s, t);
}

// The first node tailored before a node at a weaker strength, past its
// below-common nodes (ICU's [first xyz] with HAS_BEFORE2 or 3).
static int32_t PastBefore(const CollationBuilder* b, int32_t index)
{
    uint64_t node = b->nodes[index];
    if ((node & hasBefore2) != 0)
    {
        index = NextOf(b->nodes[NextOf(node)]);
        node = b->nodes[index];
    }
    if ((node & hasBefore3) != 0)
    {
        index = NextOf(b->nodes[NextOf(node)]);
    }
    return index;
}

static uint64_t FirstSecondaryIgnorable(CollationBuilder* b)
{
    int32_t index = FindOrInsertNodeForRootCe(b, 0, sTertiary);
    index = NextOf(b->nodes[index]);
    if (index != 0)
    {
        uint64_t node = b->nodes[index];
        if (IsTailored(node) && StrengthOf(node) == sTertiary)
        {
            return TempCe(index, sTertiary);
        }
    }
    return RootElementsFirstTertiary(&b->elements);
}

// [first primary ignorable]: a secondary node tailored after [0, 0, *],
// else the root's first secondary element.
static bool FirstPrimaryIgnorable(CollationBuilder* b, uint64_t* ceOut)
{
    int32_t index = FindOrInsertNodeForRootCe(b, 0, sSecondary);
    uint64_t node = b->nodes[index];
    while ((index = NextOf(node)) != 0)
    {
        node = b->nodes[index];
        int32_t strength = StrengthOf(node);
        if (strength < sSecondary)
        {
            break;
        }
        if (strength == sSecondary)
        {
            if (!IsTailored(node))
            {
                break;
            }
            if ((node & hasBefore3) != 0)
            {
                index = NextOf(b->nodes[NextOf(node)]);
            }
            *ceOut = TempCe(index, sSecondary);
            return true;
        }
    }
    *ceOut = RootElementsFirstSecondary(&b->elements);
    return false;
}

// The root element and strength of a special position other than the
// ignorables'; false with a message for one not supported.
static bool SpecialElement(CollationBuilder* b, int32_t pos, uint64_t* ceOut, int32_t* strengthOut,
                           bool* boundaryOut)
{
    const RootElements* e = &b->elements;
    *strengthOut = sPrimary;
    *boundaryOut = false;
    switch (pos)
    {
    case rulePositionLastSecondaryIgnorable - 1:
        *ceOut = RootElementsLastTertiary(e);
        *strengthOut = sTertiary;
        return true;
    case rulePositionLastPrimaryIgnorable - 1:
        *ceOut = RootElementsLastSecondary(e);
        *strengthOut = sSecondary;
        return true;
    case rulePositionFirstVariable - 1:
        *ceOut = (uint64_t)RootElementsFirstPrimary(e) << 32 | collationCommonSecondaryTertiary;
        *boundaryOut = true;
        return true;
    case rulePositionLastVariable - 1:
        *ceOut = RootElementsLastBefore(e, b->variableTop + 1);
        return true;
    case rulePositionFirstRegular - 1:
        *ceOut = RootElementsFirstAtLeast(e, b->variableTop + 1);
        *boundaryOut = true;
        return true;
    case rulePositionLastRegular - 1:
        *ceOut = RootElementsFirstAtLeast(e, b->hanFirstPrimary);
        return true;
    case rulePositionFirstImplicit - 1:
    {
        uint32_t han = 0x4E00;
        return CollationMappingsElements(&b->base, &han, 1, 0, ceOut, 0) == 1 ||
               Fail(b, "[first implicit]: U+4E00 has no single element");
    }
    case rulePositionLastImplicit - 1:
        return Fail(b, "reset to [last implicit] not supported");
    case rulePositionFirstTrailing - 1:
        *ceOut = (uint64_t)firstTrailingPrimary << 32 | collationCommonSecondaryTertiary;
        *boundaryOut = true;
        return true;
    default:
        return Fail(b, "LDML forbids tailoring to U+FFFF");
    }
}

// ICU's getSpecialResetPosition, pos counted from 0.
static uint64_t SpecialPosition(CollationBuilder* b, int32_t pos)
{
    uint64_t ce = 0;
    if (pos <= rulePositionLastTertiaryIgnorable - 1)
    {
        return 0;
    }
    if (pos == rulePositionFirstSecondaryIgnorable - 1)
    {
        return FirstSecondaryIgnorable(b);
    }
    int32_t strength = sSecondary;
    bool boundary = false;
    if (pos == rulePositionFirstPrimaryIgnorable - 1)
    {
        if (FirstPrimaryIgnorable(b, &ce))
        {
            return ce;
        }
    }
    else if (!SpecialElement(b, pos, &ce, &strength, &boundary))
    {
        return 0;
    }
    int32_t index = FindOrInsertNodeForRootCe(b, ce, strength);
    uint64_t node = b->nodes[index];
    if ((pos & 1) == 0)
    {
        if ((node & (hasBefore2 | hasBefore3)) == 0 && boundary)
        {
            index = NextOf(node);
            if (index != 0)
            {
                node = b->nodes[index];
                ce = TempCe(index, strength);
            }
            else
            {
                uint32_t p = (uint32_t)(ce >> 32);
                int32_t pIndex = RootElementsFindPrimary(&b->elements, p);
                p = RootElementsPrimaryAfter(&b->elements, p, pIndex, Compressible(b, p));
                ce = (uint64_t)p << 32 | collationCommonSecondaryTertiary;
                index = FindOrInsertNodeForRootCe(b, ce, sPrimary);
                node = b->nodes[index];
            }
        }
        if ((node & (hasBefore2 | hasBefore3)) != 0)
        {
            ce = TempCe(PastBefore(b, index), strength);
        }
        return ce;
    }
    for (;;)
    {
        int32_t next = NextOf(node);
        if (next == 0 || StrengthOf(b->nodes[next]) < strength)
        {
            break;
        }
        index = next;
        node = b->nodes[next];
    }
    return IsTailored(node) ? TempCe(index, strength) : ce;
}

int32_t CollationNfd(const uint32_t* text, uint32_t length, uint32_t* out)
{
    char utf8[collationNfdMax * 4];
    size_t n = 0;
    for (uint32_t i = 0; i < length; i++)
    {
        uint32_t c = text[i];
        if (n + 4 > sizeof(utf8))
        {
            return -1;
        }
        if (c < 0x80)
        {
            utf8[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            utf8[n++] = (char)(0xC0 | c >> 6);
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            utf8[n++] = (char)(0xE0 | c >> 12);
            utf8[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            utf8[n++] = (char)(0xF0 | c >> 18);
            utf8[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            utf8[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    char nfd[collationNfdMax * 4];
    size_t needed = 0;
    muniTextResult r =
        muniNormalize(utf8, n, muni_nfd, muni_convertReplace, nfd, sizeof(nfd), &needed);
    if (r.status != muni_success)
    {
        return -1;
    }
    int32_t count = 0;
    for (size_t i = 0; i < needed && count < collationNfdMax;)
    {
        unsigned char lead = (unsigned char)nfd[i];
        size_t extra = lead < 0x80 ? 0 : lead < 0xE0 ? 1 : lead < 0xF0 ? 2 : 3;
        uint32_t c = extra == 0 ? lead : lead & (0x3Fu >> extra);
        for (size_t k = 1; k <= extra; k++)
        {
            c = c << 6 | ((unsigned char)nfd[i + k] & 0x3F);
        }
        out[count++] = c;
        i += 1 + extra;
    }
    return count;
}

// &[before n] after the reset's node was found: the node to tailor
// after; *tempOut the strength of the temporary element.
static int32_t BeforePosition(CollationBuilder* b, int32_t index, int32_t strength,
                              int32_t* tempOut)
{
    uint64_t node = b->nodes[index];
    *tempOut = strength;
    if (StrengthOf(node) == strength && IsTailored(node))
    {
        return PreviousOf(node);
    }
    if (strength == sPrimary)
    {
        uint32_t p = Weight32(node);
        if (p == 0)
        {
            return Fail(b, "reset primary-before ignorable not possible");
        }
        if (p <= RootElementsFirstPrimary(&b->elements))
        {
            return Fail(b, "reset primary-before first non-ignorable not supported");
        }
        if (p == firstTrailingPrimary)
        {
            return Fail(b, "reset primary-before [first trailing] not supported");
        }
        p = RootElementsPrimaryBefore(&b->elements, p, Compressible(b, p));
        index = FindOrInsertNodeForPrimary(b, p);
        while (!b->failed && NextOf(b->nodes[index]) != 0)
        {
            index = NextOf(b->nodes[index]);
        }
        return index;
    }
    // The strength of the temporary element is its position's.
    *tempOut = CeStrength(b->ces[b->cesLength - 1]);
    index = FindCommonNode(b, index, sSecondary);
    if (strength >= sTertiary)
    {
        index = FindCommonNode(b, index, sTertiary);
    }
    node = b->nodes[index];
    if (StrengthOf(node) != strength)
    {
        uint32_t weight16 = GetWeight16Before(b, index, node, strength);
        return FindOrInsertWeakNode(b, index, weight16, strength);
    }
    if (Weight16(node) == 0)
    {
        return Fail(b, strength == sSecondary
                           ? "reset secondary-before secondary ignorable not possible"
                           : "reset tertiary-before completely ignorable not possible");
    }
    uint32_t weight16 = GetWeight16Before(b, index, node, strength);
    int32_t previousIndex = PreviousOf(node);
    uint32_t previousWeight16 = 0;
    for (int32_t i = previousIndex;; i = PreviousOf(node))
    {
        node = b->nodes[i];
        int32_t previousStrength = StrengthOf(node);
        if (previousStrength < strength)
        {
            previousWeight16 = commonWeight16;
            break;
        }
        if (previousStrength == strength && !IsTailored(node))
        {
            previousWeight16 = Weight16(node);
            break;
        }
    }
    if (previousWeight16 == weight16)
    {
        return previousIndex;
    }
    return InsertNodeBetween(b, previousIndex, index,
                             NodeFromWeight16(weight16) | (uint64_t)strength);
}

static int AddReset(CollationBuilder* b, int32_t strength, uint8_t position, const uint32_t* text,
                    uint32_t length)
{
    if (position != 0)
    {
        b->ces[0] = SpecialPosition(b, position - 1);
        b->cesLength = 1;
    }
    else
    {
        uint32_t nfd[maxString];
        int32_t n = CollationNfd(text, length, nfd);
        if (n < 0)
        {
            return Fail(b, "a reset string too long");
        }
        b->cesLength =
            (int32_t)CollationMappingsElements(&b->mappings, nfd, (uint32_t)n, 0, b->ces, 0);
        if (b->cesLength > collationMaxExpansion)
        {
            return Fail(b, "reset position maps to too many collation elements (more than 31)");
        }
    }
    if (b->failed || strength == sIdentical)
    {
        return !b->failed;
    }
    int32_t index = FindOrInsertNodeForCes(b, strength);
    while (!b->failed && StrengthOf(b->nodes[index]) > strength)
    {
        index = PreviousOf(b->nodes[index]);
    }
    int32_t tempStrength = strength;
    if (!b->failed)
    {
        index = BeforePosition(b, index, strength, &tempStrength);
    }
    if (b->failed)
    {
        return Fail(b, "inserting reset position for &[before n]");
    }
    b->ces[b->cesLength - 1] = TempCe(index, tempStrength);
    return 1;
}

static bool IsJamoL(uint32_t c)
{
    return c >= 0x1100 && c <= 0x1112;
}

static bool IsJamoV(uint32_t c)
{
    return c >= 0x1161 && c <= 0x1175;
}

// ICU's setCaseBits: the case of the root's primaries of the string on
// the tailored primaries, the last taking the rest's (mixed when they
// differ); tertiary elements uppercase, others none.
static void SetCaseBits(CollationBuilder* b, const uint32_t* nfd, uint32_t length)
{
    int32_t tailoredPrimaries = 0;
    for (int32_t i = 0; i < b->cesLength; i++)
    {
        tailoredPrimaries += CeStrength(b->ces[i]) == sPrimary;
    }
    uint64_t cases = 0;
    if (tailoredPrimaries > 0)
    {
        uint64_t base[collationMaxExpansion];
        uint32_t count = CollationMappingsElements(&b->base, nfd, length, 0, base, 0);
        count = count < collationMaxExpansion ? count : collationMaxExpansion;
        uint32_t lastCase = 0;
        int32_t basePrimaries = 0;
        for (uint32_t i = 0; i < count; i++)
        {
            if (base[i] >> 32 == 0)
            {
                continue;
            }
            uint32_t c = ((uint32_t)base[i] >> 14) & 3;
            basePrimaries++;
            if (basePrimaries < tailoredPrimaries)
            {
                cases |= (uint64_t)c << ((basePrimaries - 1) * 2);
            }
            else if (basePrimaries == tailoredPrimaries)
            {
                lastCase = c;
            }
            else if (c != lastCase)
            {
                lastCase = 1;
                break;
            }
        }
        if (basePrimaries >= tailoredPrimaries)
        {
            cases |= (uint64_t)lastCase << ((tailoredPrimaries - 1) * 2);
        }
    }
    for (int32_t i = 0; i < b->cesLength; i++)
    {
        uint64_t ce = b->ces[i] & 0xFFFFFFFFFFFF3FFFULL;
        int32_t strength = CeStrength(ce);
        if (strength == sPrimary)
        {
            ce |= (cases & 3) << 14;
            cases >>= 2;
        }
        else if (strength == sTertiary)
        {
            ce |= 0x8000;
        }
        b->ces[i] = ce;
    }
}

// ICU's addIfDifferent: the mapping, unless the string already has these
// elements after its prefix.
static int AddIfDifferent(CollationBuilder* b, const uint32_t* prefix, uint32_t prefixLength,
                          const uint32_t* text, uint32_t length)
{
    uint32_t joined[2 * maxString];
    memcpy(joined, prefix, prefixLength * sizeof *prefix);
    memcpy(joined + prefixLength, text, length * sizeof *text);
    uint64_t old[collationMaxExpansion];
    uint32_t count = CollationMappingsElements(&b->mappings, joined, prefixLength + length,
                                               prefixLength, old, 0);
    if (count == (uint32_t)b->cesLength &&
        memcmp(old, b->ces, (size_t)b->cesLength * sizeof *old) == 0)
    {
        return 1;
    }
    return CollationMappingsAdd(&b->mappings, prefix, prefixLength, text, length, b->ces,
                                (uint32_t)b->cesLength)
               ? 1
               : Fail(b, "writing collation elements");
}

// The relation's node inserted after the position and the temporary
// element on the position's last element.
static int TailorAfter(CollationBuilder* b, int32_t strength)
{
    int32_t index = FindOrInsertNodeForCes(b, strength);
    if (b->failed)
    {
        return 0;
    }
    uint64_t ce = b->ces[b->cesLength - 1];
    if (strength == sPrimary && !IsTemp(ce) && (uint32_t)(ce >> 32) == 0)
    {
        return Fail(b, "tailoring primary after ignorables not supported");
    }
    if (strength == sQuaternary && ce == 0)
    {
        return Fail(b, "tailoring quaternary after tertiary ignorables not supported");
    }
    index = InsertTailoredNodeAfter(b, index, strength);
    if (b->failed)
    {
        return Fail(b, "modifying collation elements");
    }
    int32_t tempStrength = CeStrength(ce);
    tempStrength = strength < tempStrength ? strength : tempStrength;
    b->ces[b->cesLength - 1] = TempCe(index, tempStrength);
    return 1;
}

static int AddRelation(CollationBuilder* b, int32_t strength, const uint32_t* prefix,
                       uint32_t prefixLength, const uint32_t* text, uint32_t length,
                       const uint32_t* extension, uint32_t extensionLength)
{
    uint32_t nfdPrefix[maxString];
    uint32_t nfd[maxString];
    int32_t p = prefixLength == 0 ? 0 : CollationNfd(prefix, prefixLength, nfdPrefix);
    int32_t n = CollationNfd(text, length, nfd);
    if (p < 0 || n <= 0)
    {
        return Fail(b, "a relation string too long");
    }
    if (n >= 2 && (IsJamoL(nfd[0]) || IsJamoV(nfd[0])))
    {
        return Fail(b, "contractions starting with conjoining Jamo L or V not supported");
    }
    if (n >= 2 && (IsJamoL(nfd[n - 1]) || (IsJamoV(nfd[n - 1]) && IsJamoL(nfd[n - 2]))))
    {
        return Fail(b, "contractions ending with conjoining Jamo L or L+V not supported");
    }
    if (strength != sIdentical && !TailorAfter(b, strength))
    {
        return 0;
    }
    SetCaseBits(b, nfd, (uint32_t)n);
    int32_t before = b->cesLength;
    if (extensionLength != 0)
    {
        uint32_t nfdExtension[maxString];
        int32_t e = CollationNfd(extension, extensionLength, nfdExtension);
        if (e < 0)
        {
            return Fail(b, "an extension string too long");
        }
        b->cesLength = (int32_t)CollationMappingsElements(&b->mappings, nfdExtension, (uint32_t)e,
                                                          0, b->ces, (uint32_t)b->cesLength);
        if (b->cesLength > collationMaxExpansion)
        {
            return Fail(b,
                        "extension string adds too many collation elements (more than 31 total)");
        }
    }
    int ok = AddIfDifferent(b, nfdPrefix, (uint32_t)p, nfd, (uint32_t)n);
    b->cesLength = before;
    return ok;
}

static int32_t Strength(uint8_t ruleStrength)
{
    return ruleStrength == ruleIdentical ? sIdentical : (int32_t)ruleStrength - 1;
}

// Applies the suppressed ranges that stand before rule i, from *next.
static void Suppress(CollationBuilder* b, const CollationRules* rules, size_t i, size_t* next)
{
    for (; !b->failed && *next < rules->suppressCount && rules->suppressAt[*next] <= i; (*next)++)
    {
        for (uint32_t c = rules->suppress[*next * 2]; c <= rules->suppress[*next * 2 + 1]; c++)
        {
            if (!CollationMappingsSuppress(&b->mappings, c))
            {
                Fail(b, "out of memory");
                return;
            }
        }
    }
}

// The rules' settings over those gathered.
static void Gather(CollationRuleSettings* s, const CollationRules* r)
{
    s->strength = r->strength != ruleUnset ? r->strength : s->strength;
    s->alternate = r->alternate != ruleUnset ? r->alternate : s->alternate;
    s->maxVariable = r->maxVariable != ruleUnset ? r->maxVariable : s->maxVariable;
    s->caseFirst = r->caseFirst != ruleUnset ? r->caseFirst : s->caseFirst;
    s->caseLevel = r->caseLevel != ruleUnset ? r->caseLevel : s->caseLevel;
    s->numeric = r->numeric != ruleUnset ? r->numeric : s->numeric;
    s->backwards = r->backwards != ruleUnset ? r->backwards : s->backwards;
    if (r->reorderGiven)
    {
        s->reorderGiven = true;
        s->reorder = r->reorder;
        s->reorderCount = r->reorderCount;
    }
}

int CollationBuilderAdd(CollationBuilder* b, const CollationRules* rules)
{
    size_t suppressed = 0;
    for (size_t i = 0; !b->failed && i < rules->count; i++)
    {
        Suppress(b, rules, i, &suppressed);
        if (b->failed)
        {
            break;
        }
        const CollationRule* r = &rules->rules[i];
        const uint32_t* points = rules->points;
        if (r->kind == ruleImport)
        {
            const char* tag = rules->imports + r->text;
            const CollationRules* imported =
                b->importer != nullptr && b->importDepth < maxImportDepth
                    ? b->importer(b->importContext, tag)
                    : nullptr;
            if (imported == nullptr)
            {
                return Fail(b, "[import %s] not found", tag);
            }
            b->importDepth++;
            CollationBuilderAdd(b, imported);
            b->importDepth--;
        }
        else if (r->kind == ruleReset)
        {
            AddReset(b, Strength(r->strength), r->position, points + r->text, r->textLength);
        }
        else
        {
            AddRelation(b, Strength(r->strength), points + r->prefix, r->prefixLength,
                        points + r->text, r->textLength, points + r->extension, r->extensionLength);
        }
    }
    Suppress(b, rules, rules->count, &suppressed);
    Gather(&b->settings, rules);
    return !b->failed;
}

// ICU's countTailoredNodes: the tailored nodes of the strength from i up
// to a stronger node or one of the strength with its own weight.
static int32_t CountTailoredNodes(const uint64_t* nodes, int32_t i, int32_t strength)
{
    int32_t count = 0;
    while (i != 0)
    {
        uint64_t node = nodes[i];
        if (StrengthOf(node) < strength)
        {
            break;
        }
        if (StrengthOf(node) == strength)
        {
            if (!IsTailored(node))
            {
                break;
            }
            count++;
        }
        i = NextOf(node);
    }
    return count;
}

// The weights of one root primary's list (makeTailoredCEs' loop body).
typedef struct Walk
{
    uint32_t p;
    uint32_t s;
    uint32_t t;
    uint32_t q;
    int32_t pIndex;
    bool pTailored;
    bool sTailored;
    bool tTailored;
    CollationWeights primaries;
    CollationWeights secondaries;
    CollationWeights tertiaries;
} Walk;

static int TertiaryNode(CollationBuilder* b, Walk* w, uint64_t node, int32_t next)
{
    if (!IsTailored(node))
    {
        w->t = Weight16(node);
        w->tTailored = false;
        return 1;
    }
    if (!w->tTailored)
    {
        int32_t count = CountTailoredNodes(b->nodes, next, sTertiary) + 1;
        uint32_t limit = 0;
        if (w->t == 0)
        {
            w->t = RootElementsTertiaryBoundary(&b->elements) - 0x100;
            limit = RootElementsFirstTertiary(&b->elements) & 0x3F3F;
        }
        else if (!w->pTailored && !w->sTailored)
        {
            limit = RootElementsTertiaryAfter(&b->elements, w->pIndex, w->s, w->t);
        }
        else if (w->t == beforeWeight16)
        {
            limit = commonWeight16;
        }
        else
        {
            limit = RootElementsTertiaryBoundary(&b->elements);
        }
        CollationWeightsForTertiary(&w->tertiaries);
        if (!CollationWeightsAllocate(&w->tertiaries, w->t, limit, count))
        {
            return Fail(b, "tertiary tailoring gap too small");
        }
        w->tTailored = true;
    }
    w->t = CollationWeightsNext(&w->tertiaries);
    return 1;
}

static int SecondaryNode(CollationBuilder* b, Walk* w, uint64_t node, int32_t next)
{
    if (!IsTailored(node))
    {
        w->s = Weight16(node);
        w->sTailored = false;
        return 1;
    }
    if (!w->sTailored)
    {
        int32_t count = CountTailoredNodes(b->nodes, next, sSecondary) + 1;
        uint32_t limit = 0;
        if (w->s == 0)
        {
            w->s = RootElementsSecondaryBoundary(&b->elements) - 0x100;
            limit = RootElementsFirstSecondary(&b->elements) >> 16;
        }
        else if (!w->pTailored)
        {
            limit = RootElementsSecondaryAfter(&b->elements, w->pIndex, w->s);
        }
        else if (w->s == beforeWeight16)
        {
            limit = commonWeight16;
        }
        else
        {
            limit = RootElementsSecondaryBoundary(&b->elements);
        }
        if (w->s == commonWeight16)
        {
            w->s = RootElementsLastCommonSecondary(&b->elements);
        }
        CollationWeightsForSecondary(&w->secondaries);
        if (!CollationWeightsAllocate(&w->secondaries, w->s, limit, count))
        {
            return Fail(b, "secondary tailoring gap too small");
        }
        w->sTailored = true;
    }
    w->s = CollationWeightsNext(&w->secondaries);
    return 1;
}

static int PrimaryNode(CollationBuilder* b, Walk* w, int32_t next)
{
    if (!w->pTailored)
    {
        int32_t count = CountTailoredNodes(b->nodes, next, sPrimary) + 1;
        bool compressible = Compressible(b, w->p);
        uint32_t limit = RootElementsPrimaryAfter(&b->elements, w->p, w->pIndex, compressible);
        CollationWeightsForPrimary(&w->primaries, compressible);
        if (!CollationWeightsAllocate(&w->primaries, w->p, limit, count))
        {
            return Fail(b, "primary tailoring gap too small");
        }
        w->pTailored = true;
    }
    w->p = CollationWeightsNext(&w->primaries);
    w->s = commonWeight16;
    w->sTailored = false;
    return 1;
}

// One node of the list: its weights, the weaker levels' reset.
static int Step(CollationBuilder* b, Walk* w, uint64_t node, int32_t next)
{
    int32_t strength = StrengthOf(node);
    if (strength == sQuaternary)
    {
        if (w->q == 3)
        {
            return Fail(b, "quaternary tailoring gap too small");
        }
        w->q++;
        return 1;
    }
    if (strength == sTertiary)
    {
        if (!TertiaryNode(b, w, node, next))
        {
            return 0;
        }
    }
    else
    {
        if (!(strength == sSecondary ? SecondaryNode(b, w, node, next) : PrimaryNode(b, w, next)))
        {
            return 0;
        }
        w->t = w->s == 0 ? 0 : commonWeight16;
        w->tTailored = false;
    }
    w->q = 0;
    return 1;
}

static int TailorList(CollationBuilder* b, int32_t i)
{
    uint64_t node = b->nodes[i];
    Walk w = {.p = Weight32(node)};
    w.s = w.p == 0 ? 0 : commonWeight16;
    w.t = w.s;
    w.pIndex = w.p == 0 ? 0 : RootElementsFindPrimary(&b->elements, w.p);
    int32_t next = NextOf(node);
    while (next != 0)
    {
        i = next;
        node = b->nodes[i];
        next = NextOf(node);
        if (!Step(b, &w, node, next))
        {
            return 0;
        }
        if (IsTailored(node))
        {
            b->nodes[i] = MakeCe(w.p, w.s, w.t, w.q);
        }
    }
    return 1;
}

// Over the unihan root, the root's mappings that name a Han primary,
// which the runtime's root gives in code-point order, copied into the
// tailoring where it has none of the code point.
static int CopyHanReferences(CollationBuilder* b)
{
    const RootTable* root = b->root;
    for (size_t i = 0; root->radicalOrder && i < root->count; i++)
    {
        uint32_t c = root->mappings[i].text[0];
        // A code point NFD decomposes is never read (CJK compatibility
        // ideographs).
        if (root->mappings[i].hanReference && !CollationLeftOut(c) &&
            !CollationMappingsCopyRoot(&b->mappings, c))
        {
            return Fail(b, "out of memory");
        }
    }
    return 1;
}

int CollationBuilderFinish(CollationBuilder* b)
{
    if (b->failed)
    {
        return 0;
    }
    if (b->mappings.count == 0)
    {
        return CopyHanReferences(b);
    }
    for (size_t r = 0; r < b->rootPrimaryCount; r++)
    {
        if (!TailorList(b, b->rootPrimaryIndexes[r]))
        {
            return 0;
        }
    }
    // finalizeCEs: each temporary element becomes its node's, its case
    // bits kept.
    CollationMappings* m = &b->mappings;
    for (size_t i = 0; i < m->count; i++)
    {
        uint64_t* e = m->elements + m->mappings[i].elementStart;
        for (uint32_t k = 0; k < m->mappings[i].elementCount; k++)
        {
            if (IsTemp(e[k]))
            {
                e[k] = b->nodes[IndexOfTemp(e[k])] | (e[k] & 0xC000);
            }
        }
    }
    return CopyHanReferences(b);
}
