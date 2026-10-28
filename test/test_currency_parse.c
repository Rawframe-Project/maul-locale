// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Currency parsing on the compiled-in data and the currency names data
// file: cases whose results ICU4C 79's DecimalFormat::parse and
// parseCurrency give alike (tools/oracle/parse compares over 500,000
// more), the round trip of each display in both modes, the names file's
// absence, and the refusals.

#include "test_harness.h"

#include "maul-locale/currency.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocDataSet s_data;
static mlocDataSet s_withNames;

typedef struct Case
{
    const char* id;
    const char* code;
    mlocParseMode mode;
    uint8_t any;
    const char* text;
    // ICU's value and currency, null for none.
    const char* expected;
    const char* currency;
    size_t end;
} Case;

static const Case s_cases[] = {
    {"ar", "EGP", mloc_parseLenient, 1,
     "\xE2\x80\x8F\xD9\xA1\xD9\xA2\xD9\xAB\xD9\xA5\xD9\xA0 \xD8\xAC.\xD9\x85.\xE2\x80\x8F", "1250",
     "EGP", 23},
    {"de-CH", "CHF", mloc_parseLenient, 0, "CHF 12.50", "12.5", "CHF", 9},
    {"de-CH", "CHF", mloc_parseLenient, 0, "CHF12.50", "12.5", "CHF", 8},
    {"de-CH", "CHF", mloc_parseStrict, 0, "CHF 12.50", nullptr, nullptr, 3},
    {"de", "EUR", mloc_parseLenient, 0, "12,50 \xE2\x82\xAC", "12.5", "EUR", 9},
    // ICU reads the code, "Eur", and ends at 9.
    {"de", "EUR", mloc_parseLenient, 0, "12,50 Euro", "12.5", "EUR", 10},
    {"de", "EUR", mloc_parseLenient, 1, "12,50 Schweizer Franken", "12.5", "CHF", 23},
    {"de", "EUR", mloc_parseLenient, 1, "12,50 SCHWEIZER FRANKEN", "12.5", "CHF", 23},
    {"de", "EUR", mloc_parseStrict, 0, "12,50 \xE2\x82\xAC", nullptr, nullptr, 5},
    {"de", "EUR", mloc_parseStrict, 0, "706\xC2\xA0\xE2\x82\xAC", "706", "EUR", 8},
    {"de", "EUR", mloc_parseStrict, 0, "706,00\xC2\xA0\xE2\x82\xAC", "706", "EUR", 11},
    {"de", "EUR", mloc_parseStrict, 0, "706,00\xE2\x82\xAC", nullptr, nullptr, 9},
    {"de", "EUR", mloc_parseStrict, 0,
     "706,00\xC2\xA0"
     "EUR",
     "706", "EUR", 11},
    {"el", "EUR", mloc_parseLenient, 1, "12,50 \xCE\xB5\xCF\x85\xCF\x81\xCF\x8E", "12.5", "EUR",
     14},
    {"el", "EUR", mloc_parseLenient, 1, "12,50 \xCE\x95\xCE\xA5\xCE\xA1\xCE\x8F", "12.5", "EUR",
     14},
    {"en", "CAD", mloc_parseLenient, 0, "$12", nullptr, nullptr, 0},
    {"en", "CAD", mloc_parseLenient, 1, "$12", "12", "USD", 3},
    {"en", "CAD", mloc_parseStrict, 0,
     "706\xC2\xA0"
     "CA$",
     nullptr, nullptr, 8},
    {"en", "CAD", mloc_parseStrict, 0, "706CA$", nullptr, nullptr, 6},
    {"en", "CAD", mloc_parseStrict, 0,
     "706\xC2\xA0"
     "CA$x",
     nullptr, nullptr, 8},
    {"en", "EUR", mloc_parseStrict, 0, "706 \xE2\x82\xAC", nullptr, nullptr, 3},
    {"en", "EUR", mloc_parseStrict, 0, "706\xC2\xA0\xE2\x82\xAC", nullptr, nullptr, 3},
    {"en", "EUR", mloc_parseStrict, 0, "706\xE2\x82\xAC", nullptr, nullptr, 6},
    {"en", "JPY", mloc_parseLenient, 0,
     "\xC2\xA5"
     "12",
     "12", "JPY", 4},
    {"en", "JPY", mloc_parseLenient, 0,
     "JP\xC2\xA5"
     "12",
     nullptr, nullptr, 0},
    {"en-u-cf-account", "USD", mloc_parseLenient, 0, "($12.50)", "-12.5", "USD", 8},
    {"en", "USD", mloc_parseLenient, 0, "($12.50)", nullptr, nullptr, 0},
    {"en", "USD", mloc_parseLenient, 0, "$12.50", "12.5", "USD", 6},
    {"en", "USD", mloc_parseLenient, 0,
     "\xE2\x82\xAC"
     "12.50",
     nullptr, nullptr, 0},
    {"en", "USD", mloc_parseLenient, 0, "(12.50)", nullptr, nullptr, 0},
    {"en", "USD", mloc_parseLenient, 0, "12.50 USD", "12.5", "USD", 9},
    {"en", "USD", mloc_parseLenient, 0, "12.50 US dollars", "12.5", "USD", 16},
    {"en", "USD", mloc_parseLenient, 0, "706\xC2\xA0$", "706", "USD", 6},
    {"en", "USD", mloc_parseLenient, 0, "US$12.50", nullptr, nullptr, 0},
    {"en", "USD", mloc_parseLenient, 0, "usd12.50", "12.5", "USD", 8},
    {"en", "USD", mloc_parseLenient, 0, "USD 12.50", "12.5", "USD", 9},
    {"en", "USD", mloc_parseLenient, 1,
     "\xEF\xBC\x84"
     "12",
     "12", "USD", 5},
    {"en", "USD", mloc_parseLenient, 1,
     "\xE2\x82\xAC"
     "12.50",
     "12.5", "EUR", 8},
    {"en", "USD", mloc_parseLenient, 1, "12.50", nullptr, nullptr, 5},
    {"en", "USD", mloc_parseLenient, 1, "12.50 euros", "12.5", "EUR", 11},
    {"en", "USD", mloc_parseLenient, 1, "12.50 EUROS", "12.5", "EUR", 11},
    {"en", "USD", mloc_parseLenient, 1, "12.50 us dollar", "12.5", "USD", 15},
    {"en", "USD", mloc_parseLenient, 1, "12.50 US DOLLARS", "12.5", "USD", 16},
    {"en", "USD", mloc_parseLenient, 1, "12 eur", nullptr, nullptr, 2},
    {"en", "USD", mloc_parseLenient, 1, "12 EUR", "12", "EUR", 6},
    {"en", "USD", mloc_parseLenient, 1, "12 Euro", "12", "EUR", 7},
    {"en", "USD", mloc_parseLenient, 1, "706\xC2\xA0\xE2\x82\xAC", "706", "EUR", 8},
    {"en", "USD", mloc_parseLenient, 1, "Rs12", "12", "INR", 4},
    {"en", "USD", mloc_parseLenient, 1,
     "\xE2\x82\xA8"
     "12",
     "12", "INR", 5},
    {"en", "USD", mloc_parseStrict, 0, "$12.50", "12.5", "USD", 6},
    {"en", "USD", mloc_parseStrict, 0,
     "$\xC2\xA0"
     "706",
     "706", "USD", 6},
    {"en", "USD", mloc_parseStrict, 0, "$706", "706", "USD", 4},
    {"en", "USD", mloc_parseStrict, 0, "12.50", nullptr, nullptr, 5},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0$", nullptr, nullptr, 3},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0$ ", nullptr, nullptr, 6},
    {"en", "USD", mloc_parseStrict, 0, "706$", nullptr, nullptr, 4},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0$$", nullptr, nullptr, 6},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0$1", nullptr, nullptr, 6},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0$x", nullptr, nullptr, 6},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0US$", nullptr, nullptr, 3},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0USD", nullptr, nullptr, 8},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0US dollars", nullptr, nullptr, 15},
    {"en", "USD", mloc_parseStrict, 0, "706\xC2\xA0USDx", nullptr, nullptr, 8},
    {"en", "USD", mloc_parseStrict, 1,
     "\xE2\x82\xAC"
     "12.50",
     "12.5", "EUR", 8},
    {"en", "USD", mloc_parseStrict, 1, "706\xC2\xA0$", nullptr, nullptr, 3},
    {"en", "USD", mloc_parseStrict, 1, "706$", nullptr, nullptr, 4},
    {"en", "USD", mloc_parseStrict, 1, "706\xC2\xA0\xC2\xA5", nullptr, nullptr, 3},
    {"en", "USD", mloc_parseStrict, 1, "706\xC2\xA5", nullptr, nullptr, 5},
    {"en", "USD", mloc_parseStrict, 1, "706\xC2\xA0\xE2\x82\xAC", nullptr, nullptr, 3},
    {"en", "USD", mloc_parseStrict, 1,
     "706\xC2\xA0"
     "euros",
     nullptr, nullptr, 10},
    {"en", "USD", mloc_parseStrict, 1, "706\xC2\xA0JPY", nullptr, nullptr, 8},
    {"en", "USD", mloc_parseStrict, 1, "706\xC2\xA0US$", nullptr, nullptr, 3},
    {"en", "USD", mloc_parseStrict, 1, "706US$", nullptr, nullptr, 3},
    {"fr", "EUR", mloc_parseLenient, 1, "12,50 \xE2\x82\xAC", "12.5", "EUR", 9},
    {"fr", "EUR", mloc_parseLenient, 1, "12,50 dollars des \xC3\x89tats-Unis", "12.5", "USD", 29},
    {"ig", "USD", mloc_parseLenient, 1, "706\xC2\xA0\xC2\xA5", "706", "JPY", 7},
    {"ig", "USD", mloc_parseStrict, 1, "706\xC2\xA0\xC2\xA5", nullptr, nullptr, 3},
    {"ig", "USD", mloc_parseStrict, 1, "706\xC2\xA5", nullptr, nullptr, 5},
    {"ja", "JPY", mloc_parseLenient, 0, "1,234\xE5\x86\x86", "1234", "JPY", 8},
    {"ja", "JPY", mloc_parseLenient, 1,
     "\xC2\xA5"
     "1,234",
     "1234", "JPY", 7},
    {"ja", "JPY", mloc_parseLenient, 1,
     "\xEF\xBF\xA5"
     "1,234",
     "1234", "JPY", 8},
    {"ja", "USD", mloc_parseLenient, 1, "1,234\xE5\x86\x86", "1234", "JPY", 8},
    {"pt-CV", "CVE", mloc_parseLenient, 0, "12$50", "12.5", "CVE", 5},
    {"pt-CV", "CVE", mloc_parseLenient, 0, "1 234$50 \xE2\x80\x8B", "1234.5", "CVE", 12},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 T\xC3\x9CRK LIRASI", nullptr, nullptr, 5},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 T\xC3\x9CRK L\xC4\xB0RASI", "12.5", "TRY", 19},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 t\xC3\xBCrk liras\xC4\xB1", "12.5", "TRY", 19},
    {"tr", "USD", mloc_parseLenient, 1, "12,50 ABD DOLARI", "12.5", "USD", 16},
    // Names of Adlam letters (supplementary code points with case), the
    // Turkish uppercasing (i to dotted I), root's equivalent signs.
    {"ff-Adlm", "GNF", mloc_parseLenient, 1,
     "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x92 "
     "\xF0\x9E\xA4\xAC\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xAA\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xB2\xF0\x9E\xA4"
     "\xB6\xF0\x9E\xA4\xAD "
     "\xF0\x9E\xA4\x98\xF0\x9E\xA4\xAD\xF0\x9E\xA4\xB2\xF0\x9E\xA4\xAB\xF0\x9E\xA4\xB2\xF0\x9E\xA4"
     "\xB3\xF0\x9E\xA4\xAE",
     "12", "GNF", 66},
    {"ff-Adlm", "USD", mloc_parseLenient, 1,
     "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x92 "
     "\xF0\x9E\xA4\x8A\xF0\x9E\xA4\x80\xF0\x9E\xA4\x88\xF0\x9E\xA4\x80\xF0\x9E\xA4\x90\xF0\x9E\xA4"
     "\x94\xF0\x9E\xA4\x8B "
     "\xF0\x9E\xA4\x98\xF0\x9E\xA4\x8B\xF0\x9E\xA4\x90\xF0\x9E\xA4\x89\xF0\x9E\xA4\x90\xF0\x9E\xA4"
     "\x91\xF0\x9E\xA4\x8C",
     "12", "GNF", 66},
    {"ff-Adlm", "GNF", mloc_parseLenient, 0,
     "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x92 "
     "\xF0\x9E\xA4\x8A\xF0\x9E\xA4\x80\xF0\x9E\xA4\x88\xF0\x9E\xA4\x80\xF0\x9E\xA4\x90\xF0\x9E\xA4"
     "\x94\xF0\x9E\xA4\x8B "
     "\xF0\x9E\xA4\x98\xF0\x9E\xA4\x8B\xF0\x9E\xA4\x90\xF0\x9E\xA4\x89\xF0\x9E\xA4\x90\xF0\x9E\xA4"
     "\x91\xF0\x9E\xA4\x8C",
     "12", "GNF", 8},
    {"ff-Adlm", "EUR", mloc_parseLenient, 0,
     "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x92.\xF0\x9E\xA5\x95\xF0\x9E\xA5\x90 "
     "\xF0\x9E\xA4\xB4\xF0\x9E\xA4\xB5\xF0\x9E\xA4\xAA\xF0\x9E\xA4\xAE\xF0\x9E\xA5\x85\xF0\x9E\xA4"
     "\xB6\xF0\x9E\xA4\xAD",
     "12.5", "EUR", 46},
    {"ff-Adlm", "EUR", mloc_parseLenient, 0,
     "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x92.\xF0\x9E\xA5\x95\xF0\x9E\xA5\x90 "
     "\xF0\x9E\xA4\x92\xF0\x9E\xA4\x93\xF0\x9E\xA4\x88\xF0\x9E\xA4\x8C\xF0\x9E\xA5\x85\xF0\x9E\xA4"
     "\x94\xF0\x9E\xA4\x8B",
     "12.5", "EUR", 17},
    {"ff-Adlm", "USD", mloc_parseLenient, 1,
     "\xF0\x9E\xA5\x91.\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90 "
     "\xF0\x9E\xA4\x81\xF0\x9E\xA4\x80\xF0\x9E\xA4\x82\xF0\x9E\xA4\x80 "
     "\xF0\x9E\xA4\x81\xF0\x9E\xA4\x89\xF0\x9E\xA4\x90\xF0\x9E\xA4\x9A\xF0\x9E\xA4\x80\xF0\x9E\xA4"
     "\x82 \xF0\x9E\xA4\x82\xF0\x9E\xA4\x80\xF0\x9E\xA4\x88\xF0\x9E\xA4\x89 "
     "\xF0\x9E\xA4\x80\xF0\x9E\xA4\x83\xF0\x9E\xA4\x89\xF0\x9E\xA4\x88\xF0\x9E\xA4\x8B\xF0\x9E\xA4"
     "\x91",
     "1", "USD", 97},
    {"ff-Adlm", "USD", mloc_parseLenient, 1,
     "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x92 "
     "\xF0\x9E\xA4\xAC\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xAA\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xB2\xF0\x9E\xA4"
     "\xB6\xF0\x9E\xA4\xAD "
     "\xF0\x9E\xA4\x98\xF0\x9E\xA4\xAD\xF0\x9E\xA4\xB2\xF0\x9E\xA4\xAB\xF0\x9E\xA4\xB2\xF0\x9E\xA4"
     "\xB3\xF0\x9E\xA4\xAE\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E"
     "\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0"
     "\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2"
     "\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4"
     "\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E"
     "\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0"
     "\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2"
     "\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4"
     "\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E"
     "\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0"
     "\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2"
     "\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4"
     "\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E"
     "\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0"
     "\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2"
     "\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4"
     "\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E"
     "\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0"
     "\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2"
     "\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4"
     "\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xA2\xF0\x9E"
     "\xA4\xA2",
     "12", "GNF", 66},
    {"ff-Adlm", "GNF", mloc_parseStrict, 1,
     "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x92 "
     "\xF0\x9E\xA4\xAC\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xAA\xF0\x9E\xA4\xA2\xF0\x9E\xA4\xB2\xF0\x9E\xA4"
     "\xB6\xF0\x9E\xA4\xAD "
     "\xF0\x9E\xA4\x98\xF0\x9E\xA4\xAD\xF0\x9E\xA4\xB2\xF0\x9E\xA4\xAB\xF0\x9E\xA4\xB2\xF0\x9E\xA4"
     "\xB3\xF0\x9E\xA4\xAE",
     nullptr, nullptr, 8},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 isvi\xC3\xA7re frang\xC4\xB1", "12.5", "CHF", 22},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 \xC4\xB0SV\xC4\xB0\xC3\x87RE FRANGI", "12.5", "CHF",
     23},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 ISVI\xC3\x87RE FRANGI", nullptr, nullptr, 5},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 Isvi\xC3\xA7re Frang\xC4\xB1", nullptr, nullptr, 5},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 ABD dolar\xC4\xB1", "12.5", "USD", 17},
    {"tr", "USD", mloc_parseStrict, 0, "12,50 ABD dolar\xC4\xB1", nullptr, nullptr, 5},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 abd dolari", nullptr, nullptr, 5},
    {"tr", "TRY", mloc_parseLenient, 1, "12,50 ABD DOLARI", "12.5", "USD", 16},
    {"tr", "USD", mloc_parseStrict, 0, "12,50 ABD DOLARI", nullptr, nullptr, 5},
    {"en", "EUR", mloc_parseLenient, 1,
     "\xEF\xBC\x84"
     "12",
     "12", "USD", 5},
    {"en", "EUR", mloc_parseLenient, 1,
     "\xEF\xBF\xA5"
     "12",
     "12", "JPY", 5},
    {"en", "EUR", mloc_parseLenient, 1,
     "\xE2\x82\xA8"
     "12",
     "12", "INR", 5},
    {"en", "EUR", mloc_parseLenient, 1, "Rs12", "12", "INR", 4},
    {"en", "EUR", mloc_parseLenient, 1, "Rs.12", "0.12", "INR", 5},
    {"en", "EUR", mloc_parseLenient, 1,
     "\xEF\xBF\xA1"
     "12",
     "12", "GBP", 5},
    {"en", "EUR", mloc_parseLenient, 1,
     "\xE2\x82\xA9"
     "12",
     "12", "KRW", 5},
    {"en", "EUR", mloc_parseLenient, 1,
     "US\xEF\xBC\x84"
     "12",
     nullptr, nullptr, 0},
    {"en", "EUR", mloc_parseLenient, 1, "12 \xEF\xBC\x84", "12", "USD", 6},
    // A sign equivalent to a two-letter symbol ("Rp", "Rs") reads as
    // that currency.
    {"id", "EUR", mloc_parseLenient, 1,
     "\xE2\x82\xA8"
     "12",
     "12", "IDR", 5},
    {"en-PK", "EUR", mloc_parseLenient, 1,
     "\xE2\x82\xA8"
     "12",
     "12", "PKR", 5},
    {"en-MU", "EUR", mloc_parseLenient, 1,
     "\xE2\x82\xA8"
     "12",
     "12", "MUR", 5},
    // A long name of the formatter's currency that the chain's search
    // leaves out (es-419 gives some of the unknown currency's plural
    // names).
    {"es-UY", "XXX", mloc_parseLenient, 1, "0,00 (moneda desconocida)", "0", "XXX", 25},
    // Text that ends with the spacing after a number, read to its end
    // and no further.
    {"en", "USD", mloc_parseLenient, 0, "12\xC2\xA0", "12", "USD", 2},
    {"en", "USD", mloc_parseLenient, 1, "12\xC2\xA0", nullptr, nullptr, 2},
    {"en", "USD", mloc_parseStrict, 0, "12\xC2\xA0", nullptr, nullptr, 2},
    {"en", "USD", mloc_parseStrict, 1, "12\xC2\xA0", nullptr, nullptr, 2},
};

