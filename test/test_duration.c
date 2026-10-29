// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Durations with the unit names data file: the styles, each field's style
// and display, the fractions, the sign, the digital separators, the parts,
// the resolved options and the refusals. Every expected text is ECMA-402's
// algorithm on ICU4C 79's data (sr-Cyrl-ME's aside, whose CLDR data is
// Cyrillic where ICU's falls back to the Latin script).

#include "duration_format.h"
#include "test_harness.h"

#include "maul-locale/duration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocDataSet s_data;

// A case: the locale, the options as "style fields displays digits"
// (a field's style l, s, n, N or 2, a display u or l, - for the default),
// the fields (- for none), and the text.
typedef struct Case
{
    const char* locale;
    const char* options;
    const char* fields;
    const char* text;
} Case;

// test262's intl402/DurationFormat inputs (en, es), cases of a sweep
// against ICU4C, and locales of each separator, padding, script and list.
static const Case s_cases[] = {
    {"en", "long ---------- ---------- -", "1 2 3 3 4 5 6 7 8 9",
     "1 year, 2 months, 3 weeks, 3 days, 4 hours, 5 minutes, 6 seconds, 7 milliseconds, 8 "
     "microseconds, 9 nanoseconds"},
    {"en", "long ---------- ---------- -", "-1 -2 -3 -3 -4 -5 -6 -7 -8 -9",
     "-1 year, 2 months, 3 weeks, 3 days, 4 hours, 5 minutes, 6 seconds, 7 milliseconds, 8 "
     "microseconds, 9 nanoseconds"},
    {"en", "long ---------- ---------- -", "- - - - 0 - -1 - - -", "-1 second"},
    {"en", "long ---------- ---------- -", "- - - -1 -2 - - - - -", "-1 day, 2 hours"},
    {"en", "short ---------- ---------- -", "1 2 3 3 4 5 6 7 8 9",
     "1 yr, 2 mths, 3 wks, 3 days, 4 hr, 5 min, 6 sec, 7 ms, 8 μs, 9 ns"},
    {"en", "short ---------- ---------- -", "-1 -2 -3 -3 -4 -5 -6 -7 -8 -9",
     "-1 yr, 2 mths, 3 wks, 3 days, 4 hr, 5 min, 6 sec, 7 ms, 8 μs, 9 ns"},
    {"en", "short ---------- ---------- -", "- - - - 0 - -1 - - -", "-1 sec"},
    {"en", "short ---------- ---------- -", "- - - -1 -2 - - - - -", "-1 day, 2 hr"},
    {"en", "narrow ---------- ---------- -", "1 2 3 3 4 5 6 7 8 9",
     "1y 2m 3w 3d 4h 5m 6s 7ms 8μs 9ns"},
    {"en", "narrow ---------- ---------- -", "-1 -2 -3 -3 -4 -5 -6 -7 -8 -9",
     "-1y 2m 3w 3d 4h 5m 6s 7ms 8μs 9ns"},
    {"en", "narrow ---------- ---------- -", "- - - - 0 - -1 - - -", "-1s"},
    {"en", "narrow ---------- ---------- -", "- - - -1 -2 - - - - -", "-1d 2h"},
    {"en", "digital ---------- ---------- -", "1 2 3 3 4 5 6 7 8 9",
     "1 yr, 2 mths, 3 wks, 3 days, 4:05:06.007008009"},
    {"en", "digital ---------- ---------- -", "-1 -2 -3 -3 -4 -5 -6 -7 -8 -9",
     "-1 yr, 2 mths, 3 wks, 3 days, 4:05:06.007008009"},
    {"en", "digital ---------- ---------- -", "- - - - 0 - -1 - - -", "-0:00:01"},
    {"en", "digital ---------- ---------- -", "- - - -1 -2 - - - - -", "-1 day, 2:00:00"},
    {"en", "short ---------- ---------- -", "1 2 3 3 4 5 6 7 8 9",
     "1 yr, 2 mths, 3 wks, 3 days, 4 hr, 5 min, 6 sec, 7 ms, 8 μs, 9 ns"},
    {"en", "digital ---------- ---------- 2", "- - - - 1 22 33 111 222 333", "1:22:33.11"},
    {"en", "digital ---------- ---------- -", "- - - - 1 22 33 111 222 333", "1:22:33.111222333"},
    {"en", "digital ---------- ---------- -", "- - - - 1234567 20 45 - - -", "1234567:20:45"},
    {"en", "digital ---------- ---------- -", "- - - - 12 1234567 20 - - -", "12:1234567:20"},
    {"en", "digital ---------- ---------- -", "- - - - 1234 1234567 12345678 - - -",
     "1234:1234567:12345678"},
    {"en", "short --------N- ---------- -", "- - - - - - 3 444 55 6", "3 sec, 444.055006 ms"},
    {"en", "short ---------N ---------- -", "- - - - - - 3 444 55 6", "3 sec, 444 ms, 55.006 μs"},
    {"en", "short ------N--- ---------- 0", "- - - - - - 1 500 - -", "1"},
    {"en", "short ------N--- ---------- 3", "- - - - - - - 1 500 -", "0.001"},
    {"en", "short ------N--- ---------- 6", "- - - - - - - - 1 500", "0.000001"},
    {"en", "digital ---------- ---------- -", "- - - - - - 10000000 - - 1",
     "0:00:10000000.000000001"},
    {"en", "digital ---------- ---------- -", "- - - - - - 1 2 3 9007199254740991",
     "0:00:9007200.256743991"},
    {"en", "digital ---------- ---------- -",
     "- - - - - - - 4503599627370497000 4.503599627370495e+21 -", "0:00:9007199254740991.975424"},
    {"en", "short ----N----- ---------- -", "- - - - 0 0 0 - - -", "0:00:00"},
    {"en", "short ----N----- ---------- -", "- - - - 0 0 1 - - -", "0:00:01"},
    {"en", "short ----N----- ---------- -", "- - - - 1 0 1 - - -", "1:00:01"},
    {"en", "short ----N----- ---------- -", "- - - - 1 1 0 - - -", "1:01:00"},
    {"en", "short ----N----- ---------- -", "- - - - 1 0 0 - - 1", "1:00:00.000000001"},
    {"en", "short ----N----- -----uu--- -", "- - - - 0 0 0 - - -", "0"},
    {"en", "short ----N----- -----uu--- -", "- - - - 0 0 1 - - -", "0:00:01"},
    {"en", "short ----N----- -----uu--- -", "- - - - 1 0 1 - - -", "1:00:01"},
    {"en", "short ----N----- -----uu--- -", "- - - - 1 1 0 - - -", "1:01"},
    {"en", "short ----N----- -----uu--- -", "- - - - 1 0 0 - - 1", "1:00:00.000000001"},
    {"en", "short ----N----- -----uu--- 0", "- - - - 0 0 0 - - -", "0"},
    {"en", "short ----N----- -----uu--- 0", "- - - - 0 0 1 - - -", "0:00:01"},
    {"en", "short ----N----- -----uu--- 0", "- - - - 1 0 1 - - -", "1:00:01"},
    {"en", "short ----N----- -----uu--- 0", "- - - - 1 1 0 - - -", "1:01"},
    {"en", "short ----N----- -----uu--- 0", "- - - - 1 0 0 - - 1", "1:00:00"},
    {"en", "digital ---------- ----u----- -", "- - - - 0 0 2 - - -", "00:02"},
    {"en", "digital ---------- ----u----- -", "- - - 1 0 1 2 - - -", "1 day, 01:02"},
    {"en", "digital ---------- ----u----- -", "- - - - 0 -1 -2 - - -", "-01:02"},
    {"en", "digital ---------- ----u----- -", "- - - -1 0 -1 -2 - - -", "-1 day, 01:02"},
    {"en", "short ---------- u--------- -", "-0 - - - - - - - - -", ""},
    {"en", "short ---------- l--------- -", "-0 - - - - - - - - -", "0 yrs"},
    {"en", "short ---------- -u-------- -", "- -0 - - - - - - - -", ""},
    {"en", "short ---------- -l-------- -", "- -0 - - - - - - - -", "0 mths"},
    {"en", "short ---------- --u------- -", "- - -0 - - - - - - -", ""},
    {"en", "short ---------- --l------- -", "- - -0 - - - - - - -", "0 wks"},
    {"en", "short ---------- ---u------ -", "- - - -0 - - - - - -", ""},
    {"en", "short ---------- ---l------ -", "- - - -0 - - - - - -", "0 days"},
    {"en", "short ---------- ----u----- -", "- - - - -0 - - - - -", ""},
    {"en", "short ---------- ----l----- -", "- - - - -0 - - - - -", "0 hr"},
    {"en", "short ---------- -----u---- -", "- - - - - -0 - - - -", ""},
    {"en", "short ---------- -----l---- -", "- - - - - -0 - - - -", "0 min"},
    {"en", "short ---------- ------u--- -", "- - - - - - -0 - - -", ""},
    {"en", "short ---------- ------l--- -", "- - - - - - -0 - - -", "0 sec"},
    {"en", "short ---------- -------u-- -", "- - - - - - - -0 - -", ""},
    {"en", "short ---------- --------u- -", "- - - - - - - - -0 -", ""},
    {"en", "short ---------- ---------u -", "- - - - - - - - - -0", ""},
    {"en", "long ---------- ----l----- -", "- - - - - -2 - - - -", "-0 hours, 2 minutes"},
    {"en", "digital ---------- ---------- -", "- - - - 0 -2 - - - -", "-0:02:00"},
    {"en", "short ----2----- ---------- -", "- - - - 1 2 3 - - -", "01:02:03"},
    {"en", "short -----2---- ---------- -", "- - - - 1 2 3 - - -", "1 hr, 02:03"},
    {"en", "short ----N----- ------u--- -", "- - - - 1 0 0 - - -", "1:00"},
    {"en", "long ------2--- ---------- -", "- - - - 1 2 3 4 - -", "1 hour, 2 minutes, 03.004"},
    {"es", "long nnnsssllln ---------- -", "1 2 3 0 4 5 6 7 8 9",
     "1a, 2m, 3sem, 0 d, 4 h, 5 min, 6 segundos, 7 milisegundos, 8 microsegundos y 9ns"},
    {"es", "long ---------- ---------- -", "1 - - - 11 - - - - -", "1 año y 11 horas"},
    {"en", "short ---l-NN--- ---------- -", "- - - 5 1 2 3 - - -", "5 days, 1 hr, 2:03"},
    {"da", "digital ---------- ---------- -", "- - - - 1 2 3 456 - -", "1.02.03,456"},
    {"da", "digital ---------- ---------- -", "2 - - 1 11 - - - - -", "2 år, 1 dag og 11.00.00"},
    {"fi", "digital ---------- ---------- -", "- - - - 1 2 3 456 - -", "1.02.03,456"},
    {"fi", "digital ---------- ---------- -", "2 - - 1 11 - - - - -", "2 v, 1 pv, 11.00.00"},
    {"af", "digital ---------- ---------- -", "- - - - 1 2 3 456 - -", "01:02:03,456"},
    {"af", "digital ---------- ---------- -", "2 - - 1 11 - - - - -", "2 j., 1 dag, 11:00:00"},
    {"de", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "1 Stunde, 2 Minuten, 3 Sekunden und 456 Millisekunden"},
    {"de", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "2 Jahre, 1 Tag und 11 Stunden"},
    {"fr", "narrow ---------- ---------- -", "- - - - 1 2 3 456 - -", "1h 2min 3s 456ms"},
    {"fr", "narrow ---------- ---------- -", "2 - - 1 11 - - - - -", "2a 1j 11h"},
    {"ar", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "ساعة، ودقيقتان، و3 ثوان، و456 ملي ثانية"},
    {"ar", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "سنتان، ويوم، و11 ساعة"},
    {"ar", "digital ---------- ---------- -", "- - - - 1 2 3 456 - -", "1:02:03.456"},
    {"ar", "digital ---------- ---------- -", "2 - - 1 11 - - - - -", "سنتان، ويوم، و11:00:00"},
    {"he", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "1 שעה, שתי דקות, 3 שניות, 456 אלפיות שנייה"},
    {"he", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "2 שנים, 1 יום, 11 שעות"},
    {"hi-u-nu-deva", "digital ---------- ---------- -", "- - - - 1 2 3 456 - -", "१:०२:०३.४५६"},
    {"hi-u-nu-deva", "digital ---------- ---------- -", "2 - - 1 11 - - - - -",
     "२ वर\xE0\xA5\x8D"
     "ष, १ दिन, ११:००:००"},
    {"ja", "long ---------- ---------- -", "- - - - 1 2 3 456 - -", "1 時間 2 分 3 秒 456 ミリ秒"},
    {"ja", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "2 年 1 日 11 時間"},
    {"zh", "short ---------- ---------- -", "- - - - 1 2 3 456 - -", "1小时2分钟3秒456毫秒"},
    {"zh", "short ---------- ---------- -", "2 - - 1 11 - - - - -", "2年1天11小时"},
    {"ru", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "1 час 2 минуты 3 секунды 456 миллисекунд"},
    {"ru", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "2 года 1 день 11 часов"},
    {"pl", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "1 godzina, 2 minuty, 3 sekundy i 456 milisekund"},
    {"pl", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "2 lata, 1 dzień i 11 godzin"},
    {"ff-Adlm", "digital ---------- ---------- -", "- - - - 1 2 3 456 - -", "𞥑:𞥐𞥒:𞥐𞥓.𞥔𞥕𞥖"},
    {"ff-Adlm", "digital ---------- ---------- -", "2 - - 1 11 - - - - -",
     "𞥒 𞤳𞤭𞤼⹁ 𞥑 𞤻𞤢𞤤., 𞥑𞥑:𞥐𞥐:𞥐𞥐"},
    {"my", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "၁ နာရ\xE1\x80\xAE"
     "၊ ၂ မ\xE1\x80\xAD"
     "နစ\xE1\x80\xBA"
     "၊ ၃ စက\xE1\x80\xB9"
     "ကန\xE1\x80\xB7"
     "\xE1\x80\xBA"
     "၊ ၄၅၆ မ\xE1\x80\xAE"
     "လ\xE1\x80\xAE"
     "စက\xE1\x80\xB9"
     "ကန\xE1\x80\xB7"
     "\xE1\x80\xBA"
     ""},
    {"my", "long ---------- ---------- -", "2 - - 1 11 - - - - -",
     "၂ န\xE1\x80\xBE"
     "စ\xE1\x80\xBA"
     "၊ ၁ ရက\xE1\x80\xBA"
     "၊ ၁၁ နာရ\xE1\x80\xAE"
     ""},
    {"sr-Cyrl-ME", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "1 сат, 2 минута, 3 секунде и 456 милисекунди"},
    {"sr-Cyrl-ME", "long ---------- ---------- -", "2 - - 1 11 - - - - -",
     "2 године, 1 дан и 11 сати"},
    {"es", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "1 hora, 2 minutos, 3 segundos y 456 milisegundos"},
    {"es", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "2 años, 1 día y 11 horas"},
    {"he", "narrow ---------- ---------- -", "- - - - 1 2 3 456 - -", "1 שע׳ 2 דק׳ 3 שנ׳ 456 ms"},
    {"he", "narrow ---------- ---------- -", "2 - - 1 11 - - - - -", "2 ש′ 1 י׳ 11 שע׳"},
    {"cy", "long ---------- ---------- -", "- - - - 1 2 3 456 - -",
     "1 awr, 2 funud, 3 eiliad, 456 milieiliad"},
    {"cy", "long ---------- ---------- -", "2 - - 1 11 - - - - -", "2 flynedd, 1 diwrnod, 11 awr"},
    {"ga", "short ---------- ---------- -", "- - - - 1 2 3 456 - -", "1 u, 2 nóim, 3 soic, 456 ms"},
    {"ga", "short ---------- ---------- -", "2 - - 1 11 - - - - -", "2 bl, 1 lá, 11 u"},
    {"es", "long ---------- ---------- -", "- - - 1 11 - - - - -", "1 día y 11 horas"},
    {"he", "long ---------- ---------- -", "- - - 2 1 - - - - -", "יומיים, 1 שעה"},
    // An item longer than the room the text pass keeps it in (75 bytes
    // of Myanmar digits and name), written again where it goes.
    {"my", "long ---------- ---------- -", "- - - - 1 - 9007199254737391 - - -",
     "၁ နာရီ၊ ၉,၀၀၇,၁၉၉,၂၅၄,၇၃၇,၃၉၁ စက္ကန့်"},
};

