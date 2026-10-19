// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Stale translations (stale.h).

#include "stale.h"

#include "catalog_hash.h"
#include "catalog_set.h"

#include "maul-locale/message.h"

#include <stdlib.h>

// Compiles the message and writes it back canonically; the caller frees
// the text.
static char* Canonical(const char* text, size_t length, size_t* lengthOut)
{
    size_t size = 0;
    mlocTextResult r = mlocCompileMessage(text, length, nullptr, 0, &size, nullptr);
    uint8_t* compiled = r.status == mloc_errorCapacity || r.status == mloc_success
                            ? malloc(size == 0 ? 1 : size)
                            : nullptr;
    char* canonical = nullptr;
    if (compiled != nullptr &&
        mlocCompileMessage(text, length, compiled, size, &size, nullptr).status == mloc_success)
    {
        mlocOutput out = {nullptr, 0, 0, nullptr, nullptr};
        (void)mlocWriteMessageText(compiled, size, &out);
        canonical = malloc(out.length == 0 ? 1 : out.length);
        out = (mlocOutput){canonical, out.length, 0, nullptr, nullptr};
        if (canonical != nullptr && mlocWriteMessageText(compiled, size, &out) == mloc_success)
        {
            *lengthOut = out.length;
        }
        else
        {
            free(canonical);
            canonical = nullptr;
        }
    }
    free(compiled);
    return canonical;
}

int SourceHash(const char* text, size_t length, uint64_t* out)
{
    size_t canonicalLength = 0;
    char* canonical = Canonical(text, length, &canonicalLength);
    if (canonical == nullptr)
    {
        return 0;
    }
    *out = mlocXxh64((const uint8_t*)canonical, canonicalLength, 0);
    free(canonical);
    return 1;
}

void SourceHashText(uint64_t hash, char out[sourceHashDigits + 1])
{
    static const char s_digits[] = "0123456789abcdef";
    for (int i = 0; i < sourceHashDigits; i++)
    {
        out[i] = s_digits[(hash >> (60 - 4 * i)) & 0xF];
    }
    out[sourceHashDigits] = '\0';
}

int SourceHashRead(const char* text, size_t length, uint64_t* out)
{
    if (length != sourceHashDigits)
    {
        return 0;
    }
    uint64_t hash = 0;
    for (size_t i = 0; i < length; i++)
    {
        char c = text[i];
        int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0)
        {
            return 0;
        }
        hash = hash << 4 | (uint64_t)digit;
    }
    *out = hash;
    return 1;
}

SourceState SourceStateOf(const ResMeta* meta, size_t count, const ResText* source)
{
    const ResMeta* m = CatalogFindMeta(meta, count, "mloc-source");
    uint64_t made = 0;
    uint64_t now = 0;
    if (m == nullptr)
    {
        return sourceUnrecorded;
    }
    return SourceHashRead(m->value.bytes, m->value.length, &made) &&
                   SourceHash(source->bytes, source->length, &now) && made != now
               ? sourceStale
               : sourceFresh;
}