static mlocResult Init(const mlocDataSet* data, const char* id, const char* code,
                       const mlocCurrencyOptions* options, mlocCurrencyFormatter* out)
{
    mlocLocale locale;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success)
    {
        return (mlocResult)-100;
    }
    return mlocInitCurrencyFormatter(out, data, &locale, code, 3, options);
}

// Parses from a copy of the text's exact length, so that the sanitizer
// builds catch a read past it.
static mlocResult ParseCopy(const mlocCurrencyFormatter* f, const char* text, size_t length,
                            mlocParseMode mode, uint8_t any, mlocDecimal* d, char* code,
                            size_t* end)
{
    char* copy = malloc(length == 0 ? 1 : length);
    if (copy == nullptr)
    {
        return (mlocResult)-100;
    }
    memcpy(copy, text, length);
    mlocResult r = mlocParseCurrency(f, copy, length, mode, any, d, code, end);
    free(copy);
    return r;
}

static bool SameValue(const mlocDecimal* a, const mlocDecimal* b)
{
    return a->kind == b->kind && a->negative == b->negative && a->count == b->count &&
           (a->count == 0 || a->exponent == b->exponent) &&
           memcmp(a->digits, b->digits, a->count) == 0;
}

static bool Parses(const Case* c)
{
    mlocCurrencyFormatter f;
    if (Init(&s_withNames, c->id, c->code, nullptr, &f) < 0)
    {
        return false;
    }
    mlocDecimal d;
    char code[3];
    size_t end = 0;
    mlocResult r = ParseCopy(&f, c->text, strlen(c->text), c->mode, c->any, &d, code, &end);
    if (c->expected == nullptr)
    {
        return r == mloc_errorSyntax && end == c->end;
    }
    mlocDecimal expected;
    return r == mloc_success && end == c->end &&
           mlocParseDecimal(c->expected, strlen(c->expected), &expected).status == mloc_success &&
           SameValue(&d, &expected) && memcmp(code, c->currency, 3) == 0;
}