static mlocDurationFieldStyle FieldStyle(char c)
{
    return c == 'l'   ? mloc_durationFieldLong
           : c == 's' ? mloc_durationFieldShort
           : c == 'n' ? mloc_durationFieldNarrow
           : c == 'N' ? mloc_durationFieldNumeric
           : c == '2' ? mloc_durationFieldTwoDigit
                      : mloc_durationFieldDefault;
}

static mlocDurationOptions ParseOptions(const char* code)
{
    char style[16];
    char fields[16];
    char displays[16];
    char digits[4];
    mlocDurationOptions o = {0};
    if (sscanf(code, "%15s %15s %15s %3s", style, fields, displays, digits) != 4)
    {
        return o;
    }
    o.style = strcmp(style, "long") == 0      ? mloc_durationStyleLong
              : strcmp(style, "narrow") == 0  ? mloc_durationStyleNarrow
              : strcmp(style, "digital") == 0 ? mloc_durationStyleDigital
                                              : mloc_durationStyleShort;
    for (int i = 0; i < mloc_durationFieldCount; i++)
    {
        o.fields[i] = FieldStyle(fields[i]);
        o.displays[i] = displays[i] == 'u'   ? mloc_durationDisplayAuto
                        : displays[i] == 'l' ? mloc_durationDisplayAlways
                                             : mloc_durationDisplayDefault;
    }
    if (digits[0] != '-')
    {
        o.fixedFractionalDigits = true;
        o.fractionalDigits = (uint8_t)(digits[0] - '0');
    }
    return o;
}

