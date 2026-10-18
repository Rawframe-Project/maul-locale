// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_list.h"

#include <string.h>

static_assert(sizeof(ListKey) == offsetof(ListKey, reserved) + 2, "a list key has no padding");
static_assert(offsetof(PreparedList, formatter) % 8 == 0, "a prepared list's formatter is aligned");

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
    // ECMA-402's names, in mlocListType's and mlocListWidth's orders.
    static const char* const types[] = {"conjunction", "disjunction", "unit"};
    static const char* const styles[] = {"long", "short", "narrow"};
    bool type = NameIs(name, length, "type");
    if (!type && !NameIs(name, length, "style"))
    {
        return mloc_messageFaultNone;
    }
    const char* const* names = type ? types : styles;
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
    return mlocInitListFormatter(out, data, locale, &options);
}

bool mlocPrepareList(const ListValue* value, const mlocDataSet* data, const mlocLocale* locale,
                     PreparedList* out)
{
    memset(out, 0, offsetof(PreparedList, formatter));
    out->key = value->key;
    out->rulesMade = mlocInitPluralRules(&out->rules, data, locale, mloc_pluralCardinal) >= 0;
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
        result = mlocFormatList(formatter, value->items, value->count, output);
    }
    // A text past the room reports the length it needs.
    return result == mloc_success || (result == mloc_errorCapacity && output->length > 0)
               ? mloc_messageFaultNone
               : mloc_messageFaultUnsupportedOperation;
}

// A plural keyword's category; false for another key.
static bool Keyword(const uint8_t* key, size_t length, mlocPluralCategory* categoryOut)
{
    static const char* const keywords[] = {"zero", "one", "two", "few", "many", "other"};
    for (int i = 0; i < 6; i++)
    {
        if (NameIs(key, length, keywords[i]))
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
    else if (mlocInitPluralRules(&rules, data, locale, mloc_pluralCardinal) < 0)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    mlocPluralCategory category;
    if (mlocSelectPluralInteger(used, (int64_t)value->count, &category) != mloc_success)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    *matchOut = category == wanted;
    return mloc_messageFaultNone;
}
