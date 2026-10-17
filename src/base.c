// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Versions and result names.

#include "maul-locale/base.h"

mlocVersion mlocGetVersion(void)
{
    return (mlocVersion){MLOC_VERSION_MAJOR, MLOC_VERSION_MINOR, MLOC_VERSION_PATCH};
}

mlocVersion mlocGetCldrVersion(void)
{
    return (mlocVersion){MLOC_CLDR_VERSION_MAJOR, MLOC_CLDR_VERSION_MINOR, MLOC_CLDR_VERSION_PATCH};
}

const char* mlocResultName(mlocResult result)
{
    switch (result)
    {
    case mloc_success:
        return "mloc_success";
    case mloc_fellBack:
        return "mloc_fellBack";
    case mloc_withFallbacks:
        return "mloc_withFallbacks";
    case mloc_errorInvalid:
        return "mloc_errorInvalid";
    case mloc_errorCapacity:
        return "mloc_errorCapacity";
    case mloc_errorData:
        return "mloc_errorData";
    case mloc_errorDataVersion:
        return "mloc_errorDataVersion";
    case mloc_errorSyntax:
        return "mloc_errorSyntax";
    case mloc_errorNotFound:
        return "mloc_errorNotFound";
    case mloc_errorFormat:
        return "mloc_errorFormat";
    default:
        return "unknown result";
    }
}
