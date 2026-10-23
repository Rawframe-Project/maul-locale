// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the generator's collation rule parser. Any bytes
// are parsed or refused with a byte inside the text; parsed rules are
// written back and parse into the same list.

#include "collation_rules.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    CollationRules rules;
    char error[256];
    if (!CollationRulesParse((const char*)data, size, &rules, error, sizeof(error)))
    {
        // "byte N: ..." with N within the text.
        char* end = nullptr;
        unsigned long byte = strncmp(error, "byte ", 5) == 0 ? strtoul(error + 5, &end, 10) : 0;
        if (end == nullptr || byte > size)
        {
            abort();
        }
        return 0;
    }
    size_t length = 0;
    char* written = CollationRulesWrite(&rules, &length);
    CollationRules again;
    if (!CollationRulesParse(written, length, &again, error, sizeof(error)) ||
        !CollationRulesSame(&rules, &again))
    {
        abort();
    }
    CollationRulesFree(&again);
    CollationRulesFree(&rules);
    free(written);
    return 0;
}
