// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_list.h"

#include <string.h>

static_assert(sizeof(ListKey) == offsetof(ListKey, reserved) + 2, "a list key has no padding");
static_assert(offsetof(PreparedList, formatter) % 8 == 0, "a prepared list's formatter is aligned");

// ECMA-402's names, each keyed by the public enumeration's value, so that
// the index a name is found at is the value it gives.
static const char* const s_types[] = {
    [mloc_listAnd] = "conjunction",
    [mloc_listOr] = "disjunction",
    [mloc_listUnit] = "unit",
};
static const char* const s_styles[] = {
    [mloc_listWide] = "long",
    [mloc_listShort] = "short",
    [mloc_listNarrow] = "narrow",
};
static const char* const s_keywords[] = {
    [mloc_pluralZero] = "zero", [mloc_pluralOne] = "one",   [mloc_pluralTwo] = "two",
    [mloc_pluralFew] = "few",   [mloc_pluralMany] = "many", [mloc_pluralOther] = "other",
};

static_assert(sizeof(s_types) / sizeof(s_types[0]) == mloc_listUnit + 1,
              "a name for each mlocListType");
static_assert(sizeof(s_styles) / sizeof(s_styles[0]) == mloc_listNarrow + 1,
              "a name for each mlocListWidth");
static_assert(sizeof(s_keywords) / sizeof(s_keywords[0]) == mloc_pluralOther + 1,
              "a keyword for each mlocPluralCategory");

bool mlocGoodList(const mlocMessageValue* argument)
{
    const mlocListItem* items = argument->as.list.items;
    size_t count = argument->as.list.count;
    if ((items == NULL && count > 0) || count > UINT32_MAX)
    {
        return false;
    }
    for (size_t i = 0; i < count; i++)
    {
        if (items[i].text == NULL && items[i].length > 0)
        {
            return false;
        }
    }
    return true;
}

ListValue mlocMakeListValue(const mlocMessageValue* argument)
{
    ListValue v;
    memset(&v, 0, sizeof v);
    v.items = argument->as.list.items;
    v.count = argument->as.list.count;
    return v;
}

static bool NameIs(const uint8_t* name, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(name, text, length) == 0;
}

mlocMessageFaultKind mlocSetListOption(ListValue* value, const uint8_t* name, size_t length,
                                       const NumberOption* option)
{
    bool type = NameIs(name, length, "type");
    if (!type && !NameIs(name, length, "style"))
    {
        return mloc_messageFaultNone;
    }
    const char* const* names = type ? s_types : s_styles;
    for (int i = 0; option->hasText && option->literal && i < 3; i++)
    {
        if (NameIs(option->text, option->length, names[i]))
        {
            if (type)
            {
                value->key.type = (uint8_t)i;
            }
            else
            {
                value->key.width = (uint8_t)i;
            }
            value->key.given |= type ? listGivenType : listGivenStyle;
            return mloc_messageFaultNone;
        }
    }
    // A value it does not take, or a variable's: they choose the
    // formatter, made once at bind.
    return mloc_messageFaultBadOption;
}

static mlocResult MakeFormatter(const ListKey* key, const mlocDataSet* data,
                                const mlocLocale* locale, mlocListFormatter* out)
{
    mlocListOptions options = {(mlocListType)key->type, (mlocListWidth)key->width};
    return mlocInitListFormatter(data, locale, &options, out);
}

bool mlocPrepareList(const ListValue* value, const mlocDataSet* data, const mlocLocale* locale,
                     PreparedList* out)
{
    memset(out, 0, offsetof(PreparedList, formatter));
    out->key = value->key;
    out->rulesMade = mlocInitPluralRules(data, locale, mloc_pluralCardinal, &out->rules) >= 0;
    return MakeFormatter(&value->key, data, locale, &out->formatter) >= 0;
}

// Whether a prepared list was made with a value's options.
static bool Fits(const PreparedList* prepared, const ListValue* value)
{
    return prepared != NULL && memcmp(&prepared->key, &value->key, sizeof value->key) == 0;
}

mlocMessageFaultKind mlocWriteListValue(const ListValue* value, const PreparedList* prepared,
                                        const mlocDataSet* data, const mlocLocale* locale,
                                        mlocOutput* output)
{
    mlocListFormatter made;
    const mlocListFormatter* formatter = &made;
    mlocResult result = mloc_success;
    if (Fits(prepared, value))
    {
        formatter = &prepared->formatter;
    }
    else
    {
        result = MakeFormatter(&value->key, data, locale, &made);
    }
    if (result >= 0)
    {
        result = mlocListFormatter_Format(formatter, value->items, value->count, output);
    }
    // A text past the room reports the length it needs.
    return result == mloc_success || (result == mloc_errorCapacity && output->length > 0)
               ? mloc_messageFaultNone
               : mloc_messageFaultUnsupportedOperation;
}

// A plural keyword's category; false for another key.
static bool Keyword(const uint8_t* key, size_t length, mlocPluralCategory* categoryOut)
{
    for (int i = 0; i < (int)(sizeof(s_keywords) / sizeof(s_keywords[0])); i++)
    {
        if (NameIs(key, length, s_keywords[i]))
        {
            *categoryOut = (mlocPluralCategory)i;
            return true;
        }
    }
    return false;
}

mlocMessageFaultKind mlocMatchList(const ListValue* value, const PreparedList* prepared,
                                   const mlocDataSet* data, const mlocLocale* locale,
                                   const uint8_t* key, size_t length, bool* matchOut)
{
    *matchOut = false;
    mlocPluralCategory wanted;
    if (mlocNumberLiteral(key, length))
    {
        // The count as an integer is written: "2" matches, "2.0" does not.
        mlocDecimal count;
        *matchOut = mlocMakeDecimal((int64_t)value->count, &count) == mloc_success &&
                    mlocExactlyKey(&count, key, length);
        return mloc_messageFaultNone;
    }
    if (!Keyword(key, length, &wanted))
    {
        return mloc_messageFaultBadVariantKey;
    }
    mlocPluralRules rules;
    const mlocPluralRules* used = &rules;
    if (Fits(prepared, value) && prepared->rulesMade)
    {
        used = &prepared->rules;
    }
    else if (mlocInitPluralRules(data, locale, mloc_pluralCardinal, &rules) < 0)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    mlocPluralCategory category;
    if (mlocPluralRules_SelectInteger(used, (int64_t)value->count, &category) != mloc_success)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    *matchOut = category == wanted;
    return mloc_messageFaultNone;
}
