// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The option names of mloc:case, mloc:particle and mloc:duration
// (maul-locale/message.h), read by their option parsers: each value of
// each enumeration an option sets, from its first to its last, is named,
// and its name gives that value and no other; a duration field takes the
// styles ECMA-402 gives it and refuses the rest.

#include "message_duration.h"
#include "message_grammar.h"
#include "test_harness.h"

#include <string.h>

// The names each option takes, keyed by the public enumeration's value;
// null for a value no name gives.
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
static const char* const s_readings[] = {
    [mloc_readingAuto] = "auto",
    [mloc_readingWord] = "word",
    [mloc_readingLetters] = "letters",
};
static const char* const s_fields[] = {
    [mloc_durationYears] = "years",
    [mloc_durationMonths] = "months",
    [mloc_durationWeeks] = "weeks",
    [mloc_durationDays] = "days",
    [mloc_durationHours] = "hours",
    [mloc_durationMinutes] = "minutes",
    [mloc_durationSeconds] = "seconds",
    [mloc_durationMilliseconds] = "milliseconds",
    [mloc_durationMicroseconds] = "microseconds",
    [mloc_durationNanoseconds] = "nanoseconds",
};
// The defaults have no name.
static const char* const s_fieldStyles[] = {
    [mloc_durationFieldLong] = "long",        [mloc_durationFieldShort] = "short",
    [mloc_durationFieldNarrow] = "narrow",    [mloc_durationFieldNumeric] = "numeric",
    [mloc_durationFieldTwoDigit] = "2-digit",
};
static const char* const s_displays[] = {
    [mloc_durationDisplayAuto] = "auto",
    [mloc_durationDisplayAlways] = "always",
};
static const char* const s_styles[] = {
    [mloc_durationStyleShort] = "short",
    [mloc_durationStyleLong] = "long",
    [mloc_durationStyleNarrow] = "narrow",
    [mloc_durationStyleDigital] = "digital",
};

#define COUNT(table) (int)(sizeof(table) / sizeof((table)[0]))

static_assert(COUNT(s_cases) == mloc_caseVocative + 1, "a name for each mlocGrammaticalCase");
static_assert(COUNT(s_particles) == mloc_particleDefinition + 1,
              "a name for each mlocKoreanParticle");
static_assert(COUNT(s_readings) == mloc_readingLetters + 1, "a name for each reading");
static_assert(COUNT(s_fields) == mloc_durationFieldCount, "a name for each mlocDurationField");
static_assert(COUNT(s_fieldStyles) == mloc_durationFieldTwoDigit + 1, "each field style");
static_assert(COUNT(s_displays) == mloc_durationDisplayAlways + 1, "each display");
static_assert(COUNT(s_styles) == mloc_durationStyleDigital + 1, "each style");

static NumberOption Literal(const char* text)
{
    return (NumberOption){
        .text = (const uint8_t*)text, .length = strlen(text), .hasText = true, .literal = true};
}

static mlocMessageFaultKind SetGrammar(GrammarValue* value, const char* name, const char* text)
{
    NumberOption option = Literal(text);
    return mlocSetGrammarOption(value, (const uint8_t*)name, strlen(name), &option);
}

static mlocMessageFaultKind SetDuration(DurationValue* value, const char* name, const char* text)
{
    NumberOption option = Literal(text);
    return mlocSetDurationOption(value, (const uint8_t*)name, strlen(name), &option);
}

// Every value but the nameless first ones (first) has a name.
static bool Named(const char* const* names, int count, int first)
{
    bool named = true;
    for (int i = 0; i < count; i++)
    {
        named = named && (names[i] != nullptr) == (i >= first);
    }
    return named;
}