static mlocDurationValues ParseFields(const char* text)
{
    double v[mloc_durationFieldCount] = {0};
    const char* at = text;
    for (int i = 0; i < mloc_durationFieldCount; i++)
    {
        char* end = nullptr;
        bool none = at[0] == '-' && (at[1] == ' ' || at[1] == '\0');
        v[i] = none ? 0 : strtod(at, &end);
        at = none ? at + 1 : end;
        at += *at == ' ' ? 1 : 0;
    }
    return (mlocDurationValues){v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]};
}

// Writes a case's duration; mloc_success and the text, or the failure.
static mlocResult Write(const char* id, const mlocDurationOptions* options,
                        const mlocDurationValues* d, mlocOutput* output)
{
    mlocLocale locale;
    mlocDurationFormatter formatter;
    if (mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status != mloc_success)
    {
        return mloc_errorInvalid;
    }
    mlocResult r = mlocInitDurationFormatter(&s_data, &locale, options, &formatter);
    return r < 0 ? r : mlocDurationFormatter_Format(&formatter, d, output);
}

static void TestCases(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        const Case* c = &s_cases[i];
        mlocDurationOptions o = ParseOptions(c->options);
        mlocDurationValues d = ParseFields(c->fields);
        char text[512];
        mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
        mlocResult r = Write(c->locale, &o, &d, &output);
        bool same = r == mloc_success && output.length == strlen(c->text) &&
                    memcmp(text, c->text, output.length) == 0;
        if (!same)
        {
            printf("  %s [%s] [%s]: %d \"%.*s\", want \"%s\"\n", c->locale, c->options, c->fields,
                   r, (int)output.length, text, c->text);
        }
        all = all && same;
    }
    CHECK(all, "each case as ECMA-402 writes it on the locale's data");
}

