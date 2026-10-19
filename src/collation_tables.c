// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_tables.h"

bool mlocValidCollationRoot(const DataView* view, DataSpan payload)
{
    (void)view;
    CollationRoot root;
    return mlocReadCollationRoot(payload, &root);
}
