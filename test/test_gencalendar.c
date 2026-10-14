// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's calendar tables (record mloc-0040): Japanese eras, a
// well-formed list becoming the table's source with each era's first day
// by the library's Gregorian arithmetic; the Chinese and Dangi years,
// the same as the library's compiled tables from the repository's
// overrides, each override in its own calendar's table; every malformed
// input refused.

#include "calendar_gen.h"
#include "calendar_years.h"
#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

static char s_error[256];

// The source made from a calendarData holding the given eras, or null
// with the message in s_error.
static char* Source(const char* eras)
{
    char document[2048];
    snprintf(document, sizeof(document),
             "<supplementalData><calendarData><calendar type=\"gregorian\"/>"
             "<calendar type=\"japanese\"><eras>%s</eras></calendar></calendarData>"
             "</supplementalData>",
             eras);
    s_error[0] = '\0';
    XmlNode* root = XmlParse(document, strlen(document), s_error, sizeof(s_error));
    char* source = nullptr;
    size_t size = 0;
    if (root == nullptr || !CalendarErasSource(root, &source, &size, s_error, sizeof(s_error)))
    {
        source = nullptr;
    }
    XmlFree(root);
    return source;
}

static bool Refused(const char* eras)
{
    char* source = Source(eras);
    free(source);
    return source == nullptr;
}

static void TestTable(void)
{
    char* source = Source("<era type=\"235\" start=\"1989-01-08\" code=\"heisei\"/>"
                          "<era type=\"236\" start=\"2019-04-30\" code=\"abcdefg\"/>");
    const char* table = "const JapaneseEra mlocJapaneseEras[] = {\n"
                        "    {\"heisei\", 1989, 726110},\n"
                        "    {\"abcdefg\", 2019, 737179},\n"
                        "};\n"
                        "\n"
                        "const uint32_t mlocJapaneseEraCount = 2;\n";
    const char* at = source != nullptr ? strstr(source, "const JapaneseEra") : nullptr;
    CHECK(at != nullptr && strcmp(at, table) == 0,
          "each era's code (up to seven letters), year and first day, a month's last day "
          "included");
    free(source);
}

static void TestRefusals(void)
{
    bool none = Refused("") && strstr(s_error, "no japanese eras") != nullptr;
    char* source = nullptr;
    size_t size = 0;
    char error[256];
    CHECK(none &&
              !GenerateCalendarEras("no/such/directory", &source, &size, error, sizeof(error)) &&
              Refused("<era start=\"2019-05-01\"/>") && Refused("<era code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-02-29\" code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-13-01\" code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-5-01\" code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-0x-01\" code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"reiwa\"/>"
                      "<era start=\"2019-05-01\" code=\"other\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"Reiwa\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"longcode\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"ce\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"reiwa\"/>"
                      "<era start=\"1989-01-08\" code=\"heisei\"/>") &&
              Refused("<other start=\"2019-05-01\" code=\"reiwa\"/>"),
          "no eras or no file, a missing or malformed start or code, a Gregorian era's code, "
          "eras out of order or on one day");
}

static bool YearsRefused(const char* overrides)
{
    char* source = nullptr;
    size_t size = 0;
    char error[256];
    bool refused = !CalendarYearsSource(overrides, &source, &size, error, sizeof(error));
    free(source);
    return refused;
}

// A generated table's years, read back from its source; false if it does
// not hold lunarYearCount of them.
static bool ReadTable(const char* source, const char* name, uint32_t* out)
{
    const char* at = source != nullptr ? strstr(source, name) : nullptr;
    at = at != nullptr ? strchr(at, '{') : nullptr;
    if (at == nullptr)
    {
        return false;
    }
    at++;
    for (int i = 0; i < lunarYearCount; i++)
    {
        char* end = nullptr;
        out[i] = (uint32_t)strtoul(at, &end, 16);
        if (end == at || *end != ',')
        {
            return false;
        }
        at = end + 1;
    }
    while (*at == ' ' || *at == '\n')
    {
        at++;
    }
    return strncmp(at, "};", 2) == 0;
}

static uint32_t s_chinese[lunarYearCount];
static uint32_t s_dangi[lunarYearCount];

// The repository's overrides give the library's own tables, and its
// source file exactly.
static void TestRepositoryYears(const char* root)
{
    char* source = nullptr;
    size_t size = 0;
    char error[256];
    bool made = GenerateCalendarYears(root, &source, &size, error, sizeof(error));
    char path[4096];
    snprintf(path, sizeof(path), "%s/src/generated/calendar_years.c", root);
    FILE* file = fopen(path, "rb");
    static char committed[65536];
    size_t length = file != nullptr ? fread(committed, 1, sizeof(committed) - 1, file) : 0;
    if (file != nullptr)
    {
        fclose(file);
    }
    CHECK(made && ReadTable(source, "mlocChineseYears", s_chinese) &&
              ReadTable(source, "mlocDangiYears", s_dangi) &&
              memcmp(s_chinese, mlocChineseYears, sizeof(s_chinese)) == 0 &&
              memcmp(s_dangi, mlocDangiYears, sizeof(s_dangi)) == 0 && length == size &&
              memcmp(committed, source, size) == 0,
          "the repository's overrides give the compiled tables and their source");
    free(source);
    char missing[4096];
    snprintf(missing, sizeof(missing), "%s/no/such/directory", root);
    CHECK(!GenerateCalendarYears(missing, &source, &size, error, sizeof(error)),
          "no overrides file refused");
}

