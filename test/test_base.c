// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Versions and result names.

#include "test_harness.h"

#include "maul-locale/base.h"

#include <string.h>

static void TestVersionMatchesHeader(void)
{
    mlocVersion version = mlocGetVersion();
    CHECK(version.major == MLOC_VERSION_MAJOR, "major version");
    CHECK(version.minor == MLOC_VERSION_MINOR, "minor version");
    CHECK(version.patch == MLOC_VERSION_PATCH, "patch version");
}

static void TestCldrVersionIsFortyNine(void)
{
    mlocVersion version = mlocGetCldrVersion();
    CHECK(version.major == 49 && version.minor == 0 && version.patch == 0, "CLDR 49.0.0");
}

// Every result with its name; the shared errors at the family's values,
// the library's own from -32 down.
static void TestResultNames(void)
{
    static const struct
    {
        mlocResult value;
        mlocResult expected;
        const char* name;
    } results[] = {
        {mloc_success, 0, "mloc_success"},
        {mloc_fellBack, 1, "mloc_fellBack"},
        {mloc_withFallbacks, 2, "mloc_withFallbacks"},
        {mloc_done, 3, "mloc_done"},
        {mloc_needMoreText, 4, "mloc_needMoreText"},
        {mloc_errorInvalid, -1, "mloc_errorInvalid"},
        {mloc_errorCapacity, -2, "mloc_errorCapacity"},
        {mloc_errorVersion, -7, "mloc_errorVersion"},
        {mloc_errorFormat, -8, "mloc_errorFormat"},
        {mloc_errorSyntax, -32, "mloc_errorSyntax"},
        {mloc_errorNotFound, -33, "mloc_errorNotFound"},
        {mloc_errorFallback, -34, "mloc_errorFallback"},
    };
    for (size_t i = 0; i < sizeof results / sizeof results[0]; i++)
    {
        CHECK(results[i].value == results[i].expected &&
                  strcmp(mlocResultName(results[i].value), results[i].name) == 0,
              results[i].name);
    }
    CHECK(strcmp(mlocResultName(12345), "unknown result") == 0, "unknown name");
}

int main(void)
{
    TestVersionMatchesHeader();
    TestCldrVersionIsFortyNine();
    TestResultNames();
    return s_failures == 0 ? 0 : 1;
}
