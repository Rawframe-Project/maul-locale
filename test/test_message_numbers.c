// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number functions of messages (maul-locale/message.h, record
// mloc-0045): :number, :integer, :offset and :percent formatting with ECMA-402's defaults
// and options, the operands and option values they take and refuse,
// options passed on from a number value, and selection by exact value
// and plural category, with the faults of each.

#include "test_harness.h"

#include "maul-locale/message.h"

#include <math.h>
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
    mlocMessageOptions options = {false, isolation, NULL, 0};
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
    s_using = &s_en;
}

int main(void)
{
    static mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
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
    TestDirection();
    return s_failures == 0 ? 0 : 1;
}
