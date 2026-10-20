// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Tailored locales' sort keys through the public collator (record
// mloc-0071) against ICU4C 79's. Reads conform_collation_builder
// --queries on standard input: for each locale file's type (not
// "private-", no alt but "short", the long form's strings keyed too),
// its tag (the file's locale, -u-co and the
// type's BCP 47 name) under four sets of options (none; kf upper; ka
// shifted with ks level4; kc), and the file's locale alone for its
// default type, with the strings the type's rules name and a fixed list
// of probes. --queries writes the tags and strings as the oracle reads
// them; --keys writes each string's key in hex, as the oracle writes
// ICU4C's; alone it prints how many keys it made and a hash of them, and
// with --expect it fails unless the hash is the given one, which CI
// checks on the pinned release.
//
// usage: conform_collation_builder FractionalUCA.txt --queries files |
//        conform_collation_keys <collation.mloc> [--queries | --keys |
//        --expect HASH]

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "maul-locale/collate.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxString = 64,
    maxKey = 8192,
};

static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
// The type's strings: their code points one after another, each string's
// start, and the end of the last.
static uint32_t* s_points;
static size_t s_pointCount;
static size_t s_pointCapacity;
static size_t* s_starts;
static size_t s_count;
static size_t s_startCapacity;
static uint64_t s_hash;
static size_t s_keys;
static size_t s_failed;
static bool s_print;
static bool s_queriesOnly;

// Code points every tag also reads: letters CLDR's tailorings move,
// case pairs, marks, digits, spaces and punctuation, other scripts.
static const uint32_t s_probes[][4] = {
    {'a'},
    {'A'},
    {0xE4},
    {'a', 'e'},
    {0xDF},
    {'s', 's'},
    {0xF1},
    {'c', 'h'},
    {'l', 'l'},
    {0xC4},
    {'1'},
    {'1', '0'},
    {'a', ' ', 'b'},
    {'-'},
    {0x3B1},
    {0x44F},
    {0x439},
    {0x4E00},
    {0x963F},
    {0xAC00},
    {0xE40, 0xE01},
    {0x1F600},
    {0x30A2},
    {0x3042},
    {0x5D0},
    {0x627},
    {0x915},
    {'z'},
    {0x17E},
    {0x10D},
    // Han from several radicals, and characters the root maps to a Han
    // character's primary: what the unihan types order by radical.
    {0x4E28},
    {0x2A6D9},
    {0x20000},
    {0x9F52},
    {0x5B57},
    {0x3400},
    {0x9FA5},
    {0x2F00},
    {0x2E80},
    {0x3220},
};

