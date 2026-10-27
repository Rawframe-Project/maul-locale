// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number functions of messages (maul-locale/message.h): :number,
// :integer, :offset and :percent formatting with ECMA-402's defaults and
// options, the operands and option values they take and refuse, options
// passed on from a number value, and selection by exact value and plural
// category, with the faults of each.

#include "message_number.h"
#include "message_prepare.h"
#include "test_harness.h"

#include "maul-locale/message.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocMessageFormatter s_en;
static mlocMessageFormatter s_fr;
static mlocMessageFormatter s_ar;
static const mlocMessageFormatter* s_using = &s_en;
static uint8_t s_compiled[1 << 16];
static uint8_t s_bound[1 << 16];
static char s_text[1 << 12];

static void Make(mlocMessageFormatter* formatter, const char* tag, mlocMessageIsolation isolation)
{
    mlocLocale locale;
    mlocMessageOptions options = {false, isolation, NULL, 0, NULL};
    CHECK(mlocParseLocale(tag, strlen(tag), &locale).status == mloc_success &&
              mlocInitMessageFormatter(formatter, &s_data, &locale, &options) == mloc_success,
          "a formatter is made");
}

// Whether a message formats to a text with the faults given in order
// (0 ending the list).
static bool Formats(const char* message, const mlocMessageArgument* arguments, size_t count,
                    const char* text, const mlocMessageFaultKind* expected)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    if (mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocBindMessage(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) != mloc_success)
    {
        printf("  [%s] does not compile and bind\n", message);
        return false;
    }
    mlocMessageFault list[8];
    mlocMessageFaults faults = {list, 8, 0};
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    mlocResult result = mlocFormatMessage(s_bound, arguments, count, &output, &faults);
    size_t expectedCount = 0;
    while (expected != NULL && expected[expectedCount] != mloc_messageFaultNone)
    {
        expectedCount++;
    }
    bool good = result == (expectedCount > 0 ? mloc_withFallbacks : mloc_success) &&
                output.length == strlen(text) && memcmp(s_text, text, output.length) == 0 &&
                faults.count == expectedCount;
    for (size_t i = 0; good && i < expectedCount; i++)
    {
        good = list[i].kind == expected[i];
    }
    if (!good)
    {
        printf("  [%s] -> %d [%.*s], %zu faults", message, (int)result, (int)output.length, s_text,
               faults.count);
        for (size_t i = 0; i < faults.count && i < 8; i++)
        {
            printf(" %d", (int)list[i].kind);
        }
        printf("\n");
    }
    return good;
}

static const mlocMessageFaultKind s_badOption[] = {mloc_messageFaultBadOption, 0};
static const mlocMessageFaultKind s_badOperand[] = {mloc_messageFaultBadOperand, 0};

static void TestFormatting(void)
{
    CHECK(Formats("{4.2 :number} {-4.20 :number} {1.5e-3 :number} {1234.5 :number}", NULL, 0,
                  "4.2 -4.2 0.002 1,234.5", NULL),
          ":number rounds to three fraction digits, ECMA-402's default, and groups");
    CHECK(Formats("{4.5 :integer} {2.5 :integer} {-4.5 :integer} {4.4 :integer} {-0.4 :integer}",
                  NULL, 0, "5 3 -5 4 -0", NULL),
          ":integer rounds half away from zero, as :number's default");
    CHECK(Formats("{4.2 :number minimumFractionDigits=2} {1234.5678 :number "
                  "maximumSignificantDigits=2} {5 :number signDisplay=always} "
                  "{1234 :number useGrouping=never} {5 :number minimumIntegerDigits=3}",
                  NULL, 0, "4.20 1,200 +5 1234 005", NULL),
          "fraction, significant and integer digits, sign and grouping");
    CHECK(Formats("{1.29 :number maximumFractionDigits=1 roundingMode=floor} "
                  "{12.345 :number roundingIncrement=5 minimumFractionDigits=2 "
                  "maximumFractionDigits=2} {1.5 :number minimumFractionDigits=2 "
                  "trailingZeroDisplay=stripIfInteger} {1 :number minimumFractionDigits=2 "
                  "trailingZeroDisplay=stripIfInteger}",
                  NULL, 0, "1.2 12.35 1.50 1", NULL),
          "rounding mode, increment and trailing zeros");
    CHECK(Formats("{1.23456 :number maximumFractionDigits=1 maximumSignificantDigits=3 "
                  "roundingPriority=morePrecision} {1.23456 :number maximumFractionDigits=1 "
                  "maximumSignificantDigits=3 roundingPriority=lessPrecision}",
                  NULL, 0, "1.23 1.2", NULL),
          "the rounding priority");
    CHECK(Formats("{1234 :integer maximumSignificantDigits=2} {7 :integer minimumIntegerDigits=2 "
                  "minimumFractionDigits=2}",
                  NULL, 0, "1,200 07", NULL),
          ":integer takes significant digits and ignores fraction digits");
    s_using = &s_fr;
    CHECK(Formats("{1234.5 :number}", NULL, 0,
                  "1\xE2\x80\xAF"
                  "234,5",
                  NULL),
          "the locale's symbols and grouping");
    s_using = &s_en;
}