// One override of China's 1906 changes China's table there alone.
static void TestOneOverride(void)
{
    char* source = nullptr;
    size_t size = 0;
    char error[256];
    bool made = CalendarYearsSource("# a comment\n\nchinese 1906 1906-01-25 0110101010101 5 qing\n",
                                    &source, &size, error, sizeof(error));
    bool read = made && ReadTable(source, "mlocChineseYears", s_chinese) &&
                ReadTable(source, "mlocDangiYears", s_dangi);
    // Korea's Qing years are the model's again; its 1906 is the same
    // either way.
    static const int s_qingYears[] = {1903, 1904, 1905, 1908, 1911};
    bool modelInKorea = read;
    for (size_t i = 0; i < sizeof(s_qingYears) / sizeof(s_qingYears[0]); i++)
    {
        modelInKorea = modelInKorea && s_dangi[s_qingYears[i] - lunarFirstYear] !=
                                           mlocDangiYears[s_qingYears[i] - lunarFirstYear];
    }
    CHECK(read && memcmp(s_chinese, mlocChineseYears, sizeof(s_chinese)) == 0 && modelInKorea &&
              s_dangi[1906 - lunarFirstYear] == mlocDangiYears[1906 - lunarFirstYear],
          "an override in its own calendar's table alone");
    free(source);
}

// The model's own year as an override line.
static void LineOf(const char* calendar, int year, uint32_t bits, char* line, size_t room)
{
    char lengths[16] = "";
    uint32_t leap = (bits >> lunarLeapShift) & 0xF;
    uint32_t count = leap != 0 ? 13 : 12;
    for (uint32_t m = 0; m < count; m++)
    {
        lengths[m] = (char)('0' + ((bits >> m) & 1));
    }
    // Every table year begins in January or February.
    uint32_t offset = bits >> lunarOffsetShift;
    snprintf(line, room, "%s %d %d-%02u-%02u %s %u model\n", calendar, year, year,
             offset < 31 ? 1u : 2u, offset < 31 ? offset + 1 : offset - 30, lengths, leap);
}

static void TestTableEnds(void)
{
    char lines[256];
    char line[128];
    LineOf("chinese", 1600, mlocChineseYears[0], line, sizeof(line));
    snprintf(lines, sizeof(lines), "%s", line);
    LineOf("dangi", 2400, mlocDangiYears[lunarYearCount - 1], line, sizeof(line));
    snprintf(lines + strlen(lines), sizeof(lines) - strlen(lines), "%s", line);
    char* source = nullptr;
    size_t size = 0;
    char error[256];
    bool made = CalendarYearsSource(lines, &source, &size, error, sizeof(error));
    free(source);
    char longLine[300];
    memset(longLine, 'x', sizeof(longLine));
    longLine[0] = '#';
    longLine[256] = '\0';
    char exactLine[300];
    memset(exactLine, 'x', sizeof(exactLine));
    exactLine[0] = '#';
    exactLine[255] = '\0';
    char* fits = nullptr;
    bool fitsMade = CalendarYearsSource(exactLine, &fits, &size, error, sizeof(error));
    free(fits);
    CHECK(made && fitsMade && YearsRefused(longLine),
          "overrides of the tables' first and last years; a line of 255 bytes, none longer");
}

static void TestYearRefusals(void)
{
    CHECK(YearsRefused("chinese 1906 1906-01-26 0110101010101 5 qing\n") &&
              YearsRefused("chinese 1906 1906-01-25 0110101010101 0 qing\n") &&
              YearsRefused("chinese 1906 1906-01-25 011010101010 5 qing\n") &&
              YearsRefused("chinese 1906 1906-01-25 01101010101011 5 qing\n") &&
              YearsRefused("chinese 1906 1906-01-25 01101010101x1 5 qing\n") &&
              YearsRefused("chinese 1599 1599-01-25 011010101010 0 qing\n") &&
              YearsRefused("chinese 2401 2401-01-25 011010101010 0 qing\n") &&
              YearsRefused("korean 1906 1906-01-25 0110101010101 5 qing\n") &&
              YearsRefused("chinese 1906 1906-01-25 0110101010101 5\n") &&
              YearsRefused("chinese 1906 1906-07-02 0110101010101 5 qing\n") &&
              YearsRefused("chinese 1906 1906-01-25 0110101010101 5 qing\n"
                           "chinese 1906 1906-01-25 0110101010101 5 qing\n"),
          "an override that misses its neighbours, its leap month or its months, a year past "
          "the tables or not holding its July 1, an unknown calendar, a missing source, a "
          "year twice");
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return 1;
    }
    TestRepositoryYears(argv[1]);
    TestOneOverride();
    TestTableEnds();
    TestYearRefusals();
    TestTable();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
