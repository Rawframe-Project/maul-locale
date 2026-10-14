// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's localeDisplayName.txt (record mloc-0038): each locale id's name
// in each display locale, in standard and dialect mode, against
// mlocGetLocaleName, with the names and subdivisions data files. A line of the exceptions file,
// tab-separated, is a display locale, a mode, an id, the name CLDR's data gives here and the
// reason; each must be used. Prints each difference and a count; exits 1 on a difference.
//
// usage: conform_locale_names names.mloc subdivisions.mloc localeDisplayName.txt
//        exceptions.txt

#include "maul-locale/data.h"
#include "maul-locale/names.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxLine = 2048,
    maxExceptions = 512,
    maxShown = 60,
};

typedef struct Exception
{
    char* key;
    char* name;
    bool used;
} Exception;

static mlocData s_sources[3];
static Exception s_exceptions[maxExceptions];
static int s_exceptionCount;

// Opens a data file into a source; its bytes live as long as the program.
static bool OpenFile(const char* path, mlocData* source)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    uint8_t* bytes = malloc((size_t)16 << 20);
    size_t read = bytes == nullptr ? 0 : fread(bytes, 1, (size_t)16 << 20, file);
    fclose(file);
    return read > 0 && mlocOpenData(source, bytes, read) == mloc_success;
}

static bool Open(const char* names, const char* subdivisions, mlocDataSet* set)
{
    mlocOpenBuiltinData(&s_sources[0]);
    *set = (mlocDataSet){s_sources, 3};
    return OpenFile(names, &s_sources[1]) && OpenFile(subdivisions, &s_sources[2]);
}

static char* Copy(const char* text)
{
    size_t length = strlen(text);
    char* copy = malloc(length + 1);
    if (copy != nullptr)
    {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

// Reads the exceptions: "display TAB mode TAB id TAB name TAB reason".
static bool ReadExceptions(const char* path)
{
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        return false;
    }
    char line[maxLine];
    while (fgets(line, sizeof(line), file) != nullptr && s_exceptionCount < maxExceptions)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }
        char* display = strtok(line, "\t");
        char* mode = strtok(nullptr, "\t");
        char* id = strtok(nullptr, "\t");
        char* name = strtok(nullptr, "\t");
        if (display == nullptr || mode == nullptr || id == nullptr || name == nullptr)
        {
            continue;
        }
        char key[maxLine];
        snprintf(key, sizeof(key), "%s\t%s\t%s", display, mode, id);
        s_exceptions[s_exceptionCount++] = (Exception){Copy(key), Copy(name), false};
    }
    fclose(file);
    return true;
}

// The name an exception gives for a line, or null.
static const char* Excepted(const char* display, const char* mode, const char* id)
{
    char key[maxLine];
    snprintf(key, sizeof(key), "%s\t%s\t%s", display, mode, id);
    for (int i = 0; i < s_exceptionCount; i++)
    {
        if (s_exceptions[i].key != nullptr && strcmp(s_exceptions[i].key, key) == 0)
        {
            s_exceptions[i].used = true;
            return s_exceptions[i].name;
        }
    }
    return nullptr;
}

// The library's name of an id, or an error mark.
static void Name(const mlocDataSet* set, const mlocLocale* display, bool dialect, const char* id,
                 char* out, size_t capacity)
{
    mlocLocale locale;
    mlocLocaleNameOptions options = {
        .languageDisplay = dialect ? mloc_languageDisplayDialect : mloc_languageDisplayStandard,
    };
    mlocOutput output = {.buffer = out, .capacity = capacity - 1};
    mlocResult r = mloc_errorInvalid;
    if (mlocParseLocale(id, strlen(id), &locale).status >= 0)
    {
        r = mlocGetLocaleName(set, display, &locale, &options, &output);
    }
    if (r >= 0)
    {
        out[output.length] = '\0';
    }
    else
    {
        snprintf(out, capacity, "<error %d>", (int)r);
    }
}

int main(int argc, char** argv)
{
    mlocDataSet set;
    FILE* in = argc == 5 ? fopen(argv[3], "r") : nullptr;
    if (in == nullptr || !Open(argv[1], argv[2], &set) || !ReadExceptions(argv[4]))
    {
        fputs("usage: conform_locale_names names.mloc subdivisions.mloc localeDisplayName.txt "
              "exceptions.txt\n",
              stderr);
        return 2;
    }
    char displayId[64] = "en";
    bool dialect = false;
    mlocLocale display;
    long lines = 0;
    long differ = 0;
    char line[maxLine];
    while (fgets(line, sizeof(line), in) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "@locale=", 8) == 0)
        {
            snprintf(displayId, sizeof(displayId), "%.63s", line + 8);
            continue;
        }
        if (strncmp(line, "@languageDisplay=", 17) == 0)
        {
            dialect = strcmp(line + 17, "dialect") == 0;
            continue;
        }
        char* semicolon = strstr(line, "; ");
        if (line[0] == '#' || semicolon == nullptr)
        {
            continue;
        }
        *semicolon = '\0';
        const char* expected = semicolon + 2;
        char tag[64];
        snprintf(tag, sizeof(tag), "%.63s", displayId);
        for (char* at = tag; *at != '\0'; at++)
        {
            *at = *at == '_' ? '-' : *at;
        }
        char name[maxLine];
        if (mlocParseLocale(tag, strlen(tag), &display).status < 0)
        {
            snprintf(name, sizeof(name), "<display %.63s>", displayId);
        }
        else
        {
            Name(&set, &display, dialect, line, name, sizeof(name));
        }
        const char* mode = dialect ? "dialect" : "standard";
        const char* excepted = Excepted(displayId, mode, line);
        lines++;
        if (strcmp(name, excepted != nullptr ? excepted : expected) != 0 && differ++ < maxShown)
        {
            printf("%s %s %s: CLDR \"%s\", here \"%s\"\n", displayId, mode, line, expected, name);
        }
    }
    fclose(in);
    for (int i = 0; i < s_exceptionCount; i++)
    {
        if (!s_exceptions[i].used)
        {
            printf("exception not used: %s\n", s_exceptions[i].key);
            differ++;
        }
    }
    printf("%ld lines, %ld differ\n", lines, differ);
    return differ == 0 ? 0 : 1;
}