static void TestCases(void)
{
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        if (!Parses(&s_cases[i]))
        {
            CHECK(false, s_cases[i].text);
        }
    }
}

// Formats an amount with a display, with or without the ISO append, and
// reads the text back, all of it, in both modes, with and without any
// currency, to the same decimal and the formatter's currency.
static bool RoundTrips(const char* id, const char* code, mlocCurrencyDisplay display,
                       uint8_t append, const char* amount)
{
    mlocCurrencyOptions options = {.display = display, .appendCode = append};
    mlocCurrencyFormatter f;
    mlocDecimal d;
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    if (Init(&s_withNames, id, code, &options, &f) < 0 ||
        mlocParseDecimal(amount, strlen(amount), &d).status != mloc_success ||
        mlocFormatCurrency(&f, &d, &output) != mloc_success)
    {
        return false;
    }
    for (int mode = mloc_parseLenient; mode <= mloc_parseStrict; mode++)
    {
        for (uint8_t any = 0; any <= 1; any++)
        {
            mlocDecimal back;
            char read[3];
            size_t end = 0;
            if (ParseCopy(&f, buffer, output.length, (mlocParseMode)mode, any, &back, read, &end) !=
                    mloc_success ||
                end != output.length || !SameValue(&back, &d) || memcmp(read, code, 3) != 0)
            {
                return false;
            }
        }
    }
    return true;
}

