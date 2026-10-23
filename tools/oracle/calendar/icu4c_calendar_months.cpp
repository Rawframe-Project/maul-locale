// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Prints every month start of a calendar between two Gregorian years, as
// ICU4C computes it: "YYYY-MM-DD Mnn[L]".
#include <unicode/calendar.h>
#include <unicode/gregocal.h>
#include <unicode/timezone.h>
#include <cstdio>
#include <cstdlib>
using namespace icu;
int main(int argc, char** argv) {
    UErrorCode e = U_ZERO_ERROR;
    char loc[64]; snprintf(loc, sizeof loc, "en@calendar=%s", argv[1]);
    int from = atoi(argv[2]), to = atoi(argv[3]);
    TimeZone* gmt = TimeZone::createTimeZone("Etc/GMT");
    Calendar* c = Calendar::createInstance(gmt->clone(), Locale(loc), e);
    GregorianCalendar g(gmt->clone(), e);
    g.setGregorianChange(-8.64e15, e); // proleptic
    g.clear(); g.set(from, 0, 1, 12, 0, 0);
    UDate t = g.getTime(e);
    g.clear(); g.set(to + 1, 0, 1, 12, 0, 0);
    UDate end = g.getTime(e);
    for (; t < end; t += 86400000.0) {
        c->setTime(t, e);
        if (c->get(UCAL_DATE, e) == 1) {
            g.setTime(t, e);
            printf("%04d-%02d-%02d M%02d%s\n", g.get(UCAL_EXTENDED_YEAR, e), g.get(UCAL_MONTH, e) + 1,
                   g.get(UCAL_DATE, e), c->get(UCAL_MONTH, e) + 1, c->get(UCAL_IS_LEAP_MONTH, e) ? "L" : "");
        }
    }
    if (U_FAILURE(e)) { fprintf(stderr, "error %s\n", u_errorName(e)); return 1; }
    return 0;
}
