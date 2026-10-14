// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's Japanese eras (record mloc-0040): a well-formed list
// becomes the table's source, each era's first day by the library's
// Gregorian arithmetic; every malformed list is refused.

#include "calendar_gen.h"
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

int main(void)
{
    TestTable();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
