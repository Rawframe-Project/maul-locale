// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloc:case and mloc:particle (record mloc-0052).

#include "message_grammar.h"

#include "message_number.h"

#include <string.h>

static bool NameIs(const uint8_t* name, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(name, text, length) == 0;
}

GrammarValue mlocMakeGrammarText(uint8_t function, const uint8_t* text, size_t length)
{
    GrammarValue v;
    memset(&v, 0, sizeof(v));
    v.function = function;
    v.text = text;
    v.length = length;
    return v;
}

GrammarValue mlocMakeGrammarNumber(uint8_t function, const NumberValue* number)
{
    GrammarValue v;
    memset(&v, 0, sizeof(v));
    v.function = function;
    v.isNumber = true;
    v.number = *number;
    return v;
}

// The index of an option's literal text in names; -1 for none.
static int Choice(const NumberOption* option, const char* const* names, int count)
{
    for (int i = 0; option->hasText && option->literal && i < count; i++)
    {
        if (NameIs(option->text, option->length, names[i]))
        {
            return i;
        }
    }
    return -1;
}

// The options that choose the rules, by name; their values' names in
// their enumerations' orders.
static const char* const s_cases[28] = {"nominative",
                                        "abessive",
                                        "ablative",
                                        "accusative",
                                        "adessive",
                                        "allative",
                                        "causal",
                                        "comitative",
                                        "dative",
                                        "delative",
                                        "elative",
                                        "ergative",
                                        "essive",
                                        "genitive",
                                        "illative",
                                        "inessive",
                                        "instrumental",
                                        "locative",
                                        "locativecopulative",
                                        "oblique",
                                        "partitive",
                                        "prepositional",
                                        "sociative",
                                        "sublative",
                                        "superessive",
                                        "terminative",
                                        "translative",
                                        "vocative"};
static const char* const s_particles[17] = {
    "none",     "topic",        "subject", "object",    "comitative", "comitativeSpoken",
    "vocative", "copulaPolite", "copula",  "direction", "capacity",   "means",
    "source",   "choice",       "any",     "quotation", "definition"};
static const char* const s_possessors[7] = {"none", "1", "2", "3", "1p", "2p", "3p"};
static const char* const s_booleans[2] = {"false", "true"};
static const char* const s_numbers[2] = {"singular", "plural"};
static const char* const s_nouns[2] = {"proper", "common"};
static const char* const s_readings[3] = {"auto", "word", "letters"};

// Each option's value, by its index among its names, into a request.
static void SetCase(mlocInflection* r, int i)
{
    r->grammaticalCase = (mlocGrammaticalCase)i;
}

static void SetParticle(mlocInflection* r, int i)
{
    r->particle = (mlocKoreanParticle)i;
}

static void SetPossessor(mlocInflection* r, int i)
{
    r->possessorPerson = i > 0 ? (uint8_t)((i - 1) % 3 + 1) : 0;
    r->possessorPlural = i > 3;
}

static void SetNumber(mlocInflection* r, int i)
{
    r->plural = i == 1;
}

static void SetCopula(mlocInflection* r, int i)
{
    r->copula = i == 1;
}

static void SetQuestion(mlocInflection* r, int i)
{
    r->question = i == 1;
}

static void SetNoun(mlocInflection* r, int i)
{
    r->commonNoun = i == 1;
}

static void SetReading(mlocInflection* r, int i)
{
    r->reading = (mlocInflectionReading)i;
}

// Sets one of the options that choose the rules; false for a name it
// does not know, *bad set for a value it does not take.
static bool SetRule(mlocInflection* r, const uint8_t* name, size_t length,
                    const NumberOption* option, bool* bad)
{
    static const struct
    {
        const char* name;
        const char* const* values;
        int count;
        void (*set)(mlocInflection*, int);
    } s_rules[] = {
        {"case", s_cases, 28, SetCase},
        {"particle", s_particles, 17, SetParticle},
        {"possessor", s_possessors, 7, SetPossessor},
        {"number", s_numbers, 2, SetNumber},
        {"copula", s_booleans, 2, SetCopula},
        {"question", s_booleans, 2, SetQuestion},
        {"noun", s_nouns, 2, SetNoun},
        {"reading", s_readings, 3, SetReading},
    };
    for (size_t k = 0; k < sizeof(s_rules) / sizeof(s_rules[0]); k++)
    {
        if (NameIs(name, length, s_rules[k].name))
        {
            int i = Choice(option, s_rules[k].values, s_rules[k].count);
            if (i >= 0)
            {
                s_rules[k].set(r, i);
            }
            *bad = i < 0;
            return true;
        }
    }
    return false;
}

mlocMessageFaultKind mlocSetGrammarOption(GrammarValue* value, const uint8_t* name, size_t length,
                                          const NumberOption* option)
{
    if (NameIs(name, length, "hint"))
    {
        // The hint may be a variable's text; anything else is ignored.
        if (!option->hasText)
        {
            return mloc_messageFaultBadOption;
        }
        value->request.hint = (const char*)option->text;
        value->request.hintLength = option->length;
        return mloc_messageFaultNone;
    }
    // Each function reads its own options and ignores the other's:
    // mloc:particle takes particle and reading, mloc:case the rest.
    bool particleOption = NameIs(name, length, "particle") || NameIs(name, length, "reading");
    if (value->function == grammarParticle && !particleOption)
    {
        return mloc_messageFaultNone;
    }
    if (value->function == grammarCase && NameIs(name, length, "particle"))
    {
        return mloc_messageFaultNone;
    }
    bool bad = false;
    if (!SetRule(&value->request, name, length, option, &bad))
    {
        return mloc_messageFaultNone;
    }
    return bad ? mloc_messageFaultBadOption : mloc_messageFaultNone;
}

mlocMessageFaultKind mlocSetGrammarFrom(void* context, const uint8_t* name, size_t length,
                                        const NumberOption* option)
{
    return option == nullptr ? mloc_messageFaultNone
                             : mlocSetGrammarOption(context, name, length, option);
}

bool mlocPrepareGrammar(const mlocDataSet* data, const mlocLocale* locale, PreparedGrammar* out)
{
    memset(out, 0, offsetof(PreparedGrammar, inflector));
    out->made = mlocInitInflector(&out->inflector, data, locale) == mloc_success ? 1 : 0;
    return out->made != 0;
}

// The fault a grammar call's result is.
static mlocMessageFaultKind FaultOf(mlocResult result)
{
    return result == mloc_success || result == mloc_errorCapacity ? mloc_messageFaultNone
           : result == mloc_errorInvalid ? mloc_messageFaultBadOption
                                         : mloc_messageFaultUnsupportedOperation;
}

mlocMessageFaultKind mlocWriteGrammarValue(const GrammarValue* value,
                                           const PreparedGrammar* prepared, const mlocDataSet* data,
                                           const mlocLocale* locale, mlocOutput* output)
{
    PreparedGrammar made;
    if (prepared == nullptr || prepared->made == 0)
    {
        if (!mlocPrepareGrammar(data, locale, &made))
        {
            return mloc_messageFaultUnsupportedOperation;
        }
        prepared = &made;
    }
    if (!value->isNumber)
    {
        return FaultOf(mlocInflectText(&prepared->inflector, (const char*)value->text,
                                       value->length, &value->request, output));
    }
    PreparedNumber number;
    if (!mlocPrepareNumber(&value->number, data, locale, &number) || number.kind != preparedNumber)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    return FaultOf(mlocInflectNumber(&prepared->inflector, &number.formatter.number,
                                     &value->number.decimal, &value->request, output));
}