static void TestOptions(void)
{
    CHECK(Formats("{4.2 :number minimumFractionDigits=100}", NULL, 0, "4.2", s_badOption) &&
              Formats("{4.2 :number minimumFractionDigits=02}", NULL, 0, "4.2", s_badOption) &&
              Formats("{4.2 :number minimumFractionDigits=-1}", NULL, 0, "4.2", s_badOption),
          "a digit size is 0 or one or two digits");
    CHECK(Formats("{1 :number minimumFractionDigits=3 maximumFractionDigits=1}", NULL, 0, "1.000",
                  s_badOption),
          "a maximum under its minimum is dropped");
    CHECK(Formats("{1 :number maximumSignificantDigits=0}", NULL, 0, "1", s_badOption) &&
              Formats("{1 :number minimumSignificantDigits=22}", NULL, 0, "1", s_badOption) &&
              Formats("{1 :number minimumIntegerDigits=22}", NULL, 0, "1", s_badOption) &&
              Formats("{1 :number minimumSignificantDigits=3 maximumSignificantDigits=2}", NULL, 0,
                      "1.00", s_badOption),
          "significant and integer digits past the number part's limits");
    CHECK(Formats("{12 :number roundingIncrement=3}", NULL, 0, "12", s_badOption) &&
              Formats("{12.3 :number roundingIncrement=5 maximumFractionDigits=2}", NULL, 0, "12.3",
                      s_badOption) &&
              Formats("{12.3 :number roundingIncrement=5}", NULL, 0, "10", NULL),
          "an increment needs equal fraction limits");
    CHECK(Formats("{1 :number signDisplay=up}", NULL, 0, "1", s_badOption) &&
              Formats("{1 :number roundingMode=up}", NULL, 0, "1", s_badOption) &&
              Formats("{1 :number select=up}", NULL, 0, "1", s_badOption) &&
              Formats("{1 :number frobnicate=up @attr=1}", NULL, 0, "1", NULL),
          "a bad value is a bad option; an unknown option and attributes are ignored");
    CHECK(Formats("{1 :number select=plural signDisplay=auto useGrouping=auto "
                  "trailingZeroDisplay=auto roundingPriority=auto roundingMode=halfExpand}",
                  NULL, 0, "1", NULL),
          "each option's first value is taken");
    mlocDecimal two;
    mlocDecimal half;
    CHECK(mlocParseDecimal("2", 1, &two).status == mloc_success &&
              mlocParseDecimal("2.5", 3, &half).status == mloc_success,
          "decimals parse");
    mlocMessageArgument values[5] = {
        {"i", 1, {mloc_messageValueInteger, 0, {.integer = 2}}},
        {"s", 1, {mloc_messageValueString, 0, {.string = {"2", 1}}}},
        {"d", 1, {mloc_messageValueDouble, 0, {.number = 2.0}}},
        {"m", 1, {mloc_messageValueDecimal, 0, {.decimal = &two}}},
        {"h", 1, {mloc_messageValueDecimal, 0, {.decimal = &half}}},
    };
    CHECK(Formats("{1 :number minimumFractionDigits=$i} {1 :number minimumFractionDigits=$s} "
                  "{1 :number minimumFractionDigits=$d} {1 :number minimumFractionDigits=$m}",
                  values, 5, "1.00 1.00 1.00 1.00", NULL),
          "a digit size from an integer, a string, a whole double or decimal");
    CHECK(Formats(".local $n = {2 :number} {{{1 :number minimumFractionDigits=$n}}}", values, 5,
                  "1.00", NULL) &&
              Formats("{1 :number minimumFractionDigits=$h}", values, 5, "1", s_badOption),
          "from a number value; not from a fraction");
    int object = 1;
    mlocMessageArgument limits[2] = {
        {"n", 1, {mloc_messageValueInteger, 0, {.integer = 99}}},
        {"o", 1, {mloc_messageValueObject, 1, {.object = &object}}},
    };
    CHECK(Formats("{1 :number maximumFractionDigits=$n}", limits, 2, "1", NULL) &&
              Formats("{1 :number minimumFractionDigits=$o}", limits, 2, "1", s_badOption),
          "99 is a digit size; an object is none");
}

