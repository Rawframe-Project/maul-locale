// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Display names (maul-locale/names.h, record mloc-0037): a code put in
// its canonical case behind its namespace, looked up along the locale's
// chain (names_data.h), else written itself, then titlecased as the
// context asks.

#include "maul-locale/names.h"

#include "data_view.h"
#include "kinds.h"
#include "locale_context.h"
#include "names_data.h"
#include "spellout_data.h"

#include <string.h>

enum
{
    // The longest code read: a language id's canonical text.
    maxCode = 160,
};

static bool Alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool Digit(char c)
{
    return c >= '0' && c <= '9';
}

static char Lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
}

static char Upper(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

// Whether text is all letters and digits, its length in a range.
static bool Alnum(const char* text, size_t length, size_t least, size_t most)
{
    for (size_t i = 0; i < length; i++)
    {
        if (!Alpha(text[i]) && !Digit(text[i]))
        {
            return false;
        }
    }
    return length >= least && length <= most;
}

// A key and value's syntax: a key of two letters or digits, then
// subtags of 3 to 8 joined by '-'.
static bool KeyValue(const char* text, size_t length)
{
    if (length < 6 || !Alnum(text, 2, 2, 2) || text[2] != '-')
    {
        return false;
    }
    for (size_t at = 3; at <= length;)
    {
        size_t end = at;
        while (end < length && text[end] != '-')
        {
            end++;
        }
        if (!Alnum(text + at, end - at, 3, 8))
        {
            return false;
        }
        at = end + 1;
    }
    return true;
}

// A language id in canonical syntax ("en-GB"), with CLDR's aliases
// applied when asked ("tl" is "fil"), and with no extension; false when
// it is not one or the aliases cannot be applied.
static bool Language(const mlocDataSet* data, const char* code, size_t length, bool aliases,
                     char* out, size_t* lengthOut)
{
    char text[maxCode];
    mlocLocale locale;
    if (length >= sizeof(text))
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        text[i] = code[i] == '_' ? '-' : code[i];
    }
    mlocTextResult r = aliases ? mlocCanonicalizeLocale(data, text, length, &locale, nullptr)
                               : mlocParseLocale(text, length, &locale);
    if (r.status != mloc_success)
    {
        return false;
    }
    // An extension starts with a singleton subtag.
    for (const char* at = locale.tail; *at != '\0';)
    {
        size_t subtag = strcspn(at, "-");
        if (subtag == 1)
        {
            return false;
        }
        at += subtag + (at[subtag] == '-');
    }
    mlocOutput output = {.buffer = out, .capacity = maxCode};
    if (mlocFormatLocale(&locale, &output) != mloc_success)
    {
        return false;
    }
    *lengthOut = output.length;
    return true;
}

// Writes a code in its canonical case behind its namespace ("l:en-GB",
// "t:ca-gregory"); *fallbackOut is where the text a missing name gives
// starts. False for a code not of the type's syntax.
static bool Canonical(const mlocDataSet* data, mlocNameType type, const char* code, size_t length,
                      char* out, size_t* lengthOut, size_t* fallbackOut)
{
    static const char s_prefixes[] = "lsrvktm";
    out[0] = s_prefixes[type];
    out[1] = ':';
    *fallbackOut = 2;
    if (type == mloc_nameLanguage)
    {
        size_t written = 0;
        bool valid = Language(data, code, length, false, out + 2, &written);
        *lengthOut = 2 + written;
        return valid;
    }
    bool valid = false;
    switch (type)
    {
    case mloc_nameScript:
        valid = length == 4 && Alpha(code[0]) && Alpha(code[1]) && Alpha(code[2]) && Alpha(code[3]);
        break;
    case mloc_nameRegion:
        valid = length == 2 ? Alpha(code[0]) && Alpha(code[1])
                            : length == 3 && Digit(code[0]) && Digit(code[1]) && Digit(code[2]);
        break;
    case mloc_nameVariant:
        valid =
            Alnum(code, length, 5, 8) || (length == 4 && Digit(code[0]) && Alnum(code, 4, 4, 4));
        break;
    case mloc_nameKey:
        valid = Alnum(code, length, 1, 2);
        break;
    case mloc_nameKeyValue:
        valid = KeyValue(code, length);
        *fallbackOut = 5;
        break;
    default:
        valid = Alnum(code, length, 3, 8);
        break;
    }
    if (!valid || length > maxCode)
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        bool upper = type == mloc_nameRegion || (type == mloc_nameScript && i == 0);
        out[2 + i] = upper ? Upper(code[i]) : Lower(code[i]);
    }
    *lengthOut = 2 + length;
    return true;
}

// Whether the first letter is titlecased in a context, as ICU decides: at
// the beginning of a sentence always, in a list or menu or alone as the
// locale's transforms for the type's usage say.
static bool Capitalizes(const mlocDataSet* data, const mlocLocale* locale, mlocNameType type,
                        mlocCapitalization capitalization)
{
    static const ContextUsage s_usages[] = {contextLanguages, contextScript, contextTerritory,
                                            contextVariant,   contextKey,    contextKeyValue};
    if (capitalization == mloc_capitalizationSentence)
    {
        return true;
    }
    if (type == mloc_nameMeasurementSystem || capitalization == mloc_capitalizationNone)
    {
        return false;
    }
    uint8_t contexts = mlocGetLocaleContext(data, locale, s_usages[type]);
    return (capitalization == mloc_capitalizationListOrMenu &&
            (contexts & contextListOrMenu) != 0) ||
           (capitalization == mloc_capitalizationStandAlone && (contexts & contextStandAlone) != 0);
}

