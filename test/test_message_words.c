// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The mloc: functions over numbers and durations (maul-locale/message.h,
// record mloc-0045): mloc:spellout by rule set or numbering system,
// MF1's spellout and ordinal texts as ICU4C 79 writes them;
// mloc:relative by unit, style and numeric with number options;
// mloc:duration over durations, ISO 8601 literals and seconds with
// ECMA-402's options; mloc:list by type and style, selecting by its
// count; their refusals, their operands passed on, their
// parts and preparation, and that none but mloc:list selects.

#include "message_prepare.h"
#include "test_harness.h"

#include "maul-locale/message.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static uint8_t s_compiled[1 << 16];
static uint8_t s_bound[1 << 16];
static char s_text[1 << 10];

static const mlocMessageFaultKind s_badOption[] = {mloc_messageFaultBadOption, 0};
static const mlocMessageFaultKind s_badOperand[] = {mloc_messageFaultBadOperand, 0};
static const mlocMessageFaultKind s_unsupported[] = {mloc_messageFaultUnsupportedOperation, 0};

// Whether a message formats in a locale to a text with the faults given
// in order (0 ending the list), its parts reported when parts is set.
static bool Formats(const char* tag, const char* message, const mlocMessageArgument* arguments,
                    size_t count, const char* text, const mlocMessageFaultKind* expected)
{
    static mlocMessageFormatter formatter;
    mlocLocale locale;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    if (mlocParseLocale(tag, strlen(tag), &locale).status != mloc_success ||
        mlocInitMessageFormatter(&formatter, &s_data, &locale, &options) != mloc_success ||
        mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocBindMessage(&formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
            mloc_success)
    {
        printf("  [%s] does not compile and bind in %s\n", message, tag);
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
        printf("  [%s] in %s -> %d [%.*s], %zu faults", message, tag, (int)result,
               (int)output.length, s_text, faults.count);
        for (size_t i = 0; i < faults.count && i < 8; i++)
        {
            printf(" %d", (int)list[i].kind);
        }
        printf("\n");
    }
    return good;
}

static void TestSpellout(void)
{
    // ICU4C 79's MF1: {0,spellout}, {0,ordinal} and {0,spellout,%rules}.
    CHECK(
        Formats("en",
                "{21 :mloc:spellout}|{3723 :mloc:spellout}|{59.5 :mloc:spellout}|{-5 "
                ":mloc:spellout}",
                NULL, 0,
                "twenty-one|three thousand seven hundred twenty-three|fifty-nine point five|minus "
                "five",
                NULL),
        "MF1's spellout: the numbering rules, exact");
    CHECK(Formats("en",
                  "{21 :mloc:spellout rules=digits-ordinal}|{3723 :mloc:spellout "
                  "rules=digits-ordinal}|{21 :mloc:spellout rules=spellout-ordinal}",
                  NULL, 0, "21st|3,723rd|twenty-first", NULL) &&
              Formats("de", "{21 :mloc:spellout}|{21 :mloc:spellout rules=digits-ordinal}", NULL, 0,
                      "ein\xC2\xAD"
                      "und\xC2\xAD"
                      "zwanzig|21.",
                      NULL) &&
              Formats("tr", "{21 :mloc:spellout rules=spellout-ordinal}|{59.5 :mloc:spellout}",
                      NULL, 0, "yirmi birinci|elli dokuz virgül beş", NULL) &&
              Formats("fr", "{21 :mloc:spellout rules=spellout-cardinal-feminine}", NULL, 0,
                      "vingt-et-une", NULL),
          "MF1's ordinal and rule sets, in four locales");
    CHECK(Formats("en", "{2024 :mloc:spellout numberingSystem=roman}", NULL, 0, "MMXXIV", NULL) &&
              Formats("en", "{2024 :mloc:spellout numberingSystem=roman rules=digits-ordinal}",
                      NULL, 0, "2,024th", NULL),
          "a numbering system, the later of it and a rule set standing");
    mlocMessageArgument a[] = {
        {"n", 1, {mloc_messageValueInteger, 0, {.integer = 21}}},
        {"r", 1, {mloc_messageValueString, 0, {.string = {"spellout-ordinal", 16}}}},
    };
    CHECK(
        Formats("en", "{$n :mloc:spellout}", a, 2, "twenty-one", NULL) &&
            Formats("en", "{$n :mloc:spellout rules=$r}", a, 2, "twenty-one", s_badOption) &&
            Formats("en", "{$n :mloc:spellout rules=Spellout}", a, 2, "twenty-one", s_badOption) &&
            Formats("fr", "{$n :mloc:spellout rules=spellout-ordinal}", a, 2, "{$n}",
                    s_unsupported) &&
            Formats("en", "{$n :mloc:spellout numberingSystem=latn}", a, 2, "{$n}",
                    s_unsupported) &&
            Formats("en", "{horse :mloc:spellout}", a, 2, "{|horse|}", s_badOperand),
        "a rule set from a variable or not a name is a bad option; one the locale lacks, an "
        "unsupported operation");
}

static void TestRelative(void)
{
    CHECK(Formats("en",
                  "{-1 :mloc:relative unit=day}|{-1 :mloc:relative unit=day numeric=auto}|{3 "
                  ":mloc:relative unit=hour style=short}|{1.25 :mloc:relative unit=hour}|{1.25 "
                  ":mloc:relative unit=hour maximumFractionDigits=0}|{1 :mloc:relative "
                  "unit=monday numeric=auto}",
                  NULL, 0, "1 day ago|yesterday|in 3 hr.|in 1.25 hours|in 1 hour|next Monday",
                  NULL),
          "units, styles, numeric and number options");
    mlocMessageArgument u = {"u", 1, {mloc_messageValueString, 0, {.string = {"day", 3}}}};
    static const mlocMessageFaultKind twice[] = {mloc_messageFaultBadOption,
                                                 mloc_messageFaultBadOperand, 0};
    CHECK(Formats("en", "{2 :mloc:relative}", NULL, 0, "{|2|}", s_badOperand) &&
              Formats("en", "{2 :mloc:relative unit=fortnight}", NULL, 0, "{|2|}", twice) &&
              Formats("en", "{2 :mloc:relative unit=$u}", &u, 1, "{|2|}", twice) &&
              Formats("en", "{2 :mloc:relative unit=day style=wide}", NULL, 0, "in 2 days",
                      s_badOption) &&
              Formats("en", "{2 :mloc:relative unit=day signDisplay=always}", NULL, 0, "in 2 days",
                      NULL),
          "no unit is a bad operand; a bad value a bad option; signDisplay is not its option");
}

static void TestDuration(void)
{
    // MF1's {0,duration} writes 3723 as "1:02:03" in en alone.
    CHECK(Formats("en",
                  "{3723 :mloc:duration}|{3723 :mloc:duration style=digital}|{59.5 "
                  ":mloc:duration}|{-5 :mloc:duration}|{90061 :mloc:duration style=digital}",
                  NULL, 0, "1 hr, 2 min, 3 sec|1:02:03|59 sec, 500 ms|-5 sec|25:01:01", NULL),
          "seconds balanced up to hours, as MF1's duration takes them");
    CHECK(Formats("en",
                  "{|PT1H30M| :mloc:duration style=long}|{|P2DT3H| :mloc:duration "
                  "style=narrow}|{|PT90M| :mloc:duration}|{|PT1H| :mloc:duration "
                  "minutesDisplay=always}",
                  NULL, 0, "1 hour, 30 minutes|2d 3h|90 min|1 hr, 0 min", NULL),
          "ISO 8601 literals, unbalanced, with styles and displays");
    CHECK(Formats("en",
                  "{|PT1H2M3.456S| :mloc:duration hours=numeric}|{|PT59.5S| :mloc:duration "
                  "style=digital fractionalDigits=2}|{|PT2H| :mloc:duration hours=2-digit}",
                  NULL, 0, "1:02:03.456|0:00:59.50|02:00:00", NULL),
          "numeric fields and fractional digits");
    mlocDurationValues values = {0, 0, 0, 2, 3, 0, 0, 0, 0, 0};
    mlocMessageArgument d = {"d", 1, {mloc_messageValueDuration, 0, {.duration = &values}}};
    CHECK(Formats("en", "{$d :mloc:duration}|{$d}|{$d :string}", &d, 1,
                  "2 days, 3 hr|P2DT3H|P2DT3H", NULL),
          "a duration argument, and its ISO 8601 text bare or as a string");
    mlocMessageArgument s = {"s", 1, {mloc_messageValueString, 0, {.string = {"long", 4}}}};
    CHECK(
        Formats("en", "{|PT1H| :mloc:duration style=wide}", NULL, 0, "1 hr", s_badOption) &&
            Formats("en", "{|PT1H| :mloc:duration years=numeric}", NULL, 0, "1 hr", s_badOption) &&
            Formats("en", "{|PT1H| :mloc:duration milliseconds=2-digit}", NULL, 0, "1 hr",
                    s_badOption) &&
            Formats("en", "{|PT1H| :mloc:duration style=$s}", &s, 1, "1 hr", s_badOption) &&
            Formats("en", "{|PT1H| :mloc:duration fractionalDigits=10}", NULL, 0, "1 hr",
                    s_badOption) &&
            Formats("en", "{|PT1H| :mloc:duration hoursDisplay=never}", NULL, 0, "1 hr",
                    s_badOption) &&
            Formats("en", "{|PT1H| :mloc:duration hours=numeric minutes=long}", NULL, 0, "{|PT1H|}",
                    s_unsupported),
        "bad values and variables are bad options; options ECMA-402 refuses together, an "
        "unsupported operation");
    mlocDurationValues mixed = {0, 0, 0, 1, -1, 0, 0, 0, 0, 0};
    mlocMessageArgument bad = {"d", 1, {mloc_messageValueDuration, 0, {.duration = &mixed}}};
    mlocMessageArgument none = {"d", 1, {mloc_messageValueDuration, 0, {.duration = NULL}}};
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    static mlocMessageFormatter formatter;
    mlocLocale en;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&formatter, &s_data, &en, &options) == mloc_success &&
              mlocCompileMessage("{$d}", 4, s_compiled, sizeof s_compiled, &size, &error).status ==
                  mloc_success &&
              mlocBindMessage(&formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) ==
                  mloc_success &&
              mlocFormatMessage(s_bound, &bad, 1, &output, NULL) == mloc_errorInvalid &&
              mlocFormatMessage(s_bound, &none, 1, &output, NULL) == mloc_errorInvalid,
          "a duration of two signs or none is refused");
    CHECK(Formats("en", "{horse :mloc:duration}", NULL, 0, "{|horse|}", s_badOperand) &&
              Formats("en", ".local $n = {90 :number} {{{$n :mloc:duration}}}", NULL, 0,
                      "1 min, 30 sec", NULL) &&
              Formats("en",
                      ".local $d = {|PT90M| :mloc:duration style=long} {{{$d :mloc:duration}|{$d "
                      ":number}}}",
                      NULL, 0, "90 minutes|{$d}", s_badOperand),
          "no duration is a bad operand; a number value's seconds; a duration value's "
          "options pass on, and it is no number");
    mlocMessageArgument huge[] = {
        {"i", 1, {mloc_messageValueDouble, 0, {.number = (double)INFINITY}}},
        {"t", 1, {mloc_messageValueInteger, 0, {.integer = (int64_t)1 << 53}}},
        {"u", 1, {mloc_messageValueInteger, 0, {.integer = ((int64_t)1 << 53) - 1}}},
    };
    CHECK(Formats("en", "{$i :mloc:duration}", huge, 3, "{$i}", s_badOperand) &&
              Formats("en", "{$t :mloc:duration}", huge, 3, "{$t}", s_badOperand) &&
              Formats("en", "{1e16 :mloc:duration}", huge, 3, "{|1e16|}", s_badOperand) &&
              Formats("en", "{$u :mloc:duration style=digital}", huge, 3, "2501999792983:36:31",
                      NULL),
          "seconds past a duration's 2^53 are a bad operand, the last whole second is not");
    CHECK(Formats("en",
                  "{|PT1M5S| :mloc:duration minutes=numeric seconds=2-digit}|{|PT1H| "
                  ":mloc:duration minutesDisplay=auto}",
                  NULL, 0, "1:05|1 hr", NULL),
          "seconds take 2-digit; a display auto");
    static const mlocMessageFaultKind selector[] = {mloc_messageFaultBadSelector, 0};
    CHECK(Formats("en", ".local $d = {|PT1H| :mloc:duration} .match $d * {{other}}", NULL, 0,
                  "other", selector),
          "a duration does not select");
}