typedef struct Parts
{
    const char* text;
    char out[512];
} Parts;

// Appends a part as "kind/field[text]", the field its mlocDurationField
// plus one.
static void Part(void* context, uint32_t kind, size_t start, size_t length)
{
    Parts* p = context;
    char item[96];
    snprintf(item, sizeof(item), "%u/%u[%.*s]", kind % mloc_durationPartField,
             kind / mloc_durationPartField, (int)length, p->text + start);
    strcat(p->out, item);
}

static bool PartsAre(const char* code, const char* fields, const char* want)
{
    char text[128];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    mlocDurationOptions o = ParseOptions(code);
    mlocDurationValues d = ParseFields(fields);
    bool same = Write("en", &o, &d, &output) == mloc_success && strcmp(p.out, want) == 0;
    if (!same)
    {
        printf("  parts %s\n", p.out);
    }
    return same;
}

static void TestParts(void)
{
    // Integer 0, literal 13, unit 17, minus 4, decimal 2, fraction 3;
    // hours 5, minutes 6, seconds 7.
    CHECK(PartsAre("digital ---------- ---------- -", "- - - - -1 -2 -3 -400 - -",
                   "4/5[-]0/5[1]13/0[:]0/6[02]13/0[:]0/7[03]2/7[.]3/7[4]"),
          "the numeric fields, each its field's, the separators the list's");
    CHECK(PartsAre("long ---------- ---------- -", "- - - 2 1 - - - - -",
                   "0/4[2]13/4[ ]17/4[days]13/0[, ]0/5[1]13/5[ ]17/5[hour]"),
          "a unit-style field's number, space and name, the list's text between");
}