static void TestRoundTrip(void)
{
    // my, si and to write a long name before the number; ro writes
    // "12,00 euro" and "20,00 de euro".
    static const char* const s_locales[] = {"en",    "de", "fr", "ja", "hi", "ar-EG", "fa", "de-CH",
                                            "pt-CV", "tr", "ru", "zh", "ko", "es",    "sv", "he",
                                            "bn",    "th", "pl", "it", "my", "si",    "to", "ro"};
    static const char* const s_codes[] = {"USD", "EUR", "JPY", "CHF", "INR", "CVE"};
    static const mlocCurrencyDisplay s_displays[] = {mloc_currencySymbol, mloc_currencyNarrowSymbol,
                                                     mloc_currencyCode, mloc_currencyName};
    for (size_t l = 0; l < sizeof(s_locales) / sizeof(s_locales[0]); l++)
    {
        for (size_t c = 0; c < sizeof(s_codes) / sizeof(s_codes[0]); c++)
        {
            for (size_t d = 0; d < sizeof(s_displays) / sizeof(s_displays[0]) * 2; d++)
            {
                // The yen has no fraction digits.
                const char* amount = strcmp(s_codes[c], "JPY") == 0 ? "-1234" : "-1234.5";
                mlocCurrencyDisplay display = s_displays[d / 2];
                uint8_t append = (uint8_t)(d % 2);
                // The ISO append goes with a symbol alone.
                if (append == 1 && display != mloc_currencySymbol &&
                    display != mloc_currencyNarrowSymbol)
                {
                    continue;
                }
                if (!RoundTrips(s_locales[l], s_codes[c], display, append, amount) ||
                    !RoundTrips(s_locales[l], s_codes[c], display, append, "12") ||
                    !RoundTrips(s_locales[l], s_codes[c], display, append, "20"))
                {
                    CHECK(false, s_locales[l]);
                }
            }
        }
    }
}

