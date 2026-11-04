// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formatters whose stamp and counts are right but whose bool or
// enumeration bytes are not (the library checks every byte it stores
// before reading it: a bool's byte other than 0 or 1 is undefined to
// read): a currency, unit, spell-out or message formatter with such a byte
// is refused by the call that reads it; each formatter the library made
// formats, before and after.

#include "currency_format.h"
#include "message_bind.h"
#include "spellout_format.h"
#include "test_harness.h"
#include "unit_patterns.h"

#include "maul-locale/currency.h"
#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/message.h"
#include "maul-locale/spellout.h"
#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_spellout[1 << 21];
alignas(8) static uint8_t s_units[1 << 22];
static mlocData s_sources[3];
static mlocDataSet s_data = {s_sources, 3};
static mlocLocale s_en;

static bool Open(const char* path, uint8_t* bytes, size_t capacity, mlocData* out)
{
    FILE* f = fopen(path, "rb");
    size_t size = f == nullptr ? 0 : fread(bytes, 1, capacity, f);
    if (f != nullptr)
    {
        fclose(f);
    }
    return size > 0 && size < capacity && mlocOpenData(bytes, size, out) == mloc_success;
}

static mlocDecimal Value(void)
{
    mlocDecimal d;
    (void)mlocParseDecimal("12.5", 4, &d);
    return d;
}

// Sets a byte of a value at an offset.
static void Forge(void* value, size_t offset, uint8_t byte)
{
    ((uint8_t*)value)[offset] = byte;
}

static void TestCurrency(void)
{
    static mlocCurrencyFormatter made;
    static mlocCurrencyFormatter forged;
    char text[128];
    mlocOutput output = {text, sizeof(text), 0, nullptr, nullptr};
    mlocDecimal d = Value();
    bool ok = mlocInitCurrencyFormatter(&s_data, &s_en, "EUR", 3, nullptr, &made) == mloc_success;
    forged = made;
    CHECK(ok && mlocCurrencyFormatter_Format(&forged, &d, &output) == mloc_success,
          "a currency formatter the library made formats");
    const size_t flags[] = {offsetof(Currency, hasOuter), offsetof(Currency, appended),
                            offsetof(Currency, chain.complete), offsetof(Currency, chain.turkic)};
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
    {
        forged = made;
        Forge(&forged, flags[i], 2);
        CHECK(ok && mlocCurrencyFormatter_Format(&forged, &d, &output) == mloc_errorInvalid,
              "a currency formatter's bool byte of 2 is refused");
    }
}

static void TestUnit(void)
{
    static mlocUnitFormatter made;
    static mlocUnitFormatter forged;
    char text[128];
    mlocOutput output = {text, sizeof(text), 0, nullptr, nullptr};
    mlocDecimal d = Value();
    bool ok = mlocInitUnitFormatter(&s_data, &s_en, "meter", 5, nullptr, &made) == mloc_success;
    forged = made;
    CHECK(ok && mlocUnitFormatter_Format(&forged, &d, &output) == mloc_success,
          "a unit formatter the library made formats");
    const size_t flags[] = {offsetof(UnitFormat, percent), offsetof(UnitFormat, usage)};
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
    {
        forged = made;
        Forge(&forged, flags[i], 2);
        CHECK(ok && mlocUnitFormatter_Format(&forged, &d, &output) == mloc_errorInvalid,
              "a unit formatter's bool byte of 2 is refused");
    }
}

static void TestSpellout(void)
{
    static mlocSpelloutFormatter made;
    static mlocSpelloutFormatter forged;
    char text[128];
    mlocOutput output = {text, sizeof(text), 0, nullptr, nullptr};
    mlocDecimal d = Value();
    const char* rules = "%spellout-cardinal";
    bool ok = mlocInitSpelloutFormatter(&s_data, &s_en, rules, strlen(rules), nullptr, &made) ==
              mloc_success;
    forged = made;
    CHECK(ok && mlocSpelloutFormatter_Format(&forged, &d, &output) == mloc_success,
          "a spell-out formatter the library made formats");
    const size_t flags[] = {offsetof(SpelloutFormat, capitalize), offsetof(SpelloutFormat, turkic)};
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
    {
        forged = made;
        Forge(&forged, flags[i], 2);
        CHECK(ok && mlocSpelloutFormatter_Format(&forged, &d, &output) == mloc_errorInvalid,
              "a spell-out formatter's bool byte of 2 is refused");
    }
}

static void TestMessage(void)
{
    static mlocMessageFormatter made;
    static mlocMessageFormatter forged;
    static uint8_t compiled[256];
    alignas(8) static uint8_t bound[4096];
    char text[128];
    mlocOutput output = {text, sizeof(text), 0, nullptr, nullptr};
    size_t size = 0;
    size_t boundSize = 0;
    mlocMessageError error;
    bool ok = mlocInitMessageFormatter(&s_data, &s_en, nullptr, &made) == mloc_success &&
              mlocCompileMessage("Hello", 5, compiled, sizeof(compiled), &size, &error).status ==
                  mloc_success;
    // The bound message reads its formatter where it is, so the forged
    // copy is bound and each byte changed after.
    forged = made;
    ok = ok && mlocMessageFormatter_Bind(&forged, compiled, size, bound, sizeof(bound),
                                         &boundSize) == mloc_success;
    CHECK(ok && mlocFormatMessage(bound, nullptr, 0, &output, nullptr) == mloc_success,
          "a message formatter the library made formats");
    const size_t flags[] = {offsetof(MessageFormatter, strict), offsetof(MessageFormatter, isolate),
                            offsetof(MessageFormatter, hasZone)};
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
    {
        forged = made;
        Forge(&forged, flags[i], 2);
        CHECK(ok && mlocFormatMessage(bound, nullptr, 0, &output, nullptr) == mloc_errorInvalid,
              "a message formatter's bool byte of 2 is refused");
    }
    forged = made;
    MessageFormatter* stored = (MessageFormatter*)&forged;
    stored->direction = mloc_directionRightToLeft + 1;
    CHECK(ok && mlocFormatMessage(bound, nullptr, 0, &output, nullptr) == mloc_errorInvalid,
          "a message formatter's direction that is none is refused");
    forged = made;
    CHECK(ok && mlocFormatMessage(bound, nullptr, 0, &output, nullptr) == mloc_success,
          "put back, it formats again");
}

int main(void)
{
    mlocOpenBuiltinData(&s_sources[0]);
    if (!Open(MLOC_SPELLOUT_FILE, s_spellout, sizeof(s_spellout), &s_sources[1]) ||
        !Open(MLOC_UNITS_FILE, s_units, sizeof(s_units), &s_sources[2]) ||
        mlocParseLocale("en", 2, &s_en).status != mloc_success)
    {
        fputs("test_forged_formatters: cannot open " MLOC_SPELLOUT_FILE " or " MLOC_UNITS_FILE "\n",
              stderr);
        return 2;
    }
    TestCurrency();
    TestUnit();
    TestSpellout();
    TestMessage();
    return s_failures == 0 ? 0 : 1;
}