static void TestOperands(void)
{
    static const mlocMessageFaultKind unsupported[] = {mloc_messageFaultUnsupportedOperation, 0};
    static const mlocMessageFaultKind unresolved[] = {mloc_messageFaultUnresolvedVariable,
                                                      mloc_messageFaultBadOperand, 0};
    CHECK(Formats("{00 :number}", NULL, 0, "{|00|}", s_badOperand) &&
              Formats("{|+1| :number}", NULL, 0, "{|+1|}", s_badOperand) &&
              Formats("{:integer}", NULL, 0, "{:integer}", s_badOperand),
          "a literal that is no number literal, or none");
    CHECK(Formats("{1e1000000 :number}", NULL, 0, "{|1e1000000|}", unsupported),
          "a literal past the decimal's reach is unsupported");
    CHECK(Formats("{$x :number}", NULL, 0, "{$x}", unresolved),
          "an unresolved variable is a bad operand too");
    int object = 7;
    mlocMessageArgument values[5] = {
        {"s", 1, {mloc_messageValueString, 0, {.string = {"-12.5", 5}}}},
        {"i", 1, {mloc_messageValueInteger, 0, {.integer = -9223372036854775807 - 1}}},
        {"d", 1, {mloc_messageValueDouble, 0, {.number = 0.1}}},
        {"o", 1, {mloc_messageValueObject, 1, {.object = &object}}},
        {"f", 1, {mloc_messageValueDouble, 0, {.number = (double)INFINITY}}},
    };
    CHECK(Formats("{$s :number} {$i :integer} {$d :number}", values, 5,
                  "-12.5 -9,223,372,036,854,775,808 0.1", NULL),
          "a string number, an integer and a double");
    CHECK(Formats("{$o :number}", values, 5, "{$o}", s_badOperand), "an object is no number");
    CHECK(Formats(".local $t = {|12| :string} {{{$t :number}}}", values, 5, "12", NULL) &&
              Formats(".local $n = {12 :number} {{{$n :string}}}", values, 5, "{$n}", s_badOperand),
          ":string's text is a number; a number is not :string's operand");
    CHECK(Formats("{$f :number}", values, 5, "\xE2\x88\x9E", NULL), "infinity formats");
}

static void TestInheritance(void)
{
    mlocMessageArgument n = {"n", 1, {mloc_messageValueInteger, 0, {.integer = 3}}};
    CHECK(Formats(".input {$n :number minimumFractionDigits=2 signDisplay=always} "
                  "{{{$n :number minimumFractionDigits=1} {$n}}}",
                  &n, 1, "+3.0 +3.00", NULL),
          "a number value's options pass on, the expression's first");
    CHECK(Formats(".local $x = {1.234 :number maximumFractionDigits=1 signDisplay=always} "
                  "{{{$x :integer} {$x :number}}}",
                  NULL, 0, "+1 +1.2", NULL),
          ":integer drops the operand's fraction digits and rounds its value");
    static const mlocMessageFaultKind select[] = {mloc_messageFaultBadOption,
                                                  mloc_messageFaultBadSelector, 0};
    CHECK(Formats(".local $a = {1 :number select=exact} .local $b = {$a :integer} "
                  ".match $b 1 {{one}} * {{other {$b}}}",
                  NULL, 0, "other 1", select),
          "a select passed on is a bad option, and the value cannot select");
}

// Fills a decimal's padding, the bytes past its last field, with a
// byte: a caller's decimal may hold anything there.
static void Pad(mlocDecimal* d, uint8_t byte)
{
    for (size_t i = offsetof(mlocDecimal, kind) + 1; i < sizeof(*d); i++)
    {
        ((uint8_t*)d)[i] = byte;
    }
}

static void TestSelection(void)
{
    CHECK(Formats(".local $n = {1 :number} .match $n one {{one}} 1 {{=1}} * {{other}}", NULL, 0,
                  "=1", NULL) &&
              Formats(".local $n = {2 :number} .match $n one {{one}} 1 {{=1}} * {{other}}", NULL, 0,
                      "other", NULL),
          "an exact key beats a keyword");
    CHECK(Formats(".local $n = {1e3 :number} .match $n 1e3 {{e}} 1000 {{=1000}} * {{other}}", NULL,
                  0, "=1000", NULL) &&
              Formats(".local $n = {1.50 :number} .match $n 1.50 {{a}} 1.5 {{b}} * {{other}}", NULL,
                      0, "b", NULL) &&
              Formats(".local $n = {1 :number minimumFractionDigits=1} .match $n 1 {{a}} 1.0 "
                      "{{b}} one {{one}} * {{other}}",
                      NULL, 0, "b", NULL),
          "the exact value as it formats, written plainly");
    CHECK(Formats(".local $n = {1 :number minimumFractionDigits=1} .match $n one {{one}} * "
                  "{{other}}",
                  NULL, 0, "other", NULL) &&
              Formats(".local $n = {-0.4 :integer} .match $n -0 {{minus}} 0 {{zero}} * {{other}}",
                      NULL, 0, "zero", NULL),
          "1.0 is other in English; zero is unsigned");
    CHECK(Formats(".local $n = {2 :number select=ordinal} .match $n one {{one}} two {{two}} * "
                  "{{other}}",
                  NULL, 0, "two", NULL) &&
              Formats(".local $n = {1 :number select=exact} .match $n one {{one}} * {{other}}",
                      NULL, 0, "other", NULL),
          "ordinal categories; exact selection ignores keywords");
    static const mlocMessageFaultKind key[] = {mloc_messageFaultBadVariantKey, 0};
    CHECK(
        Formats(".local $n = {1 :number} .match $n foo {{foo}} * {{other}}", NULL, 0, "other", key),
        "a key neither literal nor keyword is a bad variant key");
    static const mlocMessageFaultKind variable[] = {mloc_messageFaultBadOption,
                                                    mloc_messageFaultBadSelector, 0};
    mlocMessageArgument exact = {"e", 1, {mloc_messageValueString, 0, {.string = {"exact", 5}}}};
    CHECK(Formats(".local $n = {1 :number select=$e} .match $n 1 {{one}} * {{other}}", &exact, 1,
                  "other", variable),
          "select from a variable cannot select");
    static const mlocMessageFaultKind missing[] = {mloc_messageFaultUnresolvedVariable,
                                                   mloc_messageFaultBadOption,
                                                   mloc_messageFaultBadSelector, 0};
    static const mlocMessageFaultKind other[] = {mloc_messageFaultUnresolvedVariable,
                                                 mloc_messageFaultBadOption, 0};
    CHECK(Formats(".local $n = {1 :number select=$missing} .match $n 1 {{one}} * {{other}}", NULL,
                  0, "other", missing) &&
              Formats(".local $n = {1 :number selectX=$missing} .match $n 1 {{one}} * {{other}}",
                      NULL, 0, "one", other),
          "a select whose variable failed cannot select; another option can fail");
    char text[256];
    snprintf(text, sizeof text, ".local $n = {1e60 :number} .match $n 1%060d {{big}} * {{other}}",
             0);
    CHECK(Formats(text, NULL, 0, "big", NULL), "a long key of few digits matches");
    mlocDecimal one;
    CHECK(mlocParseDecimal("1", 1, &one).status == mloc_success, "1 parses");
    Pad(&one, 0xA5);
    mlocMessageArgument padded = {"n", 1, {mloc_messageValueDecimal, 0, {.decimal = &one}}};
    CHECK(Formats(".input {$n :number} .match $n 1 {{=1}} * {{other}}", &padded, 1, "=1", NULL),
          "a decimal argument of any padding matches its exact key");
    // The exact key itself, given that decimal: it compares by fields.
    CHECK(mlocExactlyKey(&one, (const uint8_t*)"1", 1), "the exact key ignores padding");
}