// With the ISO append and a plus sign shown, NaN (and zero, except zero)
// is written without the sign; the code after it is read back too, also
// where the sign is a suffix (fy: "£ NaN GBP" beside "£ 1,00+ GBP").
// Strict mode wants the plus sign of the always display, which NaN lacks.
static bool AppendedReadsBack(const char* id, const char* code, mlocCurrencyDisplay display,
                              mlocSignDisplay sign, const mlocDecimal* d, mlocParseMode lastMode)
{
    mlocCurrencyOptions options = {
        .display = display, .appendCode = 1, .number = {.signDisplay = sign}};
    mlocCurrencyFormatter f;
    char buffer[256];
    char again[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    if (Init(&s_withNames, id, code, &options, &f) < 0 ||
        mlocFormatCurrency(&f, d, &output) != mloc_success)
    {
        return false;
    }
    for (int mode = mloc_parseLenient; mode <= (int)lastMode; mode++)
    {
        mlocDecimal back;
        char read[3];
        size_t end = 0;
        mlocOutput second = {.buffer = again, .capacity = sizeof(again)};
        if (ParseCopy(&f, buffer, output.length, (mlocParseMode)mode, 0, &back, read, &end) !=
                mloc_success ||
            end != output.length || mlocFormatCurrency(&f, &back, &second) != mloc_success ||
            second.length != output.length || memcmp(buffer, again, output.length) != 0)
        {
            return false;
        }
    }
    return true;
}

static bool Appended(const char* id, mlocSignDisplay sign, const mlocDecimal* d,
                     mlocParseMode lastMode)
{
    return AppendedReadsBack(id, "GBP", mloc_currencySymbol, sign, d, lastMode);
}

static void TestAppendedUnsigned(void)
{
    static const char* const s_locales[] = {"fy", "en", "de", "nl", "he", "fa"};
    mlocDecimal zero;
    mlocDecimal nan;
    memset(&nan, 0, sizeof(nan));
    nan.kind = mloc_decimalNaN;
    bool all = mlocParseDecimal("0", 1, &zero).status == mloc_success;
    for (size_t l = 0; l < sizeof(s_locales) / sizeof(s_locales[0]); l++)
    {
        const char* id = s_locales[l];
        all = all && Appended(id, mloc_signAlways, &nan, mloc_parseLenient) &&
              Appended(id, mloc_signAlways, &zero, mloc_parseStrict) &&
              Appended(id, mloc_signExceptZero, &nan, mloc_parseStrict) &&
              Appended(id, mloc_signExceptZero, &zero, mloc_parseStrict);
    }
    CHECK(all, "NaN and zero without a sign, then the appended code, read back");
}

// fr-CA writes USD's narrow symbol as "$" and its symbol as "$ US": with
// the ISO append, "0,00 $ USD" reads the narrow symbol, not "$ US" with
// the code's "D" left over. The symbol display writes "$ US USD".
static void TestAppendedShownSymbol(void)
{
    mlocDecimal d;
    bool all = mlocParseDecimal("1234.5", 6, &d).status == mloc_success;
    static const mlocCurrencyDisplay s_displays[] = {mloc_currencyNarrowSymbol,
                                                     mloc_currencySymbol};
    for (size_t i = 0; i < sizeof(s_displays) / sizeof(s_displays[0]); i++)
    {
        all = all &&
              AppendedReadsBack("fr-CA", "USD", s_displays[i], mloc_signAuto, &d, mloc_parseStrict);
    }
    CHECK(all, "the narrow symbol, then the appended code, read back");

    // The narrow formatter still reads the symbol where the code does not
    // follow the narrow symbol ("$ US USD" whole), and the narrow symbol
    // ending the text.
    mlocCurrencyOptions narrow = {.display = mloc_currencyNarrowSymbol, .appendCode = 1};
    mlocCurrencyOptions symbol = {.display = mloc_currencySymbol, .appendCode = 1};
    mlocCurrencyFormatter n;
    mlocCurrencyFormatter w;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocDecimal back;
    char read[3];
    size_t end = 0;
    all = Init(&s_withNames, "fr-CA", "USD", &narrow, &n) >= 0 &&
          Init(&s_withNames, "fr-CA", "USD", &symbol, &w) >= 0 &&
          mlocFormatCurrency(&w, &d, &output) == mloc_success &&
          ParseCopy(&n, text, output.length, mloc_parseLenient, 0, &back, read, &end) ==
              mloc_success &&
          end == output.length && memcmp(read, "USD", 3) == 0;
    static const char s_ending[] = "0,00 $";
    all = all &&
          ParseCopy(&n, s_ending, sizeof(s_ending) - 1, mloc_parseLenient, 0, &back, read, &end) ==
              mloc_success &&
          end == sizeof(s_ending) - 1;
    CHECK(all, "the symbol where the code does not follow the narrow symbol, and one ending");
}

// Strict mode wants a long name formatter's name, in each category's
// pattern.
static bool ReadsStrictly(const char* id, const char* code, const char* text, bool reads)
{
    mlocCurrencyOptions options = {.display = mloc_currencyName};
    mlocCurrencyFormatter f;
    mlocDecimal d;
    char read[3];
    size_t at = 0;
    mlocResult r = Init(&s_withNames, id, code, &options, &f) < 0
                       ? (mlocResult)-100
                       : ParseCopy(&f, text, strlen(text), mloc_parseStrict, 0, &d, read, &at);
    return reads ? r == mloc_success && at == strlen(text) : r == mloc_errorSyntax;
}

// The code display writes "USD 12.50" (the alphaNextToNumber pattern)
// but parses with the standard pattern, as ICU's ISO currency formatter
// does: strict mode wants the code next to the digits.
static void TestCodeDisplayStrictly(void)
{
    mlocCurrencyOptions options = {.display = mloc_currencyCode};
    mlocCurrencyFormatter f;
    mlocDecimal d;
    char code[3];
    size_t spaced = 0;
    size_t joined = 0;
    CHECK(Init(&s_withNames, "en", "USD", &options, &f) == mloc_success &&
              ParseCopy(&f, "USD 12.50", 9, mloc_parseStrict, 0, &d, code, &spaced) ==
                  mloc_errorSyntax &&
              spaced == 3 &&
              ParseCopy(&f, "USD12.50", 8, mloc_parseStrict, 0, &d, code, &joined) ==
                  mloc_success &&
              joined == 8,
          "the standard pattern's affixes");
}

static void TestLongNamesStrictly(void)
{
    CHECK(ReadsStrictly("en", "USD", "12.50", false), "no name");
    CHECK(ReadsStrictly("en", "USD", "12.50 US dollars", true), "the name");
    CHECK(ReadsStrictly("ro", "EUR", "20,00 euro", true), "few's pattern");
    CHECK(ReadsStrictly("ro", "EUR", "6,00 de euro", true), "other's pattern");
    CHECK(ReadsStrictly("ro", "EUR", "6,00", false), "none");
}

static void TestWithoutNames(void)
{
    mlocCurrencyFormatter f;
    mlocDecimal d;
    char code[3];
    size_t end = 0;
    CHECK(Init(&s_data, "en", "USD", nullptr, &f) == mloc_success &&
              mlocParseCurrency(&f, "12.50 US dollars", 16, mloc_parseLenient, 0, &d, code, &end) ==
                  mloc_success &&
              end == 5 && memcmp(code, "USD", 3) == 0,
          "without the names file the long names are not read");
    CHECK(mlocParseCurrency(&f, "12.50 USD", 9, mloc_parseLenient, 0, &d, code, &end) ==
                  mloc_success &&
              end == 9,
          "the code is");
    CHECK(mlocParseCurrency(&f, "12.50 euros", 11, mloc_parseLenient, 1, &d, code, &end) ==
                  mloc_success &&
              end == 9 && memcmp(code, "EUR", 3) == 0 &&
              mlocParseCurrency(&f,
                                "\xE2\x82\xAC"
                                "12.50",
                                8, mloc_parseLenient, 1, &d, code, &end) == mloc_success &&
              memcmp(code, "EUR", 3) == 0,
          "any currency by its code and symbol, not its name");
}

// Parsing any currency reads at most MLOC_MAX_CURRENCY_PARSE_UNITS units
// of a kind along a locale's chain: five copies of the built-in data give
// "en" ten, en's and root's in each. Parsing one currency reads none.
static void TestChainLimit(void)
{
    mlocData sources[5];
    for (int i = 0; i < 5; i++)
    {
        sources[i] = s_data.sources[0];
    }
    mlocDataSet copies = {sources, 5};
    mlocCurrencyFormatter f;
    mlocDecimal d;
    char code[3];
    size_t end = 0;
    CHECK(Init(&copies, "en", "USD", nullptr, &f) == mloc_success &&
              mlocParseCurrency(&f, "$1", 2, mloc_parseLenient, 1, &d, code, &end) ==
                  mloc_errorLimit &&
              mlocParseCurrency(&f, "$1", 2, mloc_parseLenient, 0, &d, code, &end) == mloc_success,
          "any currency along a chain of more units than the bound is refused");
}

static void TestRefusals(void)
{
    mlocCurrencyFormatter f;
    mlocDecimal d;
    char code[3];
    size_t end = 0;
    CHECK(Init(&s_withNames, "en", "USD", nullptr, &f) == mloc_success, "a formatter");
    CHECK(mlocParseCurrency(nullptr, "1", 1, mloc_parseLenient, 0, &d, code, &end) ==
                  mloc_errorInvalid &&
              mlocParseCurrency(&f, nullptr, 1, mloc_parseLenient, 0, &d, code, &end) ==
                  mloc_errorInvalid &&
              mlocParseCurrency(&f, "1", 1, mloc_parseLenient, 0, nullptr, code, &end) ==
                  mloc_errorInvalid &&
              mlocParseCurrency(&f, "1", 1, mloc_parseLenient, 0, &d, nullptr, &end) ==
                  mloc_errorInvalid &&
              mlocParseCurrency(&f, "1", 1, mloc_parseLenient, 0, &d, code, nullptr) ==
                  mloc_errorInvalid,
          "null pointers");
    CHECK(mlocParseCurrency(&f, "1", 1, (mlocParseMode)2, 0, &d, code, &end) == mloc_errorInvalid &&
              mlocParseCurrency(&f, "1", 1, mloc_parseLenient, 2, &d, code, &end) ==
                  mloc_errorInvalid,
          "a mode or flag out of range");
    mlocCurrencyFormatter zero = {0};
    CHECK(mlocParseCurrency(&zero, "1", 1, mloc_parseLenient, 0, &d, code, &end) ==
              mloc_errorInvalid,
          "a formatter the library did not make");
    mlocCurrencyOptions compact = {.number = {.notation = mloc_notationCompactShort}};
    CHECK(Init(&s_withNames, "en", "USD", &compact, &f) == mloc_success &&
              mlocParseCurrency(&f, "$1K", 3, mloc_parseLenient, 0, &d, code, &end) ==
                  mloc_errorInvalid,
          "compact notation is not read");
    CHECK(Init(&s_withNames, "en", "USD", nullptr, &f) == mloc_success &&
              mlocParseCurrency(&f, nullptr, 0, mloc_parseLenient, 1, &d, code, &end) ==
                  mloc_errorSyntax &&
              end == 0,
          "no text is no amount");
}

static bool OpenData(void)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_names[4 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 1};
    FILE* file = fopen(MLOC_NAMES_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(s_names, 1, sizeof(s_names), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_withNames = (mlocDataSet){s_sources, 2};
    return size > 0 && size < sizeof(s_names) &&
           mlocOpenData(&s_sources[1], s_names, size) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_currency_parse: cannot open " MLOC_NAMES_FILE "\n", stderr);
        return 2;
    }
    TestCases();
    TestRoundTrip();
    TestAppendedUnsigned();
    TestAppendedShownSymbol();
    TestLongNamesStrictly();
    TestCodeDisplayStrictly();
    TestWithoutNames();
    TestChainLimit();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
