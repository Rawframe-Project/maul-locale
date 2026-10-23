// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's scripts written right to left: the RTL field of
// scriptMetadata.txt's lines becoming a sorted table, comments and blank
// lines skipped; a line without the field, a malformed code, a code twice
// or no script right to left refused.

#include "direction_gen.h"
#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

static char s_error[256];

static char* Source(const char* metadata)
{
    char* source = nullptr;
    size_t size = 0;
    s_error[0] = '\0';
    if (!ScriptDirectionsSource(metadata, &source, &size, s_error, sizeof(s_error)))
    {
        return nullptr;
    }
    CHECK(size == strlen(source), "the size is the source's");
    return source;
}

static bool Refused(const char* metadata)
{
    char* source = Source(metadata);
    free(source);
    return source == nullptr && s_error[0] != '\0';
}

static void TestTable(void)
{
    char* source = Source("# Field - Description\n"
                          "\n"
                          "Latn; 2; 004C; IT; 1; RECOMMENDED; NO; NO; MIN; NO; YES\n"
                          "Hebr; 9; 05D0; IL; 1; RECOMMENDED; YES; NO; MIN; NO; NO\r\n"
                          "Arab; 3; 0628; SA; 1; RECOMMENDED; YES; NO; YES; NO; NO\n"
                          "Zyyy; 1; 0040; ZZ; -1; RECOMMENDED; UNKNOWN; UNKNOWN; UNKNOWN; UNKNOWN; "
                          "UNKNOWN");
    const char* table = "const char mlocRightToLeftScripts[][4] = {\n"
                        "    {'A', 'r', 'a', 'b'},\n"
                        "    {'H', 'e', 'b', 'r'},\n"
                        "};\n"
                        "\n"
                        "const uint32_t mlocRightToLeftScriptCount = 2;\n";
    const char* at =
        source != nullptr ? strstr(source, "const char mlocRightToLeftScripts") : nullptr;
    CHECK(at != nullptr && strcmp(at, table) == 0, "the RTL scripts, sorted");
    free(source);
}

static void TestRefusals(void)
{
    CHECK(Refused("Arab; 3; 0628; SA; 1; RECOMMENDED\n") &&
              Refused("Arab; 3; 0628; SA; 1; RECOMMENDED; YES\nLatn; 2; 004C\n"),
          "a line without the RTL field");
    CHECK(Refused("arab; 3; 0628; SA; 1; RECOMMENDED; YES\n") &&
              Refused("Arabic; 3; 0628; SA; 1; RECOMMENDED; YES\n") &&
              Refused("ArAb; 3; 0628; SA; 1; RECOMMENDED; YES\n"),
          "a malformed script code");
    CHECK(Refused("Arab; 3; 0628; SA; 1; RECOMMENDED; YES\nArab; 3; 0628; SA; 1; X; YES\n"),
          "a code twice");
    CHECK(Refused("Latn; 2; 004C; IT; 1; RECOMMENDED; NO\n") && Refused("# only\n") && Refused(""),
          "no script right to left");
}

int main(int argc, char** argv)
{
    TestTable();
    TestRefusals();
    if (argc > 1)
    {
        char* source = nullptr;
        size_t size = 0;
        char common[4096];
        snprintf(common, sizeof(common), "%s/test/gendirection", argv[1]);
        CHECK(GenerateScriptDirections(common, &source, &size, s_error, sizeof(s_error)) &&
                  strstr(source, "    {'H', 'e', 'b', 'r'},\n    {'T', 'h', 'a', 'a'},\n};") !=
                      nullptr,
              "the file is read whole");
        free(source);
        source = nullptr;
        snprintf(common, sizeof(common), "%s/no-such-directory", argv[1]);
        CHECK(!GenerateScriptDirections(common, &source, &size, s_error, sizeof(s_error)) &&
                  strstr(s_error, "cannot open") != nullptr,
              "a missing file is refused");
    }
    return s_failures == 0 ? 0 : 1;
}