static void TestOffset(void)
{
    CHECK(Formats("{41 :offset add=1} {1 :offset subtract=3} {-1 :offset add=1}", NULL, 0,
                  "42 -2 0", NULL) &&
              Formats("{0.25 :offset subtract=1} {41 :offset add=1 foo=13}", NULL, 0, "-0.75 42",
                      NULL),
          "an offset adds or subtracts, across zero, other options ignored");
    CHECK(Formats(".local $x = {1.5 :number minimumFractionDigits=2 signDisplay=always} "
                  "{{{$x :offset add=1}}}",
                  NULL, 0, "+2.50", NULL) &&
              Formats(".local $x = {0.5 :percent} {{{$x :offset add=1}}}", NULL, 0, "150%", NULL),
          "the value keeps its operand's options and function");
    mlocMessageArgument arguments[] = {
        {"n", 1, {mloc_messageValueInteger, 0, {.integer = 5}}},
        {"d", 1, {mloc_messageValueString, 0, {.string = {"2", 1}}}},
        {"x", 1, {mloc_messageValueString, 0, {.string = {"x", 1}}}},
    };
    CHECK(Formats("{$n :offset subtract=1} {$n :offset add=$d}", arguments, 3, "4 7", NULL),
          "an argument operand, and a digit size from a variable");
    CHECK(Formats("{1 :offset}", NULL, 0, "{|1|}", s_badOption) &&
              Formats("{1 :offset add=1 subtract=1}", NULL, 0, "{|1|}", s_badOption) &&
              Formats("{1 :offset add=100}", NULL, 0, "{|1|}", s_badOption) &&
              Formats("{1 :offset add=01}", NULL, 0, "{|1|}", s_badOption) &&
              Formats("{$n :offset add=$x}", arguments, 3, "{$n}", s_badOption),
          "none, both, or a bad value is a bad option and a fallback");
    static const mlocMessageFaultKind missing[] = {mloc_messageFaultUnresolvedVariable,
                                                   mloc_messageFaultBadOption, 0};
    CHECK(Formats("{1 :offset add=$missing}", NULL, 0, "{|1|}", missing) &&
              Formats("{foo :offset add=1}", NULL, 0, "{|foo|}", s_badOperand),
          "a failed variable is reported once; a bad operand falls back");
    static const mlocMessageFaultKind unsupported[] = {mloc_messageFaultUnsupportedOperation, 0};
    CHECK(Formats("{1e-50 :offset add=1}", NULL, 0, "{|1e-50|}", unsupported) &&
              Formats("{1e-40 :offset add=1}", NULL, 0, "{|1e-40|}", unsupported) &&
              Formats("{1e-39 :offset subtract=1 maximumFractionDigits=3}", NULL, 0, "-1", NULL),
          "a sum past a decimal's digits is unsupported, one of 40 digits is not");
    mlocDecimal infinity = {0};
    infinity.kind = mloc_decimalInfinity;
    mlocMessageArgument endless = {"i", 1, {mloc_messageValueDecimal, 0, {.decimal = &infinity}}};
    CHECK(Formats("{1.5 :offset add=0} {$i :offset add=1}", &endless, 1, "1.5 \xE2\x88\x9E", NULL),
          "adding zero keeps the value; infinity stays");
    CHECK(Formats(".local $o = {$n :offset subtract=4} .match $o 1 {{=1}} one {{one}} * {{other}}",
                  arguments, 3, "=1", NULL) &&
              Formats(".local $o = {$n :offset subtract=4} .match $o one {{one}} * {{other}}",
                      arguments, 3, "one", NULL),
          "an offset value selects on its sum");
}

