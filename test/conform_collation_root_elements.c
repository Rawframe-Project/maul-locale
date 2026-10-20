// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root elements table the tailoring builder reads (record
// mloc-0071), built from FractionalUCA.txt, against ICU4C's. Han's
// section (its primaries and the units after each) is set aside: ICU4C
// orders Han by radical and stroke where the root here follows CLDR's
// code point order (record mloc-0070), so the Han characters with
// weights of their own sit elsewhere in the same range. Alone it writes
// the number of the other units and a hash of them (hash = hash *
// 1000003 + unit, 64-bit), then the number of Han's; with --expect it
// fails unless the first two are the given ones, which CI checks
// against ICU4C 79's table on the pinned release. --dump writes all the
// units, one in hex per line, as the oracle writes ICU's; --queries
// reads queries from stdin and writes each
// answer in hex, as the oracle does over ICU's table:
//   pb P C   the primary before P (C 1 when P's lead byte compresses)
//   pa P C   the primary after P
//   sb P S   the secondary before [P, S]
//   sa P S   the secondary after [P, S]
//   tb P S T and ta P S T, the tertiary before and after [P, S, T]
//   lb P     the last element before P
//   fa P     the first element at P or after it
//
// usage: conform_collation_root_elements <FractionalUCA.txt>
//            [--expect LENGTH HASH | --dump | --queries]

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_root.h"
#include "collation_root_elements.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int32_t Index(const RootElements* e, uint32_t p)
{
    return p == 0 ? 0 : RootElementsFindPrimary(e, p);
}

static int Query(const RootElements* e, const char* line)
{
    char op[3] = {0};
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    int n = sscanf(line, "%2s %" SCNx32 " %" SCNx32 " %" SCNx32, op, &a, &b, &c);
    if (n == 2 && strcmp(op, "lb") == 0)
    {
        printf("%016" PRIx64 "\n", RootElementsLastBefore(e, a));
    }
    else if (n == 2 && strcmp(op, "fa") == 0)
    {
        printf("%016" PRIx64 "\n", RootElementsFirstAtLeast(e, a));
    }
    else if (n == 3 && strcmp(op, "pb") == 0)
    {
        printf("%08" PRIx32 "\n", RootElementsPrimaryBefore(e, a, b != 0));
    }
    else if (n == 3 && strcmp(op, "pa") == 0)
    {
        printf("%08" PRIx32 "\n", RootElementsPrimaryAfter(e, a, Index(e, a), b != 0));
    }
    else if (n == 3 && strcmp(op, "sb") == 0)
    {
        printf("%04" PRIx32 "\n", RootElementsSecondaryBefore(e, a, b));
    }
    else if (n == 3 && strcmp(op, "sa") == 0)
    {
        printf("%04" PRIx32 "\n", RootElementsSecondaryAfter(e, Index(e, a), b));
    }
    else if (n == 4 && strcmp(op, "tb") == 0)
    {
        printf("%04" PRIx32 "\n", RootElementsTertiaryBefore(e, a, b, c));
    }
    else if (n == 4 && strcmp(op, "ta") == 0)
    {
        printf("%04" PRIx32 "\n", RootElementsTertiaryAfter(e, Index(e, a), b, c));
    }
    else
    {
        fprintf(stderr, "a query not understood: %s", line);
        return 0;
    }
    return 1;
}

static int Dump(const RootElements* e)
{
    for (int32_t i = 0; i < e->length; i++)
    {
        printf("%08" PRIx32 "\n", e->units[i]);
    }
    return 0;
}

static int Queries(const RootElements* e)
{
    char line[256];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        if (!Query(e, line))
        {
            return 1;
        }
    }
    return 0;
}

// The units outside Han's section, the indexes last, and their hash.
static int Fingerprint(const RootElements* e, const char* length, const char* hashText)
{
    uint64_t hash = 0;
    int32_t others = 0;
    bool han = false;
    for (int32_t i = rootElementsIndexCount; i < e->length; i++)
    {
        uint32_t unit = e->units[i];
        if ((unit & rootElementsDeltaFlag) == 0)
        {
            uint32_t p = unit & ~(uint32_t)rootElementsStepMask;
            han = e->firstHan != 0 && p >= e->firstHan && p <= e->lastHan;
        }
        if (!han)
        {
            hash = hash * 1000003U + unit;
            others++;
        }
    }
    for (int32_t i = 0; i < rootElementsIndexCount; i++)
    {
        hash = hash * 1000003U + e->units[i];
    }
    printf("units %" PRId32 " hash %016" PRIx64 " han %" PRId32 "\n", others, hash,
           e->length - rootElementsIndexCount - others);
    if (length == nullptr)
    {
        return 0;
    }
    char expected[64];
    snprintf(expected, sizeof(expected), "%" PRId32 " %016" PRIx64, others, hash);
    char given[64];
    snprintf(given, sizeof(given), "%s %s", length, hashText);
    if (strcmp(expected, given) != 0)
    {
        fprintf(stderr, "expected units %s\n", given);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr,
                "usage: %s <FractionalUCA.txt> [--expect LENGTH HASH | --dump | --queries]\n",
                argv[0]);
        return 2;
    }
    RootTable table = {0};
    RootElements e = {0};
    char error[256];
    if (!RootRead(argv[1], &table, error, sizeof(error)) ||
        !RootElementsBuild(&table, &e, error, sizeof(error)))
    {
        fprintf(stderr, "%s\n", error);
        RootFree(&table);
        return 1;
    }
    const char* mode = argc > 2 ? argv[2] : "";
    int status = strcmp(mode, "--dump") == 0                 ? Dump(&e)
                 : strcmp(mode, "--queries") == 0            ? Queries(&e)
                 : strcmp(mode, "--expect") == 0 && argc > 4 ? Fingerprint(&e, argv[3], argv[4])
                                                             : Fingerprint(&e, nullptr, nullptr);
    RootElementsFree(&e);
    RootFree(&table);
    return status;
}
