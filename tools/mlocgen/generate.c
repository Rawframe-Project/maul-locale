// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's generated sources (generate.h).

#include "generate.h"

#include "data_format.h"
#include "locale_gen.h"
#include "number_gen.h"
#include "plural_gen.h"
#include "unit_gen.h"

#include "maul-locale/base.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Text
{
    char* bytes;
    size_t size;
    size_t capacity;
} Text;

[[noreturn]] static void Die(const char* message)
{
    fprintf(stderr, "mlocgen: %s\n", message);
    exit(1);
}

[[gnu::format(printf, 2, 3)]] static void Append(Text* text, const char* format, ...)
{
    for (;;)
    {
        va_list arguments;
        va_start(arguments, format);
        size_t room = text->capacity - text->size;
        int written = vsnprintf(text->bytes == nullptr ? nullptr : text->bytes + text->size, room,
                                format, arguments);
        va_end(arguments);
        if (written < 0)
        {
            Die("cannot format generated text");
        }
        if ((size_t)written < room)
        {
            text->size += (size_t)written;
            return;
        }
        size_t next = text->capacity == 0 ? 4096 : text->capacity * 2;
        while (next - text->size <= (size_t)written)
        {
            next *= 2;
        }
        char* grown = realloc(text->bytes, next);
        if (grown == nullptr)
        {
            Die("out of memory");
        }
        text->bytes = grown;
        text->capacity = next;
    }
}

static uint64_t HashBytes(uint64_t hash, const void* bytes, size_t size)
{
    const unsigned char* b = bytes;
    for (size_t i = 0; i < size; i++)
    {
        hash = (hash ^ b[i]) * 0x100000001B3u;
    }
    return hash;
}

// A whole file, or null with a message.
static char* ReadFile(const char* path, size_t* sizeOut, char* errorOut, size_t errorCapacity)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        snprintf(errorOut, errorCapacity, "cannot open %s", path);
        return nullptr;
    }
    Text text = {0};
    char block[4096];
    size_t got = 0;
    while ((got = fread(block, 1, sizeof(block), file)) > 0)
    {
        Append(&text, "%.*s", (int)got, block);
    }
    int failed = ferror(file);
    fclose(file);
    if (failed)
    {
        free(text.bytes);
        snprintf(errorOut, errorCapacity, "cannot read %s", path);
        return nullptr;
    }
    Append(&text, "%s", "");
    *sizeOut = text.size;
    return text.bytes;
}

// The value of "key value" in a pin file's text, or 0.
static int PinField(const char* text, const char* key, char* out, size_t capacity)
{
    size_t keyLength = strlen(key);
    for (const char* line = text; line != nullptr; line = strchr(line, '\n'))
    {
        line += *line == '\n';
        if (strncmp(line, key, keyLength) == 0 && line[keyLength] == ' ')
        {
            const char* value = line + keyLength + 1;
            size_t length = strcspn(value, "\n");
            if (length == 0 || length >= capacity)
            {
                return 0;
            }
            memcpy(out, value, length);
            out[length] = '\0';
            return 1;
        }
    }
    return 0;
}

int GenerateVersion(const char* root, DataFileVersion* versionOut, char* errorOut,
                    size_t errorCapacity)
{
    char path[4096];
    size_t cldrSize = 0;
    size_t tzdbSize = 0;
    snprintf(path, sizeof(path), "%s/tools/cldr-release.txt", root);
    char* cldr = ReadFile(path, &cldrSize, errorOut, errorCapacity);
    snprintf(path, sizeof(path), "%s/tools/tzdb-release.txt", root);
    char* tzdb = cldr == nullptr ? nullptr : ReadFile(path, &tzdbSize, errorOut, errorCapacity);
    char unicode[32] = "";
    char release[16] = "";
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    int ok = tzdb != nullptr;
    if (ok && (!PinField(cldr, "unicode", unicode, sizeof(unicode)) ||
               sscanf(unicode, "%u.%u.%u", &major, &minor, &patch) != 3))
    {
        snprintf(errorOut, errorCapacity, "tools/cldr-release.txt names no unicode version");
        ok = 0;
    }
    if (ok && (!PinField(tzdb, "release", release, sizeof(release)) || strlen(release) > 8))
    {
        snprintf(errorOut, errorCapacity, "tools/tzdb-release.txt names no release");
        ok = 0;
    }
    if (ok)
    {
        *versionOut = (DataFileVersion){
            .cldr = mlocPackVersion(MLOC_CLDR_VERSION_MAJOR, MLOC_CLDR_VERSION_MINOR,
                                    MLOC_CLDR_VERSION_PATCH),
            .unicode = mlocPackVersion(major, minor, patch),
            .library = mlocPackVersion(MLOC_VERSION_MAJOR, MLOC_VERSION_MINOR, MLOC_VERSION_PATCH),
        };
        memcpy(versionOut->tzdb, release, strlen(release));
        uint64_t hash = HashBytes(0xCBF29CE484222325u, cldr, cldrSize);
        versionOut->tables = HashBytes(hash, tzdb, tzdbSize);
    }
    free(cldr);
    free(tzdb);
    return ok;
}