static void TestPercent(void)
{
    CHECK(Formats("{0.5 :percent} {0.123 :percent} {0.1234 :percent maximumFractionDigits=1}", NULL,
                  0, "50% 12% 12.3%", NULL),
          "a percent is its value times 100, no fraction digits by default");
    CHECK(Formats(".local $x = {0.5 :number minimumIntegerDigits=3} {{{$x :percent}}}", NULL, 0,
                  "50%", NULL) &&
              Formats("{0.5 :percent minimumIntegerDigits=3}", NULL, 0, "50%", NULL),
          ":percent drops and ignores minimum integer digits");
    CHECK(Formats(".local $p = {0.01 :percent} .match $p one {{one}} * {{other}}", NULL, 0, "one",
                  NULL) &&
              Formats(".local $p = {1 :percent} .match $p one {{one}} * {{other}}", NULL, 0,
                      "other", NULL) &&
              Formats(".local $p = {0.07 :percent} .match $p 7 {{=7}} * {{other}}", NULL, 0, "=7",
                      NULL),
          "a percent selects on its value times 100");
    CHECK(Formats(".local $x = {0.01 :number select=exact} .local $p = {$x :percent} "
                  ".match $p one {{one}} * {{other}}",
                  NULL, 0, "one", NULL),
          "an operand's select is dropped without a fault");
}

static void TestCurrency(void)
{
    mlocMessageArgument x = {"x", 1, {mloc_messageValueInteger, 0, {.integer = 41}}};
    CHECK(Formats("{42 :currency currency=EUR} {42.5 :currency currency=jpy}", NULL, 0,
                  "\xE2\x82\xAC"
                  "42.00 \xC2\xA5"
                  "43",
                  NULL) &&
              Formats("{$x :currency currency=USD}", &x, 1, "$41.00", NULL),
          "a number with a currency, in its digits, the code in any case");
    CHECK(Formats("{42 :currency currency=USD fractionDigits=0} {42 :currency currency=USD "
                  "fractionDigits=3}",
                  NULL, 0, "$42 $42.000", NULL) &&
              Formats(".local $c = {42 :currency currency=USD fractionDigits=0} "
                      "{{{$c :currency fractionDigits=auto}}}",
                      NULL, 0, "$42.00", NULL),
          "fractionDigits, and auto over the operand's");
    CHECK(Formats(
              "{1 :currency currency=CAD} {1 :currency currency=CAD currencyDisplay=narrowSymbol} "
              "{1 :currency currency=CAD currencyDisplay=code} "
              "{1 :currency currency=CAD currencyDisplay=never}",
              NULL, 0,
              "CA$1.00 $1.00 CAD\xC2\xA0"
              "1.00 1.00",
              NULL),
          "the displays");
    CHECK(Formats("{-5 :currency currency=USD currencySign=accounting} {-5 :currency currency=USD "
                  "currencySign=standard}",
                  NULL, 0, "($5.00) -$5.00", NULL),
          "the currency sign");
    mlocMessageArgument n = {"n", 1, {mloc_messageValueInteger, 0, {.integer = -5}}};
    CHECK(Formats(".input {$n :currency currency=USD trailingZeroDisplay=stripIfInteger} "
                  "{{{$n :currency currencySign=accounting}}}",
                  &n, 1, "($5)", NULL) &&
              Formats("{5.01 :currency currency=USD trailingZeroDisplay=stripIfInteger}", NULL, 0,
                      "$5.01", NULL),
          "a currency value's options pass on");
    CHECK(Formats(".local $n = {1 :number select=exact minimumFractionDigits=3 "
                  "signDisplay=always} {{{$n :currency currency=EUR}}}",
                  NULL, 0,
                  "+\xE2\x82\xAC"
                  "1.00",
                  NULL),
          "a number's select and fraction digits are dropped, its sign kept");
    CHECK(Formats(".local $c = {1 :currency currency=EUR} .local $n = {$c :number} "
                  ".match $n one {{one {$n}}} * {{other}}",
                  NULL, 0, "one 1", NULL),
          "a number from a currency value selects");
    mlocDecimal amount = {0};
    CHECK(mlocParseDecimal("42.5", 4, &amount).status == mloc_success, "an amount is read");
    mlocMessageArgument a[] = {
        {"a", 1, {mloc_messageValueCurrency, 0, {.currency = {&amount, {'e', 'u', 'r'}}}}},
        {"c", 1, {mloc_messageValueString, 0, {.string = {"usd", 3}}}},
    };
    CHECK(Formats("{$a :currency} {$a} {$a :number} {$a :string} {$a :offset add=1}", a, 2,
                  "\xE2\x82\xAC"
                  "42.50 42.5 EUR 42.5 42.5 EUR \xE2\x82\xAC"
                  "43.50",
                  NULL),
          "a currency amount: formatted, bare, as a number, as text, offset");
    CHECK(Formats("{$a :currency currency=USD}", a, 2,
                  "\xE2\x82\xAC"
                  "42.50",
                  s_badOption) &&
              Formats("{42 :currency currency=$c}", a, 2, "$42.00", NULL),
          "an amount's currency stands; a code from a variable");
    static const mlocMessageFaultKind noCode[] = {mloc_messageFaultBadOption,
                                                  mloc_messageFaultBadOperand, 0};
    CHECK(Formats("{42 :currency currency=EURO}", NULL, 0, "{|42|}", noCode) &&
              Formats("{42 :currency currency=E1R}", NULL, 0, "{|42|}", noCode) &&
              Formats("{42 :currency currency=EUR currencyDisplay=formal}", NULL, 0,
                      "\xE2\x82\xAC"
                      "42.00",
                      s_badOption) &&
              Formats("{42 :currency currency=EUR fractionDigits=100}", NULL, 0,
                      "\xE2\x82\xAC"
                      "42.00",
                      s_badOption),
          "bad option values; no currency is a bad operand");
    // 124 and 125 bytes of digits: with " EUR", 128 fit the writer's room
    // and 129 do not.
    mlocDecimal wide = {0};
    mlocDecimal wider = {0};
    CHECK(
        mlocParseDecimal("1111111111222222222233333333334444444444e-122", 45, &wide).status ==
                mloc_success &&
            mlocParseDecimal("1111111111222222222233333333334444444444e-123", 45, &wider).status ==
                mloc_success,
        "long amounts are read");
    mlocMessageArgument long1 = {
        "a", 1, {mloc_messageValueCurrency, 0, {.currency = {&wide, {'E', 'U', 'R'}}}}};
    mlocMessageArgument long2 = {
        "a", 1, {mloc_messageValueCurrency, 0, {.currency = {&wider, {'E', 'U', 'R'}}}}};
    static const mlocMessageFaultKind unsupported[] = {mloc_messageFaultUnsupportedOperation, 0};
    CHECK(Formats("{$a}", &long1, 1,
                  "0.0000000000000000000000000000000000000000000000000000000000"
                  "000000000000000000000000111111111122222222223333333333444444"
                  "4444"
                  " EUR",
                  NULL) &&
              Formats("{$a}", &long2, 1, "{$a}", unsupported),
          "an amount written bare fits its room or falls back");
    static mlocMessageFormatter bare;
    mlocLocale en;
    mlocDataSet none = {NULL, 0};
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&bare, &none, &en, &options) == mloc_success,
          "a formatter is made without data");
    s_using = &bare;
    CHECK(Formats("{42 :currency currency=EUR}", NULL, 0, "{|42|}", unsupported),
          "data without the currencies' digits cannot make a currency value");
    s_using = &s_en;
}

