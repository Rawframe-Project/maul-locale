// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloc:case and mloc:particle.

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

// The index of an option's literal text in names (null entries
// skipped); -1 for none.
static int Choice(const NumberOption* option, const char* const* names, int count)
{
    for (int i = 0; option->hasText && option->literal && i < count; i++)
    {
        if (names[i] != nullptr && NameIs(option->text, option->length, names[i]))
        {
            return i;
        }
    }
    return -1;
}

// The options that choose the rules, by name. The case, particle and
// reading names are keyed by the public enumeration's value, so that the
// index a name is found at is the value it gives; the others' names are
// in the order their setters read.
static const char* const s_cases[] = {
    [mloc_caseNominative] = "nominative",
    [mloc_caseAbessive] = "abessive",
    [mloc_caseAblative] = "ablative",
    [mloc_caseAccusative] = "accusative",
    [mloc_caseAdessive] = "adessive",
    [mloc_caseAllative] = "allative",
    [mloc_caseCausal] = "causal",
    [mloc_caseComitative] = "comitative",
    [mloc_caseDative] = "dative",
    [mloc_caseDelative] = "delative",
    [mloc_caseElative] = "elative",
    [mloc_caseErgative] = "ergative",
    [mloc_caseEssive] = "essive",
    [mloc_caseGenitive] = "genitive",
    [mloc_caseIllative] = "illative",
    [mloc_caseInessive] = "inessive",
    [mloc_caseInstrumental] = "instrumental",
    [mloc_caseLocative] = "locative",
    [mloc_caseLocativecopulative] = "locativecopulative",
    [mloc_caseOblique] = "oblique",
    [mloc_casePartitive] = "partitive",
    [mloc_casePrepositional] = "prepositional",
    [mloc_caseSociative] = "sociative",
    [mloc_caseSublative] = "sublative",
    [mloc_caseSuperessive] = "superessive",
    [mloc_caseTerminative] = "terminative",
    [mloc_caseTranslative] = "translative",
    [mloc_caseVocative] = "vocative",
};
static const char* const s_particles[] = {
    [mloc_particleNone] = "none",
    [mloc_particleTopic] = "topic",
    [mloc_particleSubject] = "subject",
    [mloc_particleObject] = "object",
    [mloc_particleComitative] = "comitative",
    [mloc_particleComitativeSpoken] = "comitativeSpoken",
    [mloc_particleVocative] = "vocative",
    [mloc_particleCopulaPolite] = "copulaPolite",
    [mloc_particleCopula] = "copula",
    [mloc_particleDirection] = "direction",
    [mloc_particleCapacity] = "capacity",
    [mloc_particleMeans] = "means",
    [mloc_particleSource] = "source",
    [mloc_particleChoice] = "choice",
    [mloc_particleAny] = "any",
    [mloc_particleQuotation] = "quotation",
    [mloc_particleDefinition] = "definition",
};
static const char* const s_possessors[7] = {"none", "1", "2", "3", "1p", "2p", "3p"};
static const char* const s_booleans[2] = {"false", "true"};
static const char* const s_numbers[2] = {"singular", "plural"};
static const char* const s_nouns[2] = {"proper", "common"};
static const char* const s_readings[] = {
    [mloc_readingAuto] = "auto",
    [mloc_readingWord] = "word",
    [mloc_readingLetters] = "letters",
};

#define NAMES(table) table, (int)(sizeof(table) / sizeof((table)[0]))

// Each option's value, by its index among its names, into a request.
// A name for each value the options set.
static_assert(sizeof(s_cases) / sizeof(s_cases[0]) == mloc_caseVocative + 1, "a name per case");
static_assert(sizeof(s_particles) / sizeof(s_particles[0]) == mloc_particleDefinition + 1,
              "a name per particle");
static_assert(sizeof(s_readings) / sizeof(s_readings[0]) == mloc_readingLetters + 1,
              "a name per reading");
// A person's gender's names, keyed by the public enumeration's value.
static const char* const s_genders[] = {
    [mloc_personOther] = "other",
    [mloc_personMale] = "male",
    [mloc_personFemale] = "female",
};
static_assert(sizeof(s_genders) / sizeof(s_genders[0]) == mloc_personFemale + 1,
              "a name for each mlocPersonGender");

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
        {"case", NAMES(s_cases), SetCase},
        {"particle", NAMES(s_particles), SetParticle},
        {"possessor", NAMES(s_possessors), SetPossessor},
        {"number", NAMES(s_numbers), SetNumber},
        {"copula", NAMES(s_booleans), SetCopula},
        {"question", NAMES(s_booleans), SetQuestion},
        {"noun", NAMES(s_nouns), SetNoun},
        {"reading", NAMES(s_readings), SetReading},
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
    out->made = mlocInitInflector(data, locale, &out->inflector) == mloc_success ? 1 : 0;
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
        return FaultOf(mlocInflector_InflectText(&prepared->inflector, (const char*)value->text,
                                                 value->length, &value->request, output));
    }
    PreparedNumber number;
    if (!mlocPrepareNumber(&value->number, data, locale, &number) || number.kind != preparedNumber)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    return FaultOf(mlocInflector_InflectNumber(&prepared->inflector, &number.formatter.number,
                                               &value->number.decimal, &value->request, output));
}

// A person's gender by its name; -1 for another text.
static int PersonGender(const uint8_t* text, size_t length)
{
    for (int i = 0; i < (int)(sizeof(s_genders) / sizeof(s_genders[0])); i++)
    {
        if (text != nullptr && NameIs(text, length, s_genders[i]))
        {
            return i;
        }
    }
    return -1;
}

bool mlocMakeGenderValue(const mlocListItem* items, size_t count, const uint8_t* text,
                         size_t length, const mlocDataSet* data, const mlocLocale* locale,
                         GenderValue* out)
{
    // The list's gender reads its count and whether every person is
    // male or female, so a list of any length stands as one of two.
    size_t persons = items != nullptr ? count : 1;
    size_t males = 0;
    size_t females = 0;
    int first = 0;
    for (size_t i = 0; i < persons; i++)
    {
        int g = items != nullptr ? PersonGender((const uint8_t*)items[i].text, items[i].length)
                                 : PersonGender(text, length);
        if (g < 0)
        {
            return false;
        }
        first = i == 0 ? g : first;
        males += g == mloc_personMale ? 1 : 0;
        females += g == mloc_personFemale ? 1 : 0;
    }
    mlocPersonGender genders[2] = {(mlocPersonGender)first, (mlocPersonGender)first};
    if (persons > 1)
    {
        // Two of one gender stand for a list all of it, a male and a
        // female for any other.
        mlocPersonGender all = males == persons     ? mloc_personMale
                               : females == persons ? mloc_personFemale
                                                    : mloc_personOther;
        genders[0] = all == mloc_personOther ? mloc_personMale : all;
        genders[1] = all == mloc_personOther ? mloc_personFemale : all;
    }
    mlocPersonGender gender;
    if (mlocGetListGender(data, locale, genders, persons < 2 ? persons : 2, &gender) !=
        mloc_success)
    {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->gender = (uint8_t)gender;
    return true;
}

mlocMessageFaultKind mlocMatchGender(const GenderValue* value, const uint8_t* key, size_t length,
                                     bool* matchOut)
{
    int g = PersonGender(key, length);
    if (g < 0)
    {
        *matchOut = false;
        return mloc_messageFaultBadVariantKey;
    }
    *matchOut = g == value->gender;
    return mloc_messageFaultNone;
}