static size_t Utf8(const uint32_t* text, size_t length, char* out)
{
    size_t n = 0;
    for (size_t i = 0; i < length; i++)
    {
        uint32_t c = text[i];
        if (c < 0x80)
        {
            out[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            out[n++] = (char)(0xC0 | c >> 6);
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out[n++] = (char)(0xE0 | c >> 12);
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            out[n++] = (char)(0xF0 | c >> 18);
            out[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return n;
}

static void Write(const uint32_t* text, size_t length)
{
    printf("q");
    for (size_t i = 0; i < length; i++)
    {
        printf(" %04X", text[i]);
    }
    printf("\n");
}

// One string's key under the collator, hashed or written.
static void Key(const mlocCollator* c, bool made, const uint32_t* text, size_t length)
{
    if (s_queriesOnly)
    {
        Write(text, length);
        return;
    }
    char utf8[maxString * 4];
    static uint8_t key[maxKey];
    size_t n = 0;
    if (!made ||
        mlocGetSortKey(c, utf8, Utf8(text, length, utf8), key, sizeof(key), &n) != mloc_success)
    {
        s_failed++;
        s_hash = s_hash * 1000003U + 0xE770;
        if (s_print)
        {
            printf("k error\n");
        }
        return;
    }
    s_keys++;
    if (s_print)
    {
        printf("k ");
    }
    for (size_t i = 0; i < n; i++)
    {
        s_hash = s_hash * 1000003U + key[i];
        if (s_print)
        {
            printf("%02X", key[i]);
        }
    }
    if (s_print)
    {
        printf("\n");
    }
}

// The strings of the type read so far, and the probes, under one tag.
static void Tag(const char* tag)
{
    if (s_print || s_queriesOnly)
    {
        printf("tag %s\n", tag);
    }
    mlocLocale locale;
    mlocCollator c;
    bool made = mlocParseLocale(tag, strlen(tag), &locale).status == mloc_success &&
                mlocInitCollator(&c, &s_data, &locale, nullptr) == mloc_success;
    for (size_t i = 0; i < s_count; i++)
    {
        Key(&c, made, s_points + s_starts[i], s_starts[i + 1] - s_starts[i]);
    }
    for (size_t i = 0; i < sizeof(s_probes) / sizeof(s_probes[0]); i++)
    {
        size_t length = 0;
        while (length < 4 && s_probes[i][length] != 0)
        {
            length++;
        }
        Key(&c, made, s_probes[i], length);
    }
}

// BCP 47's co value of a CLDR type name.
static const char* Code(const char* type)
{
    static const char* const codes[][2] = {{"phonebook", "phonebk"},
                                           {"traditional", "trad"},
                                           {"dictionary", "dict"},
                                           {"gb2312han", "gb2312"}};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        if (strcmp(type, codes[i][0]) == 0)
        {
            return codes[i][1];
        }
    }
    return type;
}

// The tags of one type: its file's locale alone when the file is new,
// then the type's four sets of options.
static void Type(const char* file, const char* type, bool first)
{
    static const char* const options[] = {"", "-kf-upper", "-ka-shifted-ks-level4", "-kc"};
    char base[96];
    bool root = strcmp(file, "root") == 0;
    snprintf(base, sizeof(base), "%s", root ? "und" : file);
    for (char* p = base; *p != '\0'; p++)
    {
        *p = *p == '_' ? '-' : *p;
    }
    if (first)
    {
        Tag(base);
    }
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++)
    {
        char tag[160];
        snprintf(tag, sizeof(tag), "%s-u-co-%s%s", base, Code(type), options[i]);
        Tag(tag);
    }
}

static bool Open(const char* path, uint8_t** bytes)
{
    FILE* f = fopen(path, "rb");
    if (f == nullptr)
    {
        return false;
    }
    long size = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
    if (size <= 0 || fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return false;
    }
    *bytes = malloc((size_t)size);
    bool ok = *bytes != nullptr && fread(*bytes, 1, (size_t)size, f) == (size_t)size &&
              mlocOpenData(&s_sources[1], *bytes, (size_t)size) == mloc_success;
    fclose(f);
    return ok;
}

static void* Grow(void* items, size_t* capacity, size_t needed, size_t size)
{
    if (needed <= *capacity)
    {
        return items;
    }
    *capacity = needed * 2;
    void* grown = realloc(items, *capacity * size);
    if (grown == nullptr)
    {
        fputs("out of memory\n", stderr);
        exit(1);
    }
    return grown;
}

// A query line's code points, as the type's next string.
static void Read(const char* line)
{
    s_starts = Grow(s_starts, &s_startCapacity, s_count + 2, sizeof *s_starts);
    s_starts[s_count] = s_pointCount;
    size_t length = 0;
    for (const char* p = line + 1; *p != '\0' && *p != '\n' && length < maxString;)
    {
        char* end = nullptr;
        unsigned long c = strtoul(p, &end, 16);
        if (end == p)
        {
            break;
        }
        s_points = Grow(s_points, &s_pointCapacity, s_pointCount + 1, sizeof *s_points);
        s_points[s_pointCount++] = (uint32_t)c;
        length++;
        p = end;
    }
    s_count += length > 0;
    s_starts[s_count] = s_pointCount;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <collation.mloc> [--queries | --keys | --expect HASH]\n",
                argv[0]);
        return 2;
    }
    s_queriesOnly = argc > 2 && strcmp(argv[2], "--queries") == 0;
    s_print = argc > 2 && strcmp(argv[2], "--keys") == 0;
    const char* expected = argc > 3 && strcmp(argv[2], "--expect") == 0 ? argv[3] : nullptr;
    uint8_t* bytes = nullptr;
    mlocOpenBuiltinData(&s_sources[0]);
    if (!Open(argv[1], &bytes))
    {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        free(bytes);
        return 1;
    }
    static char line[1 << 16];
    char file[64] = "";
    char type[64] = "";
    char last[64] = "";
    bool selectable = false;
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        char alt[64] = "";
        if (strncmp(line, "type ", 5) == 0)
        {
            s_count = 0;
            s_pointCount = 0;
            int fields = sscanf(line + 5, "%63s %63s %63s", file, type, alt);
            selectable = (fields == 2 || strcmp(alt, "short") == 0) &&
                         strncmp(type, "private-", 8) != 0 && strcmp(file, "crafted") != 0;
        }
        else if (strncmp(line, "q ", 2) == 0 && selectable)
        {
            Read(line);
        }
        else if (strncmp(line, "end", 3) == 0 && selectable)
        {
            Type(file, type, strcmp(file, last) != 0);
            snprintf(last, sizeof(last), "%s", file);
        }
    }
    free(bytes);
    free(s_points);
    free(s_starts);
    if (s_print || s_queriesOnly)
    {
        return 0;
    }
    char hash[32];
    snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)s_hash);
    printf("keys: %zu made, %zu refused, hash %s\n", s_keys, s_failed, hash);
    bool mismatch = expected != nullptr && strcmp(expected, hash) != 0;
    if (mismatch)
    {
        fprintf(stderr, "expected keys %s\n", expected);
    }
    return mismatch || s_failed != 0;
}