// Whether binding prepared a value for each expression of a message as
// wanted says, in order ('p' prepared, '-' not); a currency formatter
// where 'c'.
static bool PreparedAs(const char* message, const char* wanted)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    Resolution r;
    memset(&r, 0, sizeof r);
    if (mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocBindMessage(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
            mloc_success ||
        !mlocReadBound(s_bound, &r.h, &r.formatter) ||
        r.h.view.count[messageExpressions] != strlen(wanted))
    {
        printf("  [%s] does not bind as wanted\n", message);
        return false;
    }
    r.words = mlocBoundWords(s_bound);
    for (uint32_t e = 0; wanted[e] != '\0'; e++)
    {
        const PreparedNumber* p = mlocPreparedFor(&r, e);
        // The bound message is aligned, so its values are read in place;
        // 'o' and 'r' for cardinal and ordinal rules prepared.
        char got = p == NULL                     ? '-'
                   : p->kind == preparedUnit     ? 'u'
                   : p->kind == preparedCurrency ? 'c'
                   : p->rulesType == 1           ? 'o'
                   : p->rulesType == 2           ? 'r'
                                                 : 'p';
        if (got != wanted[e])
        {
            printf("  [%s] expression %u: %c, not %c\n", message, (unsigned)e, got, wanted[e]);
            return false;
        }
    }
    return true;
}