static void TestResolved(void)
{
    mlocLocale en;
    mlocLocale af;
    mlocDurationFormatter formatter;
    mlocDurationOptions digital = {.style = mloc_durationStyleDigital};
    mlocDurationOptions got;
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocCanonicalizeLocale(&s_data, "af", 2, &af, nullptr).status == mloc_success &&
              mlocInitDurationFormatter(&s_data, &en, &digital, &formatter) == mloc_success &&
              mlocDurationFormatter_GetOptions(&formatter, &got) == mloc_success,
          "a digital formatter's options");
    CHECK(got.style == mloc_durationStyleDigital && got.fields[0] == mloc_durationFieldShort &&
              got.displays[0] == mloc_durationDisplayAuto &&
              got.fields[4] == mloc_durationFieldNumeric &&
              got.displays[4] == mloc_durationDisplayAlways &&
              got.fields[5] == mloc_durationFieldTwoDigit &&
              got.fields[6] == mloc_durationFieldTwoDigit &&
              got.fields[7] == mloc_durationFieldNumeric &&
              got.displays[7] == mloc_durationDisplayAuto && !got.fixedFractionalDigits,
          "ECMA-402's digital defaults, a fraction resolved as numeric");
    CHECK(mlocInitDurationFormatter(&s_data, &af, &digital, &formatter) == mloc_success &&
              mlocDurationFormatter_GetOptions(&formatter, &got) == mloc_success &&
              got.fields[4] == mloc_durationFieldTwoDigit,
          "hours of two digits where the locale's hms pattern pads them");
    mlocDurationOptions fixed = {.fixedFractionalDigits = true, .fractionalDigits = 4};
    CHECK(mlocInitDurationFormatter(&s_data, &en, &fixed, &formatter) == mloc_success &&
              mlocDurationFormatter_GetOptions(&formatter, &got) == mloc_success &&
              got.style == mloc_durationStyleShort && got.fields[9] == mloc_durationFieldShort &&
              got.fixedFractionalDigits && got.fractionalDigits == 4,
          "the defaults' short fields and fixed digits");
}