static void TestList(void)
{
    static const mlocListItem three[] = {{"Ann", 3}, {"Bob", 3}, {"Cem", 3}};
    mlocMessageArgument l = {"l", 1, {mloc_messageValueList, 0, {.list = {three, 3}}}};
    CHECK(Formats("en",
                  "{$l :mloc:list}|{$l :mloc:list style=short}|{$l :mloc:list "
                  "type=disjunction}|{$l :mloc:list type=unit style=narrow}",
                  &l, 1, "Ann, Bob, and Cem|Ann, Bob, & Cem|Ann, Bob, or Cem|Ann Bob Cem", NULL) &&
              Formats("tr", "{$l :mloc:list}|{$l :mloc:list type=disjunction}", &l, 1,
                      "Ann, Bob ve Cem|Ann, Bob veya Cem", NULL),
          "a list by type and style, in its locale");
    static const mlocListItem spanish[] = {{"Pedro", 5}, {"Ignacio", 7}};
    mlocMessageArgument es = {"l", 1, {mloc_messageValueList, 0, {.list = {spanish, 2}}}};
    CHECK(Formats("es", "{$l :mloc:list}", &es, 1, "Pedro e Ignacio", NULL),
          "a list's joining word as its next item asks");
    mlocMessageArgument s[] = {
        l,
        {"s", 1, {mloc_messageValueString, 0, {.string = {"short", 5}}}},
    };
    CHECK(
        Formats("en", "{$l :mloc:list type=and}", &l, 1, "Ann, Bob, and Cem", s_badOption) &&
            Formats("en", "{$l :mloc:list style=wide}", &l, 1, "Ann, Bob, and Cem", s_badOption) &&
            Formats("en", "{$l :mloc:list style=$s}", s, 2, "Ann, Bob, and Cem", s_badOption),
        "bad values and variables are bad options");
    CHECK(Formats("en", "{horse :mloc:list}", NULL, 0, "{|horse|}", s_badOperand) &&
              Formats("en", "{$s :mloc:list}", s, 2, "{$s}", s_badOperand) &&
              Formats("en", "{$l}|{$l :string}", &l, 1, "{$l}|{$l}",
                      (const mlocMessageFaultKind[]){mloc_messageFaultUnsupportedOperation,
                                                     mloc_messageFaultUnsupportedOperation, 0}),
          "no list is a bad operand; a bare list is not written");
    CHECK(Formats("en",
                  ".local $o = {$l :mloc:list type=disjunction} {{{$o :mloc:list "
                  "style=short}|{$o :number}}}",
                  &l, 1, "Ann, Bob, or Cem|{$o}", s_badOperand),
          "a list value's options pass on, and it is no number");
    const char* select = ".local $n = {$l :mloc:list} .match $n 0 {{none}} 1 {{only {$n}}} one "
                         "{{{$n} is}} * {{{$n} are}}";
    mlocMessageArgument none = {"l", 1, {mloc_messageValueList, 0, {.list = {NULL, 0}}}};
    mlocMessageArgument one = {"l", 1, {mloc_messageValueList, 0, {.list = {three, 1}}}};
    mlocMessageArgument two = {"l", 1, {mloc_messageValueList, 0, {.list = {three, 2}}}};
    CHECK(Formats("en", select, &none, 1, "none", NULL) &&
              Formats("en", select, &one, 1, "only Ann", NULL) &&
              Formats("en", select, &two, 1, "Ann and Bob are", NULL) &&
              Formats("en", ".local $n = {$l :mloc:list} .match $n one {{{$n} is}} * {{are}}", &one,
                      1, "Ann is", NULL) &&
              Formats("en", ".local $n = {$l :mloc:list} .match $n 1.0 {{a}} 1e0 {{b}} * {{c}}",
                      &one, 1, "c", NULL),
          "a list selects by its count: an exact count first, then its plural category");
    mlocMessageArgument unprepared[] = {
        one,
        {"s", 1, {mloc_messageValueString, 0, {.string = {"short", 5}}}},
    };
    CHECK(Formats("en", ".local $n = {$l :mloc:list style=$s} .match $n one {{is}} * {{are}}",
                  unprepared, 2, "is", s_badOption),
          "a list not prepared selects with rules made in the call");
    static const mlocMessageFaultKind badKey[] = {mloc_messageFaultBadVariantKey, 0};
    CHECK(Formats("en", ".local $n = {$l :mloc:list} .match $n horse {{a}} * {{b}}", &one, 1, "b",
                  badKey),
          "a key neither a count nor a plural keyword is a bad variant key");
    static const mlocListItem holes[] = {{NULL, 1}};
    mlocMessageArgument hole = {"l", 1, {mloc_messageValueList, 0, {.list = {holes, 1}}}};
    mlocMessageArgument lost = {"l", 1, {mloc_messageValueList, 0, {.list = {NULL, 1}}}};
    static mlocMessageFormatter formatter;
    mlocLocale en;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    CHECK(
        mlocParseLocale("en", 2, &en).status == mloc_success &&
            mlocInitMessageFormatter(&formatter, &s_data, &en, &options) == mloc_success &&
            mlocCompileMessage("{$l :mloc:list}", 15, s_compiled, sizeof s_compiled, &size, &error)
                    .status == mloc_success &&
            mlocBindMessage(&formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) ==
                mloc_success &&
            mlocFormatMessage(s_bound, &hole, 1, &output, NULL) == mloc_errorInvalid &&
            mlocFormatMessage(s_bound, &lost, 1, &output, NULL) == mloc_errorInvalid,
        "a list with an item's text or its items missing is refused");
}

