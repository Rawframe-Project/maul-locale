// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_words.h"

#include <string.h>

// Keeps a formatter made in a call in its own frame, not in every
// write's.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

bool mlocWordsValue(const NumberValue* value)
{
    return value->function == numberFunctionSpellout || value->function == numberFunctionRelative;
}

// Makes a mloc:spellout value's formatter: its numbering system's, or
// its rule set's ("%" and the name), MF1's "%spellout-numbering" by
// default.
static mlocResult MakeSpellout(const NumberValue* value, const mlocDataSet* data,
                               const mlocLocale* locale, mlocSpelloutFormatter* out)
{
    if ((value->given & numberGivenSystem) != 0)
    {
        return mlocInitNumberingSystemFormatter(data, locale, value->unit, value->unitLength, NULL,
                                                out);
    }
    static const char numbering[] = "spellout-numbering";
    const char* name = (value->given & numberGivenRules) != 0 ? value->unit : numbering;
    size_t length =
        (value->given & numberGivenRules) != 0 ? value->unitLength : sizeof numbering - 1;
    // A rule set's name is a unit identifier's letters, so 64 bytes hold
    // any the data has; a longer one it cannot have.
    char ruleSet[64];
    if (length + 1 > sizeof ruleSet)
    {
        return mloc_errorNotFound;
    }
    ruleSet[0] = '%';
    memcpy(ruleSet + 1, name, length);
    return mlocInitSpelloutFormatter(data, locale, ruleSet, length + 1, NULL, out);
}

// Makes a mloc:relative value's formatter.
static mlocResult MakeRelative(const NumberValue* value, const mlocDataSet* data,
                               const mlocLocale* locale, mlocRelativeFormatter* out)
{
    mlocRelativeOptions options;
    memset(&options, 0, sizeof options);
    options.number = mlocNumberFormatOptions(value);
    options.width = (value->given & numberGivenStyle) != 0 ? (mlocRelativeWidth)value->width
                                                           : mloc_relativeWidthLong;
    options.numeric = (value->given & numberGivenNumeric) != 0 ? (mlocRelativeNumeric)value->numeric
                                                               : mloc_relativeNumericAlways;
    return mlocInitRelativeFormatter(data, locale, &options, out);
}

bool mlocPrepareWords(const NumberValue* value, const mlocDataSet* data, const mlocLocale* locale,
                      PreparedNumber* out)
{
    memset(out, 0, offsetof(PreparedNumber, formatter));
    out->key = mlocNumberKey(value);
    if (value->function == numberFunctionSpellout)
    {
        out->kind = preparedSpellout;
        return MakeSpellout(value, data, locale, &out->formatter.spellout) >= 0;
    }
    out->kind = preparedRelative;
    return MakeRelative(value, data, locale, &out->formatter.relative) >= 0;
}

// Writes a spelled-out value. Its text has no parts, and the formatter
// refuses an output that asks for them.
static mlocResult WriteSpellout(const mlocSpelloutFormatter* formatter, const NumberValue* value,
                                mlocOutput* output)
{
    mlocOutput plain = {output->buffer, output->capacity, 0, NULL, NULL};
    mlocResult result = mlocSpelloutFormatter_Format(formatter, &value->decimal, &plain);
    output->length = plain.length;
    return result;
}

static mlocResult WriteRelative(const mlocRelativeFormatter* formatter, const NumberValue* value,
                                mlocOutput* output)
{
    return mlocRelativeFormatter_Format(formatter, &value->decimal,
                                        (mlocRelativeUnit)value->relativeUnit, output);
}

// Writes a value with a formatter made here, off the resolver's frames.
MLOC_NOINLINE static mlocResult WriteMade(const NumberValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output)
{
    if (value->function == numberFunctionRelative)
    {
        mlocRelativeFormatter relative;
        mlocResult made = MakeRelative(value, data, locale, &relative);
        return made < 0 ? made : WriteRelative(&relative, value, output);
    }
    mlocSpelloutFormatter spellout;
    mlocResult made = MakeSpellout(value, data, locale, &spellout);
    return made < 0 ? made : WriteSpellout(&spellout, value, output);
}

mlocMessageFaultKind mlocWriteWordsValue(const NumberValue* value, const PreparedNumber* prepared,
                                         const mlocDataSet* data, const mlocLocale* locale,
                                         mlocOutput* output)
{
    mlocResult result = !mlocNumberFits(prepared, value) ? WriteMade(value, data, locale, output)
                        : value->function == numberFunctionRelative
                            ? WriteRelative(&prepared->formatter.relative, value, output)
                            : WriteSpellout(&prepared->formatter.spellout, value, output);
    // A text past the room reports the length it needs.
    return result == mloc_success || (result == mloc_errorCapacity && output->length > 0)
               ? mloc_messageFaultNone
               : mloc_messageFaultUnsupportedOperation;
}
