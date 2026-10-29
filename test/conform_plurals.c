// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Every sample of CLDR's plural rules against the library: reads the lines
// of `mlocgen plural-samples` ("type<TAB>id <TAB>category<TAB>sample") and
// selects the sample's category for the id on the compiled-in data. A
// compact sample ("1.1c6") is the whole value (1100000) with the exponent
// as c. CI runs it.
//
// usage: mlocgen plural-samples <common> | conform_plurals

#include "maul-locale/plural.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const s_categories[6] = {"zero", "one", "two", "few", "many", "other"};

// The category a sample selects, or -1 when it cannot be read.
static int Select(const mlocDataSet* data, const char* type, const char* id, const char* sample)
{
    mlocLocale locale;
    mlocPluralRules rules;
    mlocPluralType kind = strcmp(type, "ordinal") == 0 ? mloc_pluralOrdinal : mloc_pluralCardinal;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        mlocInitPluralRules(data, &locale, kind, &rules) < 0)
    {
        return -1;
    }
    char text[128];
    snprintf(text, sizeof(text), "%s", sample);
    char* compact = strchr(text, 'c');
    uint32_t exponent = 0;
    if (compact != nullptr)
    {
        // "1.1c6" reads as "1.1e6", the whole value.
        exponent = (uint32_t)strtoul(compact + 1, nullptr, 10);
        *compact = 'e';
    }
    mlocDecimal decimal;
    mlocPluralOperands operands;
    mlocPluralCategory category;
    if (mlocParseDecimal(text, strlen(text), &decimal).status != mloc_success ||
        mlocGetPluralOperands(&decimal, &operands) != mloc_success)
    {
        return -1;
    }
    operands.c = exponent;
    if (mlocPluralRules_SelectOperands(&rules, &operands, &category) != mloc_success)
    {
        return -1;
    }
    return (int)category;
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[512];
    long total = 0;
    long agreed = 0;
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char* fields[4] = {line};
        int count = 1;
        for (char* at = line; *at != '\0' && count < 4; at++)
        {
            if (*at == '\t')
            {
                *at = '\0';
                fields[count++] = at + 1;
            }
        }
        if (count != 4)
        {
            continue;
        }
        total++;
        int got = Select(&data, fields[0], fields[1], fields[3]);
        if (got >= 0 && strcmp(s_categories[got], fields[2]) == 0)
        {
            agreed++;
        }
        else if (total - agreed <= 20)
        {
            printf("%s %s %s: got %s\n", fields[0], fields[1], fields[3],
                   got >= 0 ? s_categories[got] : "(failed)");
        }
    }
    printf("plurals: %ld of %ld samples agree with CLDR\n", agreed, total);
    return agreed == total && total > 0 ? 0 : 1;
}
