// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Messages as the library compiles them (message_text.h).

#include "message_text.h"

#include "maul-locale/message.h"

#include <stdint.h>
#include <stdlib.h>

// The compiled message, or null when it does not compile.
static uint8_t* Compile(const char* text, size_t length, size_t* sizeOut)
{
    size_t size = 0;
    mlocTextResult r = mlocCompileMessage(text, length, nullptr, 0, &size, nullptr);
    uint8_t* compiled = r.status == mloc_errorCapacity || r.status == mloc_success
                            ? malloc(size == 0 ? 1 : size)
                            : nullptr;
    if (compiled != nullptr &&
        mlocCompileMessage(text, length, compiled, size, &size, nullptr).status != mloc_success)
    {
        free(compiled);
        compiled = nullptr;
    }
    *sizeOut = size;
    return compiled;
}

int MessageCompiles(const char* text, size_t length)
{
    size_t size = 0;
    uint8_t* compiled = Compile(text, length, &size);
    free(compiled);
    return compiled != nullptr;
}

char* CanonicalMessage(const char* text, size_t length, size_t* lengthOut)
{
    size_t size = 0;
    uint8_t* compiled = Compile(text, length, &size);
    char* canonical = nullptr;
    if (compiled != nullptr)
    {
        mlocOutput out = {nullptr, 0, 0, nullptr, nullptr};
        (void)mlocWriteMessageText(compiled, size, &out);
        canonical = malloc(out.length + 1);
        out = (mlocOutput){canonical, out.length, 0, nullptr, nullptr};
        if (canonical != nullptr && mlocWriteMessageText(compiled, size, &out) == mloc_success)
        {
            canonical[out.length] = '\0';
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