// Whether options are refused as ECMA-402's constructor throws.
static bool Refused(mlocDurationOptions o)
{
    mlocLocale en;
    mlocDurationFormatter formatter;
    return mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
           mlocInitDurationFormatter(&s_data, &en, &o, &formatter) == mloc_errorInvalid;
}

static void TestOptionRefusals(void)
{
    mlocDurationOptions o = {.style = (mlocDurationStyle)4};
    CHECK(Refused(o), "a style out of range");
    o = (mlocDurationOptions){0};
    o.fields[mloc_durationDays] = mloc_durationFieldNumeric;
    CHECK(Refused(o), "a date field numeric");
    o = (mlocDurationOptions){0};
    o.fields[mloc_durationMilliseconds] = mloc_durationFieldTwoDigit;
    CHECK(Refused(o), "a fraction of two digits");
    o = (mlocDurationOptions){0};
    o.fields[mloc_durationSeconds] = (mlocDurationFieldStyle)6;
    o.displays[mloc_durationSeconds] = mloc_durationDisplayDefault;
    CHECK(Refused(o), "a field style out of range");
    o = (mlocDurationOptions){0};
    o.displays[mloc_durationHours] = (mlocDurationDisplay)3;
    CHECK(Refused(o), "a display out of range");
    o = (mlocDurationOptions){0};
    o.fields[mloc_durationMilliseconds] = mloc_durationFieldNumeric;
    o.displays[mloc_durationMilliseconds] = mloc_durationDisplayAlways;
    CHECK(Refused(o), "display always on a fraction");
    o = (mlocDurationOptions){0};
    o.fields[mloc_durationMinutes] = mloc_durationFieldNumeric;
    o.fields[mloc_durationSeconds] = mloc_durationFieldLong;
    CHECK(Refused(o), "a unit style after a numeric field");
    o = (mlocDurationOptions){0};
    o.fields[mloc_durationMilliseconds] = mloc_durationFieldNumeric;
    o.fields[mloc_durationMicroseconds] = mloc_durationFieldLong;
    CHECK(Refused(o), "a unit style after a fraction");
    o = (mlocDurationOptions){.fixedFractionalDigits = true, .fractionalDigits = 10};
    CHECK(Refused(o), "ten fraction digits");
}

