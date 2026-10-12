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

static void TestResultNames(void)
{
    CHECK(strcmp(mlocResultName(mloc_success), "mloc_success") == 0, "success name");
    CHECK(strcmp(mlocResultName(mloc_errorInvalid), "mloc_errorInvalid") == 0, "invalid name");
    CHECK(strcmp(mlocResultName(mloc_errorCapacity), "mloc_errorCapacity") == 0, "capacity name");
    CHECK(strcmp(mlocResultName(mloc_errorData), "mloc_errorData") == 0, "data name");
    CHECK(strcmp(mlocResultName(mloc_errorDataVersion), "mloc_errorDataVersion") == 0,
          "data version name");
    CHECK(strcmp(mlocResultName(mloc_errorSyntax), "mloc_errorSyntax") == 0, "syntax name");
    CHECK(strcmp(mlocResultName(mloc_errorNotFound), "mloc_errorNotFound") == 0, "not found name");
    CHECK(strcmp(mlocResultName(12345), "unknown result") == 0, "unknown name");
}

int main(void)
{
    TestVersionMatchesHeader();
    TestCldrVersionIsFortyNine();
    TestResultNames();
    return s_failures == 0 ? 0 : 1;
}
