// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Turkic letters' indexed ASCII table (src/grammar_turkic.c, record
// mloc-0047): every ASCII code point read by index as the table of Latin
// letters reads it by search.

#include "grammar_turkic.h"
#include "test_harness.h"

int main(void)
{
    // A language whose table begins in ASCII takes the searched path.
    static const TurkicLetter s_first[1] = {{0, turkicVoiced}};
    TurkicLanguage searched = mlocTurkish;
    searched.letters = s_first;
    searched.letterCount = 1;
    bool same = true;
    for (uint32_t c = 0; c < 128; c++)
    {
        same = same && mlocTurkicLetterInfo(&mlocTurkish, c) == mlocTurkicLetterInfo(&searched, c);
    }
    CHECK(same, "ASCII by index as by search");
    return s_failures == 0 ? 0 : 1;
}