static void TestGrammar(void)
{
    CHECK(Named(s_cases, COUNT(s_cases), 0) && Named(s_particles, COUNT(s_particles), 0) &&
              Named(s_readings, COUNT(s_readings), 0),
          "every case, particle and reading named");
    static const uint8_t word[] = "kitap";
    int good = 0;
    for (int i = 0; i < COUNT(s_cases); i++)
    {
        GrammarValue value = mlocMakeGrammarText(grammarCase, word, 5);
        good += SetGrammar(&value, "case", s_cases[i]) == mloc_messageFaultNone &&
                value.request.grammaticalCase == i;
    }
    CHECK(good == COUNT(s_cases), "each case's name gives its case");
    good = 0;
    for (int i = 0; i < COUNT(s_particles); i++)
    {
        GrammarValue value = mlocMakeGrammarText(grammarParticle, word, 5);
        good += SetGrammar(&value, "particle", s_particles[i]) == mloc_messageFaultNone &&
                value.request.particle == i;
    }
    CHECK(good == COUNT(s_particles), "each particle's name gives its particle");
    good = 0;
    for (int i = 0; i < COUNT(s_readings); i++)
    {
        GrammarValue value = mlocMakeGrammarText(grammarParticle, word, 5);
        good += SetGrammar(&value, "reading", s_readings[i]) == mloc_messageFaultNone &&
                value.request.reading == i;
    }
    CHECK(good == COUNT(s_readings), "each reading's name gives its reading");
    GrammarValue cased = mlocMakeGrammarText(grammarCase, word, 5);
    GrammarValue particled = mlocMakeGrammarText(grammarParticle, word, 5);
    CHECK(SetGrammar(&cased, "case", "dativ") == mloc_messageFaultBadOption &&
              SetGrammar(&particled, "particle", "") == mloc_messageFaultBadOption,
          "a name of no value is refused");
}

// The styles a field takes (ECMA-402's DurationFormat): years to days the
// names, hours to seconds also numeric and 2-digit, the smaller ones also
// numeric.
static bool Takes(int field, int style)
{
    if (style <= mloc_durationFieldNarrow)
    {
        return true;
    }
    if (style == mloc_durationFieldNumeric)
    {
        return field >= mloc_durationHours;
    }
    return field >= mloc_durationHours && field <= mloc_durationSeconds;
}

static void TestDuration(void)
{
    CHECK(Named(s_fields, COUNT(s_fields), 0) &&
              Named(s_fieldStyles, COUNT(s_fieldStyles), mloc_durationFieldLong) &&
              Named(s_displays, COUNT(s_displays), mloc_durationDisplayAuto) &&
              Named(s_styles, COUNT(s_styles), 0),
          "every field, style and display named but the defaults");
    mlocDurationValues zero;
    memset(&zero, 0, sizeof zero);
    int good = 0;
    for (int i = 0; i < COUNT(s_styles); i++)
    {
        DurationValue value = mlocMakeDurationValue(&zero);
        good += SetDuration(&value, "style", s_styles[i]) == mloc_messageFaultNone &&
                value.key.style == i;
    }
    CHECK(good == COUNT(s_styles), "each style's name gives its style");
    good = 0;
    int cases = 0;
    for (int field = 0; field < COUNT(s_fields); field++)
    {
        char display[32];
        (void)snprintf(display, sizeof display, "%sDisplay", s_fields[field]);
        for (int i = mloc_durationFieldLong; i < COUNT(s_fieldStyles); i++)
        {
            DurationValue value = mlocMakeDurationValue(&zero);
            mlocMessageFaultKind fault = SetDuration(&value, s_fields[field], s_fieldStyles[i]);
            good += Takes(field, i) ? fault == mloc_messageFaultNone && value.key.fields[field] == i
                                    : fault == mloc_messageFaultBadOption;
            cases++;
        }
        for (int i = mloc_durationDisplayAuto; i < COUNT(s_displays); i++)
        {
            DurationValue value = mlocMakeDurationValue(&zero);
            good += SetDuration(&value, display, s_displays[i]) == mloc_messageFaultNone &&
                    value.key.displays[field] == i;
            cases++;
        }
    }
    CHECK(good == cases, "each field takes its styles by name, refuses the others, and its "
                         "display's names");
}

int main(void)
{
    TestGrammar();
    TestDuration();
    return s_failures == 0 ? 0 : 1;
}