static void TestPrepared(void)
{
    CHECK(PreparedAs("{$n :number} {$n :integer maximumSignificantDigits=2} {$n :percent} "
                     "{$n :currency currency=EUR} {$n :offset add=1} {$n :string} {$n}",
                     "oooco--"),
          "each number function's expression is prepared, :string's and a bare one's not");
    CHECK(PreparedAs("{$n :number select=ordinal} {$n :number select=exact} {$n :integer}", "rpo"),
          "the plural rules a value selects with are prepared with it");
    CHECK(PreparedAs("{$n :number maximumFractionDigits=$d}", "-") &&
              PreparedAs(".local $d = {2} {{{$n :number maximumFractionDigits=$d}}}", "-o") &&
              PreparedAs(".local $d = {$x :number} {{{$n :number maximumFractionDigits=$d}}}",
                         "o-") &&
              PreparedAs("{$a :currency}", "-"),
          "an option from an argument, or no currency, leaves an expression unprepared");
    // A bound message at each alignment, and moved to another: the
    // values prepared in it read in place, or made in the call when moved
    // out of alignment.
    static const char plural[] = ".input {$n :number select=ordinal} .match $n one {{st}} two "
                                 "{{nd}} few {{rd}} * {{th}}";
    size_t size = 0;
    mlocMessageError error;
    CHECK(
        mlocCompileMessage(plural, sizeof plural - 1, s_compiled, sizeof s_compiled, &size, &error)
                .status == mloc_success,
        "the message compiles");
    static uint8_t room[1 << 14];
    mlocMessageArgument n = {"n", 1, {mloc_messageValueInteger, 0, {.integer = 22}}};
    size_t measured = 0;
    CHECK(mlocBindMessage(s_using, s_compiled, size, NULL, 0, &measured) == mloc_errorCapacity,
          "the bound size is measured");
    bool good = true;
    for (size_t at = 0; at < 8; at++)
    {
        size_t bound = 0;
        good = good &&
               mlocBindMessage(s_using, s_compiled, size, room + at, measured, &bound) ==
                   mloc_success &&
               bound == measured;
        for (size_t to = 0; good && to < 8; to++)
        {
            memmove(room + 4096 + to, room + at, bound);
            mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
            good = mlocFormatMessage(room + 4096 + to, &n, 1, &output, NULL) == mloc_success &&
                   output.length == 2 && memcmp(s_text, "nd", 2) == 0;
        }
    }
    CHECK(good, "a bound message at any alignment, moved to any other, selects alike");
    mlocDecimal amount = {0};
    CHECK(mlocParseDecimal("42.5", 4, &amount).status == mloc_success, "an amount is read");
    mlocMessageArgument yen = {
        "a", 1, {mloc_messageValueCurrency, 0, {.currency = {&amount, {'J', 'P', 'Y'}}}}};
    CHECK(Formats("{$a :currency currency=EUR}", &yen, 1,
                  "\xC2\xA5"
                  "43",
                  s_badOption),
          "a call whose options are not the prepared ones makes its own formatter");
    mlocMessageArgument given[] = {
        {"n", 1, {mloc_messageValueInteger, 0, {.integer = 1}}},
        {"d", 1, {mloc_messageValueInteger, 0, {.integer = 0}}},
    };
    CHECK(Formats(".local $x = {$n :number minimumFractionDigits=$d} .match $x one {{one}} * "
                  "{{other}}",
                  given, 2, "one", NULL),
          "a value not prepared selects with rules made in the call");
}