static void TestRefusals(void)
{
    mlocLocale en;
    mlocDurationFormatter formatter = {0};
    char text[16];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocDurationValues one = {.hours = 1};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitDurationFormatter(&s_data, &en, nullptr, nullptr) == mloc_errorInvalid &&
              mlocInitDurationFormatter(nullptr, &en, nullptr, &formatter) == mloc_errorInvalid &&
              mlocInitDurationFormatter(&s_data, nullptr, nullptr, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitDurationFormatter(&s_data, &en, nullptr, &formatter) == mloc_success,
          "null pointers at init");
    mlocDurationValues mixed = {.hours = 1, .minutes = -1};
    mlocDurationValues fraction = {.hours = 1.5};
    mlocDurationValues huge = {.seconds = 9007199254740992.0};
    CHECK(mlocDurationFormatter_Format(&formatter, &mixed, &output) == mloc_errorInvalid &&
              mlocDurationFormatter_Format(&formatter, &fraction, &output) == mloc_errorInvalid &&
              mlocDurationFormatter_Format(&formatter, &huge, &output) == mloc_errorInvalid &&
              mlocDurationFormatter_Format(&formatter, nullptr, &output) == mloc_errorInvalid &&
              mlocDurationFormatter_Format(&formatter, &one, nullptr) == mloc_errorInvalid &&
              mlocDurationFormatter_Format(nullptr, &one, &output) == mloc_errorInvalid,
          "fields of two signs, not integers or past 2^53 seconds; null pointers");
    output = (mlocOutput){.buffer = text, .capacity = 3};
    CHECK(mlocDurationFormatter_Format(&formatter, &one, &output) == mloc_errorCapacity &&
              output.length == strlen("1 hr"),
          "the length needed when the text does not fit");
    output = (mlocOutput){.buffer = text, .capacity = 4};
    CHECK(mlocDurationFormatter_Format(&formatter, &one, &output) == mloc_success &&
              output.length == 4,
          "a text that fills the room exactly");
    output = (mlocOutput){.buffer = nullptr, .capacity = 0};
    CHECK(mlocDurationFormatter_Format(&formatter, &one, &output) == mloc_errorCapacity &&
              output.length == 4,
          "a measure without a buffer");
    mlocDurationFormatter forged = formatter;
    memset(&forged, 0x5A, sizeof(forged));
    mlocDurationOptions got;
    output = (mlocOutput){.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocDurationFormatter_Format(&forged, &one, &output) == mloc_errorInvalid &&
              mlocDurationFormatter_GetOptions(&forged, &got) == mloc_errorInvalid &&
              mlocDurationFormatter_GetOptions(nullptr, &got) == mloc_errorInvalid &&
              mlocDurationFormatter_GetOptions(&formatter, nullptr) == mloc_errorInvalid,
          "a formatter the library did not make");
}