// The first code point of valid UTF-8 text and its length in bytes.
static uint32_t First(const uint8_t* text, size_t length, size_t* sizeOut)
{
    uint32_t size = text[0] < 0x80 ? 1 : text[0] < 0xE0 ? 2 : text[0] < 0xF0 ? 3 : 4;
    if (length < size)
    {
        *sizeOut = 0;
        return 0;
    }
    static const uint8_t s_masks[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t cp = text[0] & s_masks[size];
    for (uint32_t i = 1; i < size; i++)
    {
        cp = cp << 6 | (text[i] & 0x3Fu);
    }
    *sizeOut = size;
    return cp;
}

// The titlecase of a text's first code point, or false when it has none
// other: ASCII by itself ("i" as Turkish and Azerbaijani titlecase it),
// the rest by the names' titlecase table.
static bool Titlecase(const mlocDataSet* data, const mlocLocale* locale, uint32_t cp,
                      const uint8_t** textOut, uint32_t* lengthOut)
{
    static const uint8_t s_dottedI[] = {0xC4, 0xB0};
    static const char s_upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    bool turkic = strcmp(locale->language, "tr") == 0 || strcmp(locale->language, "az") == 0;
    if (turkic && cp == 'i')
    {
        *textOut = s_dottedI;
        *lengthOut = sizeof(s_dottedI);
        return true;
    }
    if (cp < 0x80)
    {
        if (cp < 'a' || cp > 'z')
        {
            return false;
        }
        *textOut = (const uint8_t*)s_upper + (cp - 'a');
        *lengthOut = 1;
        return true;
    }
    DataSpan unit;
    return mlocFindDataSetUnit(data, kindNameTitlecase, nullptr, 0, &unit) &&
           mlocSpelloutTitlecase(unit, cp, textOut, lengthOut);
}

// Writes text, its first code point titlecased when asked.
static mlocResult Write(const mlocDataSet* data, const mlocLocale* locale, const uint8_t* text,
                        size_t length, bool capitalize, mlocOutput* output)
{
    const uint8_t* title = nullptr;
    uint32_t titleLength = 0;
    size_t firstSize = 0;
    if (capitalize && length > 0)
    {
        uint32_t cp = First(text, length, &firstSize);
        if (firstSize == 0 || !Titlecase(data, locale, cp, &title, &titleLength))
        {
            firstSize = 0;
            titleLength = 0;
        }
    }
    size_t needed = length - firstSize + titleLength;
    output->length = needed;
    if (needed > output->capacity)
    {
        return mloc_errorCapacity;
    }
    // Nothing to write fits a null buffer.
    if (output->buffer == nullptr)
    {
        return mloc_success;
    }
    if (titleLength > 0)
    {
        memcpy(output->buffer, title, titleLength);
    }
    if (length > firstSize)
    {
        memcpy(output->buffer + titleLength, text + firstSize, length - firstSize);
    }
    return mloc_success;
}

// A language id's name by its CLDR alias, as ICU4C looks a deprecated
// code up when the chain has no name of its own for it ("tl" as "fil",
// "tw" as "ak"); false when the alias changes nothing or has no name.
static bool Replaced(const mlocDataSet* data, const mlocLocale* locale, const char* code,
                     size_t length, mlocNameType type, NameField field, NameText* out)
{
    char plain[2 + maxCode] = "l:";
    char replaced[2 + maxCode] = "l:";
    size_t plainLength = 0;
    size_t replacedLength = 0;
    return type == mloc_nameLanguage &&
           Language(data, code, length, false, plain + 2, &plainLength) &&
           Language(data, code, length, true, replaced + 2, &replacedLength) &&
           (plainLength != replacedLength || memcmp(plain, replaced, plainLength + 2) != 0) &&
           mlocFindName(data, locale, replaced, replacedLength + 2, field, out);
}

mlocResult mlocGetName(const mlocDataSet* data, const mlocLocale* locale, mlocNameType type,
                       const char* code, size_t length, const mlocNameOptions* options,
                       mlocOutput* output)
{
    mlocNameOptions o = options != nullptr ? *options : (mlocNameOptions){0};
    if (data == nullptr || locale == nullptr || (code == nullptr && length != 0) ||
        output == nullptr || output->part != nullptr ||
        (output->buffer == nullptr && output->capacity != 0) ||
        (uint32_t)type > mloc_nameMeasurementSystem || (uint32_t)o.form >= nameFieldCount ||
        (uint32_t)o.fallback > mloc_nameFallbackNone ||
        (uint32_t)o.capitalization > mloc_capitalizationStandAlone)
    {
        return mloc_errorInvalid;
    }
    char canonical[2 + maxCode];
    size_t canonicalLength = 0;
    size_t fallback = 0;
    if (!Canonical(data, type, code, length, canonical, &canonicalLength, &fallback))
    {
        return mloc_errorSyntax;
    }
    if (!mlocDataSetHasKind(data, kindNameIds))
    {
        return mloc_errorNotFound;
    }
    NameText name;
    bool capitalize = Capitalizes(data, locale, type, o.capitalization);
    if (mlocFindName(data, locale, canonical, canonicalLength, (NameField)o.form, &name) ||
        Replaced(data, locale, code, length, type, (NameField)o.form, &name))
    {
        return Write(data, locale, name.text, name.length, capitalize, output);
    }
    if (o.fallback == mloc_nameFallbackNone)
    {
        return mloc_errorNotFound;
    }
    mlocResult r = Write(data, locale, (const uint8_t*)canonical + fallback,
                         canonicalLength - fallback, capitalize, output);
    return r == mloc_success ? mloc_fellBack : r;
}