static void TestOperands(void)
{
    CHECK(Formats("en",
                  ".local $n = {2.5 :number maximumFractionDigits=0} {{{$n :mloc:relative "
                  "unit=day}}}",
                  NULL, 0, "in 3 days", NULL) &&
              Formats("en", ".local $s = {7 :mloc:spellout} {{{$s :number}|{$s}}}", NULL, 0,
                      "7|seven", NULL) &&
              Formats("en",
                      ".local $r = {2 :mloc:relative unit=day} {{{$r :mloc:spellout}|{$r "
                      ":mloc:relative unit=week}}}",
                      NULL, 0, "two|in 2 weeks", NULL),
          "number values pass their number and the options the function takes");
    static const mlocMessageFaultKind selector[] = {mloc_messageFaultBadSelector, 0};
    CHECK(Formats("en", ".local $s = {1 :mloc:spellout} .match $s 1 {{one}} * {{other}}", NULL, 0,
                  "other", selector) &&
              Formats("en",
                      ".local $r = {1 :mloc:relative unit=day} .match $r one {{one}} * {{other}}",
                      NULL, 0, "other", selector),
          "neither selects");
}

// Whether binding in English prepared each expression's value as
// wanted says, in order ('s' or 'r' for a spell-out or relative
// formatter, 'n' for a number formatter, 'D' for a duration's, 'L' for a
// list's with its cardinal rules, '-' for none).
static bool PreparedAs(const char* message, const char* wanted)
{
    static mlocMessageFormatter formatter;
    mlocLocale en;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    Resolution r;
    memset(&r, 0, sizeof r);
    if (mlocParseLocale("en", 2, &en).status != mloc_success ||
        mlocInitMessageFormatter(&formatter, &s_data, &en, &options) != mloc_success ||
        mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocBindMessage(&formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
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
        char got = p == NULL                     ? '-'
                   : wanted[e] == 'D'            ? 'D'
                   : p->kind == preparedSpellout ? 's'
                   : p->kind == preparedRelative ? 'r'
                   : p->kind == preparedNumber   ? 'n'
                   : wanted[e] == 'L'
                       ? (((const PreparedList*)(const void*)p)->rulesMade ? 'L' : 'x')
                       : '?';
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
    CHECK(PreparedAs("{$n :mloc:spellout} {$n :mloc:relative unit=day} {$n :mloc:relative "
                     "unit=day maximumFractionDigits=$d} {$n :mloc:spellout rules=xyz-abc}",
                     "sr--") &&
              PreparedAs("{$d :mloc:duration style=long} {$d :mloc:duration hours=numeric "
                         "minutes=long}",
                         "D-") &&
              PreparedAs("{$l :mloc:list} {$l :mloc:list type=unit style=narrow} {$l :mloc:list "
                         "style=$s}",
                         "LL-"),
          "both are prepared, unless an option comes from an argument or the formatter "
          "cannot be made");
    mlocMessageArgument a[] = {
        {"n", 1, {mloc_messageValueDecimal, 0, {.decimal = NULL}}},
        {"d", 1, {mloc_messageValueInteger, 0, {.integer = 0}}},
    };
    mlocDecimal amount;
    CHECK(mlocParseDecimal("1.25", 4, &amount).status == mloc_success, "a decimal is read");
    a[0].value.as.decimal = &amount;
    CHECK(Formats("en", "{$n :mloc:relative unit=hour maximumFractionDigits=$d}", a, 2, "in 1 hour",
                  NULL),
          "a relative time made in the call");
}

// The parts a call reported.
typedef struct Parts
{
    uint32_t kinds[16];
    size_t count;
} Parts;

static void Collect(void* context, uint32_t part, size_t start, size_t length)
{
    (void)start;
    (void)length;
    Parts* parts = context;
    if (parts->count < 16)
    {
        parts->kinds[parts->count] = part;
    }
    parts->count++;
}

static void TestParts(void)
{
    mlocMessageFormatter formatter;
    mlocLocale en;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    const char* message = "{21 :mloc:spellout}|{3 :mloc:relative unit=hour}";
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    Parts parts = {{0}, 0};
    mlocOutput output = {s_text, sizeof s_text, 0, Collect, &parts};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&formatter, &s_data, &en, &options) == mloc_success &&
              mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size,
                                 &error)
                      .status == mloc_success &&
              mlocBindMessage(&formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) ==
                  mloc_success &&
              mlocFormatMessage(s_bound, NULL, 0, &output, NULL) == mloc_success &&
              output.length == 21 && memcmp(s_text, "twenty-one|in 3 hours", 21) == 0,
          "both write with parts asked for");
    // The spelled-out number, the text, the relative time and its parts:
    // a literal, the integer and a literal.
    uint32_t wanted[] = {0x81, 0x80, 0x181, 13, 0, 13};
    bool good = parts.count == 6;
    for (size_t i = 0; good && i < sizeof wanted / sizeof wanted[0]; i++)
    {
        good = parts.kinds[i] == wanted[i];
    }
    CHECK(good, "a spelled-out number has no parts of its own, a relative time its number's");
    static const mlocListItem two[] = {{"Ann", 3}, {"Bob", 3}};
    mlocMessageArgument l = {"l", 1, {mloc_messageValueList, 0, {.list = {two, 2}}}};
    parts.count = 0;
    output.length = 0;
    CHECK(mlocCompileMessage("{$l :mloc:list}", 15, s_compiled, sizeof s_compiled, &size, &error)
                      .status == mloc_success &&
              mlocBindMessage(&formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) ==
                  mloc_success &&
              mlocFormatMessage(s_bound, &l, 1, &output, NULL) == mloc_success &&
              output.length == 11 && memcmp(s_text, "Ann and Bob", 11) == 0,
          "a list writes with parts asked for");
    // The list, then its item, the literal and the item.
    uint32_t listed[] = {0x81, 0, 1, 0};
    good = parts.count == 4;
    for (size_t i = 0; good && i < sizeof listed / sizeof listed[0]; i++)
    {
        good = parts.kinds[i] == listed[i];
    }
    CHECK(good, "a list's items and literals nest in its placeholder");
}

