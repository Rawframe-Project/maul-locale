// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Compares display names with ICU4C's. Reads lines "locale TAB code TAB
// mode TAB name" from standard input, as an oracle built on ICU4C's
// LocaleDisplayNames writes them with no substitution: the locale as ICU
// names it ("sr_Latn_BA"), a code with its namespace ("l:en-GB",
// "t:ca-gregory"), the mode (0 the standard form, 1 the short one, then
// the standard form at the beginning of a sentence, in a list or menu and
// alone), and the name, "<none>" for none. Prints each line whose name
// differs and a count; exits 1 on a difference.
//
// usage: conform_names names.mloc < oracle-lines

#include "maul-locale/data.h"
#include "maul-locale/names.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxLine = 4096,
    maxShown = 40,
};

static mlocData s_sources[2];

// Opens the compiled-in data and the names file; false on failure.
static bool Open(const char* path, mlocDataSet* set)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    static uint8_t* s_bytes;
    s_bytes = malloc((size_t)16 << 20);
    size_t read = s_bytes == nullptr ? 0 : fread(s_bytes, 1, (size_t)16 << 20, file);
    fclose(file);
    mlocOpenBuiltinData(&s_sources[0]);
    *set = (mlocDataSet){s_sources, 2};
    return read > 0 && mlocOpenData(&s_sources[1], s_bytes, read) == mloc_success;
}

// The library's name for a line's locale, code and mode, or "<none>".
static void Name(const mlocDataSet* set, char* id, const char* code, int mode, char* out,
                 size_t capacity)
{
    static const char s_types[] = "lsrvktm";
    for (char* at = id; *at != '\0'; at++)
    {
        *at = *at == '_' ? '-' : *at;
    }
    mlocLocale locale;
    const char* type = strchr(s_types, code[0]);
    mlocNameOptions options = {
        .form = mode == 1 ? mloc_nameFormShort : mloc_nameFormStandard,
        .fallback = mloc_nameFallbackNone,
        .capitalization = mode < 2 ? mloc_capitalizationNone : (mlocCapitalization)(mode - 1),
    };
    mlocOutput output = {.buffer = out, .capacity = capacity - 1};
    mlocResult r = mloc_errorInvalid;
    if (type != nullptr && code[1] == ':' && mlocParseLocale(id, strlen(id), &locale).status >= 0)
    {
        r = mlocGetName(set, &locale, (mlocNameType)(type - s_types), code + 2, strlen(code + 2),
                        &options, &output);
    }
    if (r == mloc_success)
    {
        out[output.length] = '\0';
    }
    else
    {
        snprintf(out, capacity, "%s", r == mloc_errorNotFound ? "<none>" : "<error>");
    }
}

int main(int argc, char** argv)
{
    mlocDataSet set;
    if (argc != 2 || !Open(argv[1], &set))
    {
        fputs("usage: conform_names names.mloc < oracle-lines\n", stderr);
        return 2;
    }
    static char s_line[maxLine];
    long lines = 0;
    long differ = 0;
    while (fgets(s_line, sizeof(s_line), stdin) != nullptr)
    {
        s_line[strcspn(s_line, "\r\n")] = '\0';
        char* id = strtok(s_line, "\t");
        char* code = strtok(nullptr, "\t");
        char* mode = strtok(nullptr, "\t");
        char* expected = strtok(nullptr, "");
        if (id == nullptr || code == nullptr || mode == nullptr || expected == nullptr)
        {
            continue;
        }
        char name[maxLine];
        char shownId[64];
        snprintf(shownId, sizeof(shownId), "%s", id);
        Name(&set, id, code, atoi(mode), name, sizeof(name));
        lines++;
        if (strcmp(name, expected) != 0)
        {
            if (differ++ < maxShown)
            {
                printf("%s %s %s: ICU \"%s\", here \"%s\"\n", shownId, code, mode, expected, name);
            }
        }
    }
    printf("%ld lines, %ld differ\n", lines, differ);
    return differ == 0 ? 0 : 1;
}