static bool Digital(const char* pattern, bool twoDigit, const char* first, const char* second)
{
    static const DurationFormat s_zero;
    DurationFormat f = s_zero;
    bool padded = false;
    return mlocReadDurationDigital(pattern, strlen(pattern), &f, &padded) && padded == twoDigit &&
           f.separatorLength[0] == strlen(first) &&
           memcmp(f.separator[0], first, strlen(first)) == 0 &&
           f.separatorLength[1] == strlen(second) &&
           memcmp(f.separator[1], second, strlen(second)) == 0;
}

static bool NotDigital(const char* pattern)
{
    static const DurationFormat s_zero;
    DurationFormat f = s_zero;
    bool padded = false;
    return !mlocReadDurationDigital(pattern, strlen(pattern), &f, &padded);
}

static void TestDigitalPatterns(void)
{
    CHECK(Digital("h:mm:ss", false, ":", ":") && Digital("hh.mm.ss", true, ".", ".") &&
              Digital("h m s", false, " ", " ") && Digital("h:m:s", false, ":", ":") &&
              Digital("h--------------mm|ss", false, "--------------", "|") &&
              Digital("h:mm---------------s", false, ":", "---------------"),
          "an hms pattern's separators, a separator of 15 bytes, and its hour padding");
    CHECK(NotDigital("") && NotDigital("mm:ss") && NotDigital("hhh:mm:ss") &&
              NotDigital("h:mmm:ss") && NotDigital("h:mm:sss") && NotDigital("hmm:ss") &&
              NotDigital("h:mmss") && NotDigital("h:mm") && NotDigital("h:mm:ss ") &&
              NotDigital("h----------------mm:ss") && NotDigital("h:mm----------------ss"),
          "an hms pattern of another shape, or a separator past 15 bytes");
}

static void TestBounds(void)
{
    mlocLocale en;
    mlocDurationFormatter formatter;
    DurationFormat f;
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitDurationFormatter(&s_data, &en, nullptr, &formatter) == mloc_success,
          "a formatter to spoil");
    memcpy(&f, &formatter, sizeof(f));
    CHECK(mlocValidDurationFormat(&f), "the formatter as made");
    f.separatorLength[1] = durationSeparatorMost;
    bool most = mlocValidDurationFormat(&f);
    f.separatorLength[1] = durationSeparatorMost + 1;
    CHECK(most && !mlocValidDurationFormat(&f), "a separator of the most bytes, and one past");
    memcpy(&f, &formatter, sizeof(f));
    f.slots[9].start[5] = durationPool;
    f.slots[9].length[5] = 0;
    bool end = mlocValidDurationFormat(&f);
    f.slots[9].start[5] = durationPool - 1;
    f.slots[9].length[5] = 1;
    bool last = mlocValidDurationFormat(&f);
    f.slots[9].length[5] = 2;
    bool past = mlocValidDurationFormat(&f);
    f.slots[9].start[5] = durationPool + 1;
    f.slots[9].length[5] = 0;
    CHECK(end && last && !past && !mlocValidDurationFormat(&f),
          "patterns at the pool's end, and past it");
}

// The compiled-in data, and with it the unit names data file.
static bool OpenData(void)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_names[8 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    FILE* file = fopen(MLOC_UNITS_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(s_names, 1, sizeof(s_names), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_data = (mlocDataSet){s_sources, 2};
    return size > 0 && size < sizeof(s_names) &&
           mlocOpenData(s_names, size, &s_sources[1]) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_duration: cannot open " MLOC_UNITS_FILE "\n", stderr);
        return 2;
    }
    TestCases();
    TestParts();
    TestResolved();
    TestOptionRefusals();
    TestRefusals();
    TestDigitalPatterns();
    TestBounds();
    return s_failures == 0 ? 0 : 1;
}