// Opens a data file into a buffer as a source; false when it cannot.
static bool Open(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t size = file == NULL ? 0 : fread(buffer, 1, capacity, file);
    if (file != NULL)
    {
        fclose(file);
    }
    if (size == 0 || size >= capacity || mlocOpenData(out, buffer, size) != mloc_success)
    {
        fprintf(stderr, "test_message_words: cannot open %s\n", path);
        return false;
    }
    return true;
}

int main(void)
{
    static mlocData sources[4];
    alignas(8) static uint8_t spellout[8 << 20];
    alignas(8) static uint8_t relative[8 << 20];
    alignas(8) static uint8_t units[8 << 20];
    mlocOpenBuiltinData(&sources[0]);
    if (!Open(MLOC_SPELLOUT_FILE, spellout, sizeof spellout, &sources[1]) ||
        !Open(MLOC_RELATIVE_FILE, relative, sizeof relative, &sources[2]) ||
        !Open(MLOC_UNITS_FILE, units, sizeof units, &sources[3]))
    {
        return 2;
    }
    s_data = (mlocDataSet){sources, 4};
    TestSpellout();
    TestRelative();
    TestDuration();
    TestList();
    TestOperands();
    TestParts();
    TestPrepared();
    return s_failures == 0 ? 0 : 1;
}
