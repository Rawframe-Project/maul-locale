// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The gender of a list of persons: the locale's rule from the generated
// table, then UTS #35's definitions, with ICU's answers for a list of none
// or one.

#include "list_genders.h"

#include "maul-locale/list.h"

#include <string.h>

// The rule of a locale ID; -1 for an ID the table lacks.
static int RuleOf(const char* id)
{
    uint32_t low = 0;
    uint32_t high = mlocListGenderRuleCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        int order = strcmp(mlocListGenderRules[middle].locale, id);
        if (order == 0)
        {
            return mlocListGenderRules[middle].rule;
        }
        if (order < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return -1;
}

// The locale's rule: canonicalized by the data set's aliases where it
// can be, then its language and script, then its language; neutral for
// none.
static int LocaleRule(const mlocDataSet* data, const mlocLocale* locale)
{
    mlocLocale canonical = *locale;
    char tag[160];
    mlocOutput output = {tag, sizeof(tag), 0, nullptr, nullptr};
    mlocLocale aliased;
    if (mlocFormatLocale(locale, &output) == mloc_success &&
        mlocCanonicalizeLocale(data, tag, output.length, &aliased, nullptr).status == mloc_success)
    {
        canonical = aliased;
    }
    char id[16];
    size_t language = 0;
    while (language < sizeof(canonical.language) && canonical.language[language] != '\0')
    {
        language++;
    }
    memcpy(id, canonical.language, language);
    id[language] = '\0';
    if (canonical.script[0] != '\0')
    {
        char scripted[16];
        memcpy(scripted, id, language);
        scripted[language] = '_';
        memcpy(scripted + language + 1, canonical.script, 4);
        scripted[language + 5] = '\0';
        int rule = RuleOf(scripted);
        if (rule >= 0)
        {
            return rule;
        }
    }
    int rule = RuleOf(id);
    return rule >= 0 ? rule : listGenderNeutral;
}

mlocResult mlocGetListGender(const mlocDataSet* data, const mlocLocale* locale,
                             const mlocPersonGender* genders, size_t count,
                             mlocPersonGender* genderOut)
{
    if (data == nullptr || locale == nullptr || genderOut == nullptr ||
        (genders == nullptr && count > 0))
    {
        return mloc_errorInvalid;
    }
    bool allMale = true;
    bool allFemale = true;
    for (size_t i = 0; i < count; i++)
    {
        if ((uint32_t)genders[i] > mloc_personFemale)
        {
            return mloc_errorInvalid;
        }
        allMale = allMale && genders[i] == mloc_personMale;
        allFemale = allFemale && genders[i] == mloc_personFemale;
    }
    if (count <= 1)
    {
        *genderOut = count == 0 ? mloc_personOther : genders[0];
        return mloc_success;
    }
    int rule = LocaleRule(data, locale);
    *genderOut = rule == listGenderNeutral                 ? mloc_personOther
                 : allFemale                               ? mloc_personFemale
                 : rule == listGenderMaleTaints || allMale ? mloc_personMale
                                                           : mloc_personOther;
    return mloc_success;
}