static void TestUnit(void)
{
    CHECK(Formats("{3 :unit unit=meter} {3 :unit unit=meter unitDisplay=long} {3 :unit unit=meter "
                  "unitDisplay=narrow} {1.23456 :unit unit=meter}",
                  NULL, 0, "3 m 3 meters 3m 1.235 m", NULL),
          "units in their widths, 0 to 3 fraction digits");
    CHECK(Formats("{3 :unit unit=megahertz} {3 :unit unit=liter-per-100-kilometer}", NULL, 0,
                  "3 MHz 3 L/100 km", NULL),
          "identifiers of every letter and digits");
    static const mlocMessageFaultKind unsupported[] = {mloc_messageFaultUnsupportedOperation, 0};
    CHECK(Formats("{1234.5 :unit unit=meter usage=road} {1234.5 :unit unit=meter usage=road "
                  "maximumFractionDigits=3}",
                  NULL, 0, "0.77 mi 0.767 mi", NULL) &&
              Formats("{1234.5 :unit unit=meter usage=fluid}", NULL, 0, "4,050 ft", NULL),
          "a usage converts to the locale's unit, its rounding unless the options' wins; one "
          "the unit's quantity lacks takes its default, as ICU's");
    static const mlocMessageFaultKind noUnit[] = {mloc_messageFaultBadOption,
                                                  mloc_messageFaultBadOperand, 0};
    CHECK(Formats("{3 :unit}", NULL, 0, "{|3|}", s_badOperand) &&
              Formats("{3 :unit unit=Meter}", NULL, 0, "{|3|}", noUnit) &&
              Formats("{3 :unit unit=meter-}", NULL, 0, "{|3|}", noUnit) &&
              Formats("{3 :unit unit=foo}", NULL, 0, "{|3|}", unsupported) &&
              Formats("{3 :unit unit=meter usage=Road}", NULL, 0, "3 m", s_badOption) &&
              Formats("{3 :unit unit=meter unitDisplay=wide}", NULL, 0, "3 m", s_badOption),
          "no unit, a unit or usage not well-formed, one the data lacks, a bad display");
    CHECK(Formats(".local $u = {3 :unit unit=meter} {{{$u :unit unitDisplay=long}}}", NULL, 0,
                  "3 meters", NULL) &&
              Formats(".local $u = {3 :unit unit=meter} {{{$u :unit unit=foot}}}", NULL, 0, "3 m",
                      s_badOption) &&
              Formats(".local $n = {3.14159 :number maximumFractionDigits=1 select=exact} "
                      "{{{$n :unit unit=meter}}}",
                      NULL, 0, "3.1 m", NULL),
          "a unit value's options pass on, its unit stands; a number's pass on, select dropped");
    static const mlocMessageFaultKind selector[] = {mloc_messageFaultBadSelector, 0};
    CHECK(Formats(".local $u = {1 :unit unit=meter} .match $u one {{one}} * {{other}}", NULL, 0,
                  "other", selector),
          "a unit value does not select");
    mlocDecimal amount = {0};
    CHECK(mlocParseDecimal("3.5", 3, &amount).status == mloc_success, "an amount is read");
    mlocMessageArgument a[] = {
        {"m", 1, {mloc_messageValueUnit, 0, {.unit = {&amount, "kilometer", 9}}}},
        {"x", 1, {mloc_messageValueString, 0, {.string = {"foot", 4}}}},
        {"b", 1, {mloc_messageValueUnit, 0, {.unit = {&amount, "Kilo meter", 10}}}},
    };
    CHECK(Formats("{$m :unit} {$m} {$m :number} {$m :string} {$m :offset add=1}", a, 3,
                  "3.5 km 3.5 kilometer 3.5 3.5 kilometer 4.5 km", NULL),
          "a unit amount: formatted, bare, as a number, as text, offset");
    CHECK(Formats(".local $s = {$m :string} .match $s |3.5 kilometer| {{=}} * {{other}}", a, 3, "=",
                  NULL) &&
              Formats(".local $s = {$m :string} .match $s |3.5 kilometers| {{=}} |3.5 kilomete| "
                      "{{-}} * {{other}}",
                      a, 3, "other", NULL),
          ":string matches an amount's digits and unit");
    CHECK(Formats("{$m :unit unit=meter}", a, 3, "3.5 km", s_badOption) &&
              Formats("{2 :unit unit=$x unitDisplay=long}", a, 3, "2 feet", NULL) &&
              Formats("{$b :unit}", a, 3, "{$b}", s_badOperand),
          "an amount's unit stands; a unit from a variable; an amount's ill-formed unit");
    CHECK(PreparedAs("{$n :unit unit=meter} {$n :unit unit=$x}", "u-"),
          "a :unit expression is prepared with a unit formatter");
    static mlocMessageFormatter bare;
    mlocLocale en;
    static mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    static mlocDataSet without;
    without = (mlocDataSet){&builtin, 1};
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&bare, &without, &en, &options) == mloc_success,
          "a formatter is made without the unit names");
    s_using = &bare;
    CHECK(Formats("{3 :unit unit=meter}", NULL, 0, "{|3|}", unsupported),
          "without the unit names :unit cannot format");
    s_using = &s_en;
}

static void TestDirection(void)
{
    static mlocMessageFormatter isolatingEn;
    static mlocMessageFormatter isolatingAr;
    Make(&isolatingEn, "en", mloc_messageIsolationDefault);
    Make(&isolatingAr, "ar", mloc_messageIsolationDefault);
    s_using = &isolatingEn;
    CHECK(Formats("{1 :number}", NULL, 0, "1", NULL), "an English number is bare");
    s_using = &isolatingAr;
    CHECK(Formats("{1 :number}", NULL, 0,
                  "\xE2\x81\xA7"
                  "1\xE2\x81\xA9",
                  NULL),
          "an Arabic one is right to left");
    CHECK(Formats("{1 :currency currency=EUR}", NULL, 0,
                  "\xE2\x81\xA7\xE2\x80\x8F"
                  "1.00\xC2\xA0\xE2\x82\xAC\xE2\x81\xA9",
                  NULL),
          "and a currency");
    s_using = &s_en;
}

int main(void)
{
    // The compiled-in data and the unit names, which :unit reads.
    static mlocData sources[2];
    alignas(8) static uint8_t names[8 << 20];
    mlocOpenBuiltinData(&sources[0]);
    FILE* file = fopen(MLOC_UNITS_FILE, "rb");
    size_t size = file == NULL ? 0 : fread(names, 1, sizeof names, file);
    if (file != NULL)
    {
        fclose(file);
    }
    if (size == 0 || size >= sizeof names || mlocOpenData(&sources[1], names, size) != mloc_success)
    {
        fputs("test_message_numbers: cannot open " MLOC_UNITS_FILE "\n", stderr);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    Make(&s_en, "en", mloc_messageIsolationNone);
    Make(&s_fr, "fr", mloc_messageIsolationNone);
    Make(&s_ar, "ar", mloc_messageIsolationNone);
    TestFormatting();
    TestOptions();
    TestOperands();
    TestInheritance();
    TestSelection();
    TestOffset();
    TestPercent();
    TestCurrency();
    TestPrepared();
    TestUnit();
    TestDirection();
    return s_failures == 0 ? 0 : 1;
}
