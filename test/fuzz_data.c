// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the data file validator. The input is a data file
// whose magic, format and version are set to the library's, so that the
// fuzzer reaches the tables. A file that opens must answer every lookup
// from inside its own bytes.

#include "data_format.h"
#include "data_view.h"

#include <stdlib.h>
#include <string.h>

#define MAX_FILE 65536

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

// Even payload sizes only, so that a validator's refusal is reached.
static bool ValidEven(const DataView* view, DataSpan payload)
{
    Require(payload.bytes >= view->bytes &&
            payload.bytes + payload.size <= view->bytes + view->poolOffset);
    return payload.size % 2 == 0;
}

static const DataKind s_kinds[] = {{0, ValidEven}, {1, ValidEven}, {2, ValidEven}};

static void Put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
    {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}

static void Stamp(uint8_t* file)
{
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    memcpy(file, MLOC_DATA_MAGIC, 4);
    file[headerFormat] = dataFormatVersion;
    file[headerFormat + 1] = 0;
    file[headerHeaderSize] = dataHeaderSize;
    file[headerHeaderSize + 1] = 0;
    Put32(file + headerCldr,
          mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch));
    Put32(file + headerUnicode,
          mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch));
    Put32(file + headerDataFormat, version.format);
    memcpy(file + headerTzdb, version.tzdb, 8);
    for (int i = 0; i < 8; i++)
    {
        file[headerTables + i] = (uint8_t)(version.tables >> (8 * i));
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    alignas(8) static uint8_t s_file[MAX_FILE];
    if (size > MAX_FILE)
    {
        return 0;
    }
    memcpy(s_file, data, size);
    if (size >= dataHeaderSize)
    {
        Stamp(s_file);
    }
    DataView view;
    if (mlocOpenDataWithKinds(&view, s_file, size, s_kinds, 3) != mloc_success)
    {
        return 0;
    }
    mlocData opened = {0};
    memcpy(&opened, &view, sizeof(view));
    for (uint32_t i = 0; i < view.localeCount; i++)
    {
        const char* id = nullptr;
        size_t length = 0;
        Require(mlocGetDataLocale(&opened, i, &id, &length) == mloc_success);
        Require((const uint8_t*)id >= s_file + view.poolOffset &&
                (const uint8_t*)id + length <= s_file + size);
        uint32_t index = 0;
        Require(mlocFindDataLocale(&view, id, length, &index) && index == i);
        for (uint32_t kind = 0; kind < 3; kind++)
        {
            DataSpan span;
            if (mlocFindDataUnit(&view, kind, i, &span))
            {
                Require(span.bytes + span.size <= s_file + view.poolOffset);
            }
        }
    }
    return 0;
}
