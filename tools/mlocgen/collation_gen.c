// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_gen.h"

#include "collation_root.h"
#include "collation_unit.h"
#include "kinds.h"

#include <stdio.h>
#include <stdlib.h>

int GenerateCollation(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/uca/FractionalUCA.txt", commonDirectory);
    RootTable table;
    if (!RootRead(path, &table, errorOut, errorCapacity))
    {
        return 0;
    }
    size_t size = 0;
    uint8_t* unit = CollationRootUnit(&table, &size, errorOut, errorCapacity);
    RootFree(&table);
    if (unit == nullptr)
    {
        return 0;
    }
    DataFileAdd(file, kindCollationRoot, nullptr, unit, size);
    free(unit);
    return 1;
}
