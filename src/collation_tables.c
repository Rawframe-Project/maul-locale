// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_tables.h"

bool mlocValidCollationRoot(const DataView* view, DataSpan payload)
{
    (void)view;
    CollationRoot root;
    return mlocReadCollationRoot(payload, &root);
}

// Whether a name is a co value's letters, digits and hyphens.
static bool NameValid(const uint8_t* name, uint32_t length)
{
    bool ok = length > 0 && length <= collationMaxTypeName;
    for (uint32_t i = 0; ok && i < length; i++)
    {
        uint8_t c = name[i];
        ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    }
    return ok;
}

// Whether name a sorts before name b: by bytes, a prefix first.
static bool Before(const uint8_t* a, uint32_t aLength, const uint8_t* b, uint32_t bLength)
{
    int c = memcmp(a, b, aLength < bLength ? aLength : bLength);
    return c < 0 || (c == 0 && aLength < bLength);
}

bool mlocValidCollationTypes(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < collationTypesHead ||
        mlocLoad32(payload.bytes) > (payload.size - collationTypesHead) / collationTypeSize ||
        mlocLoad32(payload.bytes + 8) != 0 || mlocLoad32(payload.bytes + 12) != 0)
    {
        return false;
    }
    CollationTypes t = mlocReadCollationTypes(payload);
    uint64_t namesAt = (uint64_t)(t.names - payload.bytes);
    // Names and tailorings are packed in order: each starts where the
    // one before ends.
    uint64_t name = t.defaultLength;
    bool ok = (t.count > 0 || t.defaultLength > 0) &&
              (t.defaultLength == 0 ||
               (namesAt + name <= payload.size && NameValid(t.defaultName, t.defaultLength)));
    const uint8_t* last = nullptr;
    uint32_t lastLength = 0;
    for (uint32_t i = 0; ok && i < t.count; i++)
    {
        const uint8_t* entry = payload.bytes + collationTypesHead + (size_t)i * collationTypeSize;
        uint32_t length = mlocLoad32(entry + 4);
        ok = mlocLoad32(entry) == name && namesAt + name + length <= payload.size &&
             NameValid(t.names + name, length) &&
             (last == nullptr || Before(last, lastLength, t.names + name, length));
        last = t.names + name;
        lastLength = length;
        name += length;
    }
    uint64_t at = (namesAt + name + 7) / 8 * 8;
    for (uint32_t i = 0; ok && i < t.count; i++)
    {
        const uint8_t* entry = payload.bytes + collationTypesHead + (size_t)i * collationTypeSize;
        uint32_t size = mlocLoad32(entry + 12);
        CollationTailoring tailoring;
        ok = mlocLoad32(entry + 8) == at && at + size <= payload.size &&
             mlocReadCollationTailoring((DataSpan){payload.bytes + at, size}, &tailoring);
        at += ((uint64_t)size + 7) / 8 * 8;
    }
    return ok && at == payload.size;
}
