// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for locale and code names. The input's first byte
// picks the type, the second the options, the third the display locale
// (one of a few, or the input's own locale); the rest is a code and,
// after a NUL, a locale id. mlocGetName names the code and
// mlocGetLocaleName the locale canonicalized, each over the compiled-in
// data with the names and subdivisions data files. A call answers only
// as its documentation says, measures what it then writes in full, never
// writes past a short buffer, writes valid UTF-8, and writes the same
// text again.

#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static bool OpenFile(const char* path, uint8_t* bytes, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    size_t size = fread(bytes, 1, capacity, file);
    fclose(file);
    return size < capacity && mlocOpenData(bytes, size, out) == mloc_success;
}

// The data files are read once into buffers with room to spare (8 MiB
// for names.mloc's 4.3 MB); a file that outgrows its buffer stops the
// run at the first input.
static const mlocDataSet* Data(void)
{
    static mlocData s_sources[3];
    alignas(8) static uint8_t s_names[8 << 20];
    alignas(8) static uint8_t s_subdivisions[1 << 20];
    static mlocDataSet s_set;
    if (s_set.count == 0)
    {
        mlocOpenBuiltinData(&s_sources[0]);
        Require(OpenFile(MLOC_NAMES_FILE, s_names, sizeof(s_names), &s_sources[1]) &&
                OpenFile(MLOC_SUBDIVISIONS_FILE, s_subdivisions, sizeof(s_subdivisions),
                         &s_sources[2]));
        s_set = (mlocDataSet){s_sources, 3};
    }
    return &s_set;
}

static bool ValidUtf8(const uint8_t* text, size_t length)
{
    for (size_t at = 0; at < length;)
    {
        uint8_t b = text[at];
        size_t n = b < 0x80                ? 1
                   : b >= 0xC2 && b < 0xE0 ? 2
                   : b >= 0xE0 && b < 0xF0 ? 3
                   : b >= 0xF0 && b < 0xF5 ? 4
                                           : 0;
        if (n == 0 || at + n > length)
        {
            return false;
        }
        for (size_t k = 1; k < n; k++)
        {
            if ((text[at + k] & 0xC0) != 0x80)
            {
                return false;
            }
        }
        uint32_t cp = n == 1 ? b : (uint32_t)(b & (0xFF >> (n + 1)));
        for (size_t k = 1; k < n; k++)
        {
            cp = cp << 6 | (text[at + k] & 0x3Fu);
        }
        static const uint32_t s_least[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < s_least[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        {
            return false;
        }
        at += n;
    }
    return true;
}

typedef struct Call
{
    const mlocDataSet* data;
    const mlocLocale* display;
    // A code's name when locale is null, else the locale's.
    const mlocLocale* locale;
    mlocNameType type;
    const char* code;
    size_t length;
    mlocNameOptions name;
    mlocLocaleNameOptions localeName;
} Call;

static mlocResult Run(const Call* c, mlocOutput* output)
{
    return c->locale == nullptr
               ? mlocGetName(c->data, c->display, c->type, c->code, c->length, &c->name, output)
               : mlocGetLocaleName(c->data, c->display, c->locale, &c->localeName, output);
}

// Measures, writes in full and again, and writes into a buffer one byte
// short and into none, each with guard bytes after it.
static void Check(const Call* c, bool inRange)
{
    mlocOutput probe = {0};
    mlocResult measured = Run(c, &probe);
    if (!inRange)
    {
        Require(measured == mloc_errorInvalid);
        return;
    }
    Require(measured == mloc_errorCapacity || measured == mloc_errorSyntax ||
            measured == mloc_errorNotFound);
    if (measured != mloc_errorCapacity)
    {
        Require(c->locale == nullptr || measured == mloc_errorNotFound);
        return;
    }
    size_t length = probe.length;
    Require(length > 0 && length < 4096);
    static char s_text[4096 + 16];
    static char s_again[4096 + 16];
    memset(s_text, 0xA5, length + 16);
    mlocOutput out = {.buffer = s_text, .capacity = length};
    mlocResult written = Run(c, &out);
    Require((written == mloc_success || written == mloc_fellBack) && out.length == length);
    Require(ValidUtf8((const uint8_t*)s_text, length));
    for (size_t k = 0; k < 16; k++)
    {
        Require((uint8_t)s_text[length + k] == 0xA5);
    }
    mlocOutput again = {.buffer = s_again, .capacity = length};
    Require(Run(c, &again) == written && again.length == length &&
            memcmp(s_again, s_text, length) == 0);
    memset(s_again, 0xA5, length + 16);
    mlocOutput shortOut = {.buffer = s_again, .capacity = length - 1};
    Require(Run(c, &shortOut) == mloc_errorCapacity && shortOut.length == length);
    for (size_t k = length - 1; k < length + 16; k++)
    {
        Require((uint8_t)s_again[k] == 0xA5);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const char* const s_displays[] = {"en", "de",  "ja",      "ar",    "zh-Hant",
                                             "tr", "und", "sr-Latn", "fr-CA", "he"};
    if (size < 3)
    {
        return 0;
    }
    const mlocDataSet* set = Data();
    uint8_t type = data[0];
    uint8_t options = data[1];
    uint8_t which = data[2];
    const char* code = (const char*)data + 3;
    size_t rest = size - 3;
    const char* nul = memchr(code, 0, rest);
    size_t codeLength = nul != nullptr ? (size_t)(nul - code) : rest;
    const char* id = nul != nullptr ? nul + 1 : code;
    size_t idLength = nul != nullptr ? rest - codeLength - 1 : rest;

    mlocLocale locale;
    bool canonical =
        mlocCanonicalizeLocale(set, id, idLength, &locale, nullptr).status == mloc_success;
    mlocLocale display;
    if (which % 11 < 10 || !canonical)
    {
        const char* d = s_displays[which % 10];
        Require(mlocCanonicalizeLocale(set, d, strlen(d), &display, nullptr).status ==
                mloc_success);
    }
    else
    {
        display = locale;
    }

    // The low bits pick values in range; the high bit of each field one
    // past it.
    Call name = {.data = set,
                 .display = &display,
                 .type = (mlocNameType)(type & 0x0F),
                 .code = code,
                 .length = codeLength,
                 .name = {.form = (mlocNameForm)(options & 0x0F),
                          .fallback = (mlocNameFallback)(options >> 4 & 3),
                          .capitalization = (mlocCapitalization)(options >> 6 & 3)}};
    bool nameInRange = (type & 0x0F) <= mloc_nameSubdivision &&
                       (options & 0x0F) <= mloc_nameFormStandAlone &&
                       (options >> 4 & 3) <= mloc_nameFallbackNone;
    Check(&name, nameInRange);
    if (canonical)
    {
        Call named = {.data = set,
                      .display = &display,
                      .locale = &locale,
                      .localeName = {.languageDisplay = (mlocLanguageDisplay)(type >> 4 & 3),
                                     .form = (mlocNameForm)(options & 0x0F),
                                     .capitalization = (mlocCapitalization)(options >> 6 & 3)}};
        bool localeInRange = (type >> 4 & 3) <= mloc_languageDisplayStandard &&
                             (options & 0x0F) <= mloc_nameFormStandAlone;
        Check(&named, localeInRange);
    }
    return 0;
}
