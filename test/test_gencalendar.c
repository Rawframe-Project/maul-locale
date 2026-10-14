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

// The source made from a calendarData holding the given eras, or null.
static char* Source(const char* eras)
{
    char document[2048];
    snprintf(document, sizeof(document),
             "<supplementalData><calendarData><calendar type=\"gregorian\"/>"
             "<calendar type=\"japanese\"><eras>%s</eras></calendar></calendarData>"
             "</supplementalData>",
             eras);
    char error[256] = "";
    XmlNode* root = XmlParse(document, strlen(document), error, sizeof(error));
    char* source = nullptr;
    size_t size = 0;
    if (root == nullptr || !CalendarErasSource(root, &source, &size, error, sizeof(error)))
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
                          "<era type=\"236\" start=\"2019-05-01\" code=\"reiwa\"/>");
    CHECK(source != nullptr && strstr(source, "    {\"heisei\", 1989, 726110},\n") != nullptr &&
              strstr(source, "    {\"reiwa\", 2019, 737180},\n") != nullptr &&
              strstr(source, "mlocJapaneseEraCount = 2;") != nullptr,
          "each era's code, year and first day");
    free(source);
}

static void TestRefusals(void)
{
    CHECK(Refused("") && Refused("<era start=\"2019-05-01\"/>") &&
              Refused("<era code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-02-29\" code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-13-01\" code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-5-01\" code=\"reiwa\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"Reiwa\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"longcode\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"ce\"/>") &&
              Refused("<era start=\"2019-05-01\" code=\"reiwa\"/>"
                      "<era start=\"1989-01-08\" code=\"heisei\"/>") &&
              Refused("<other start=\"2019-05-01\" code=\"reiwa\"/>"),
          "no eras, a missing or malformed start or code, a Gregorian era's code, "
          "eras out of order");
}

int main(void)
{
    TestTable();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