static int WriteFile(const char* root, const char* name, const Text* text, char* errorOut,
                     size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/src/generated/%s", root, name);
    FILE* file = fopen(path, "wb");
    if (file == nullptr)
    {
        snprintf(errorOut, errorCapacity, "cannot create %s", path);
        return 0;
    }
    size_t written = fwrite(text->bytes, 1, text->size, file);
    if (fclose(file) != 0 || written != text->size)
    {
        snprintf(errorOut, errorCapacity, "cannot write %s", path);
        return 0;
    }
    return 1;
}

static void VersionHeader(Text* text, const DataFileVersion* version, const char* tzdb)
{
    Append(text, "%s",
           "// SPDX-License-Identifier: MIT\n"
           "// Copyright (c) 2026 Sirac Ozmen\n"
           "//\n"
           "// The data version this library reads (record mloc-0005). Generated by\n"
           "// tools/mlocgen from tools/cldr-release.txt, tools/tzdb-release.txt and\n"
           "// the generated code tables; do not edit.\n"
           "// clang-format off\n\n"
           "#ifndef MLOC_GENERATED_DATA_VERSION_H\n"
           "#define MLOC_GENERATED_DATA_VERSION_H\n\n"
           "#include <stdint.h>\n\n");
    Append(text, "#define MLOC_DATA_UNICODE_MAJOR %u\n", version->unicode >> 16);
    Append(text, "#define MLOC_DATA_UNICODE_MINOR %u\n", version->unicode >> 8 & 0xFF);
    Append(text, "#define MLOC_DATA_UNICODE_PATCH %u\n", version->unicode & 0xFF);
    Append(text, "#define MLOC_DATA_TZDB \"%s\"\n", tzdb);
    Append(text, "#define MLOC_DATA_TABLES_HASH UINT64_C(0x%016llX)\n\n",
           (unsigned long long)version->tables);
    Append(text, "%s", "#endif\n");
}

static void BuiltinSource(Text* text, const uint8_t* bytes, size_t size)
{
    Append(text,
           "// SPDX-License-Identifier: MIT\n"
           "// Copyright (c) 2026 Sirac Ozmen\n"
           "//\n"
           "// The compiled-in data, a data file's bytes (record mloc-0014).\n"
           "// Generated by tools/mlocgen; do not edit. %zu bytes.\n"
           "// clang-format off\n\n"
           "#include <stdint.h>\n\n"
           "alignas(8) const uint8_t mlocBuiltinData[%zu] = {\n",
           size, size);
    for (size_t i = 0; i < size; i++)
    {
        Append(text, "%s%u,%s", i % 16 == 0 ? "    " : " ", bytes[i],
               i % 16 == 15 || i + 1 == size ? "\n" : "");
    }
    Append(text, "};\n\nconst uint32_t mlocBuiltinDataSize = %zu;\n", size);
}

int Generate(const char* commonDirectory, const char* root, char* errorOut, size_t errorCapacity)
{
    DataFileVersion version;
    if (!GenerateVersion(root, &version, errorOut, errorCapacity))
    {
        return 0;
    }
    DataFile data = {0};
    DataFile names = {0};
    DataFile unitNames = {0};
    Text rules = {0};
    if (!GenerateLocaleUnits(commonDirectory, &data, errorOut, errorCapacity) ||
        !GeneratePluralUnits(commonDirectory, &data, &rules.bytes, &rules.size, errorOut,
                             errorCapacity) ||
        !GenerateNumberUnits(commonDirectory, &data, &names, &unitNames, errorOut, errorCapacity) ||
        !GenerateUnitTable(commonDirectory, &data, errorOut, errorCapacity))
    {
        DataFileFree(&data);
        DataFileFree(&names);
        DataFileFree(&unitNames);
        return 0;
    }
    // The compiled rules are code tables the data's indexes name, so they
    // enter the data version (record mloc-0020).
    version.tables = HashBytes(version.tables, rules.bytes, rules.size);
    char tzdb[9] = "";
    memcpy(tzdb, version.tzdb, sizeof(version.tzdb));
    Text header = {0};
    VersionHeader(&header, &version, tzdb);
    size_t size = 0;
    uint8_t* bytes = DataFileWrite(&data, &version, &size);
    Text builtin = {0};
    BuiltinSource(&builtin, bytes, size);
    // The currency names ship as a data file of their own, which a
    // program adds to its data set (record mloc-0022).
    size_t namesSize = 0;
    uint8_t* namesBytes = DataFileWrite(&names, &version, &namesSize);
    Text namesFile = {.bytes = (char*)namesBytes, .size = namesSize};
    // So do the unit names (record mloc-0024).
    size_t unitNamesSize = 0;
    uint8_t* unitNamesBytes = DataFileWrite(&unitNames, &version, &unitNamesSize);
    Text unitNamesFile = {.bytes = (char*)unitNamesBytes, .size = unitNamesSize};
    int ok = WriteFile(root, "data_version.h", &header, errorOut, errorCapacity) &&
             WriteFile(root, "builtin_data.c", &builtin, errorOut, errorCapacity) &&
             WriteFile(root, "plural_rules.c", &rules, errorOut, errorCapacity) &&
             WriteFile(root, "currency-names.mloc", &namesFile, errorOut, errorCapacity) &&
             WriteFile(root, "units.mloc", &unitNamesFile, errorOut, errorCapacity);
    free(namesBytes);
    free(unitNamesBytes);
    DataFileFree(&names);
    DataFileFree(&unitNames);
    free(header.bytes);
    free(builtin.bytes);
    free(rules.bytes);
    free(bytes);
    DataFileFree(&data);
    return ok;
}
