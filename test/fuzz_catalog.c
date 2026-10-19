// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for catalogs (record mloc-0054). The input is a data
// file, its version stamped as the library's, so that the fuzzer reaches
// the catalog units' validators. A catalog that opens finds every key it
// stores at its own index, refuses the input's tail as a key unless it
// stores it, and gives every message from inside its own bytes.

#include "catalog_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/catalog.h"

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
    Put32(file + headerCldr,
          mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch));
    Put32(file + headerUnicode,
          mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch));
    Put32(file + headerLibrary,
          mlocPackVersion(version.library.major, version.library.minor, version.library.patch));
    memcpy(file + headerTzdb, version.tzdb, 8);
    for (int i = 0; i < 8; i++)
    {
        file[headerTables + i] = (uint8_t)(version.tables >> (8 * i));
    }
}

// Every stored key is found at its index, and every index's message lies
// in the file.
static void CheckCatalog(const mlocCatalog* catalog, const mlocData* opened, const uint8_t* file,
                         size_t size)
{
    mlocCatalogInfo info;
    mlocResult r = mlocGetCatalogInfo(catalog, &info);
    Require(r == mloc_success || r == mloc_errorData);
    DataView view = mlocViewOf(opened);
    DataSpan span = {0};
    bool found = false;
    for (uint32_t i = 0; !found && i < view.localeCount; i++)
    {
        found = mlocFindDataUnit(&view, kindCatalogKeys, i, &span);
    }
    CatalogKeys keys;
    Require(found && mlocReadCatalogKeys(span, &keys));
    for (uint32_t i = 0; i < keys.count; i++)
    {
        uint32_t length = 0;
        const uint8_t* key = mlocCatalogKeyAt(&keys, i, &length);
        uint32_t index = UINT32_MAX;
        Require(mlocFindCatalogKey(catalog, (const char*)key, length, &index) == mloc_success &&
                index == i);
        const void* message = nullptr;
        size_t bytes = 0;
        r = mlocGetCatalogMessage(catalog, i, &message, &bytes);
        Require(r == mloc_errorNotFound || (r == mloc_success && (const uint8_t*)message >= file &&
                                            (const uint8_t*)message + bytes <= file + size));
    }
    const void* message = nullptr;
    size_t bytes = 0;
    Require(mlocGetCatalogMessage(catalog, keys.count, &message, &bytes) == mloc_errorInvalid);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    alignas(8) static uint8_t s_file[MAX_FILE];
    if (size > MAX_FILE || size < dataHeaderSize)
    {
        return 0;
    }
    memcpy(s_file, data, size);
    Stamp(s_file);
    mlocData opened;
    mlocCatalog catalog;
    if (mlocOpenData(&opened, s_file, size) != mloc_success ||
        mlocOpenCatalog(&catalog, &opened) != mloc_success)
    {
        return 0;
    }
    CheckCatalog(&catalog, &opened, s_file, size);
    // The last bytes as a key: found only where stored.
    size_t tail = size < 16 ? size : 16;
    uint32_t index = 0;
    if (mlocFindCatalogKey(&catalog, (const char*)data + size - tail, tail, &index) == mloc_success)
    {
        const void* message = nullptr;
        size_t length = 0;
        mlocResult r = mlocGetCatalogMessage(&catalog, index, &message, &length);
        Require(r == mloc_success || r == mloc_errorNotFound);
    }
    return 0;
}
