// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale keys (keys.h).

#include "keys.h"

#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Parts
{
    char language[9];
    char script[5];
    char region[4];
    char variants[96];
} Parts;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

static bool AllOf(const char* text, size_t length, bool digits)
{
    for (size_t i = 0; i < length; i++)
    {
        bool digit = text[i] >= '0' && text[i] <= '9';
        bool letter = (text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z');
        if (digits ? !digit : !letter)
        {
            return false;
        }
    }
    return true;
}

// Splits a CLDR id into its parts; variants stay joined by "_".
static Parts Split(const char* id)
{
    Parts parts = {0};
    size_t length = strcspn(id, "_");
    if (length == 0 || length >= sizeof(parts.language))
    {
        Die("a locale id the generator cannot read", id);
    }
    memcpy(parts.language, id, length);
    const char* at = id + length;
    while (*at == '_')
    {
        at++;
        length = strcspn(at, "_");
        bool early = parts.region[0] == 0 && parts.variants[0] == 0;
        if (early && parts.script[0] == 0 && length == 4 && AllOf(at, 4, false))
        {
            memcpy(parts.script, at, 4);
        }
        else if (early &&
                 ((length == 2 && AllOf(at, 2, false)) || (length == 3 && AllOf(at, 3, true))))
        {
            memcpy(parts.region, at, length);
        }
        else if (strlen(parts.variants) + length + 2 < sizeof(parts.variants))
        {
            strncat(parts.variants, "_", 2);
            strncat(parts.variants, at, length);
        }
        else
        {
            Die("a locale id with more variants than the generator holds", id);
        }
        at += length;
    }
    return parts;
}

// The script likely subtags give a language with a script and region
// (either may be empty), as UTS #35 looks it up; empty when none.
static const char* LikelyScript(const Keys* keys, const Parts* parts, char* scratch)
{
    const char* tries[4];
    char ids[4][32];
    int count = 0;
    if (parts->script[0] != 0 && parts->region[0] != 0)
    {
        snprintf(ids[count], sizeof(ids[count]), "%s_%s_%s", parts->language, parts->script,
                 parts->region);
        tries[count] = ids[count];
        count++;
    }
    if (parts->script[0] != 0)
    {
        snprintf(ids[count], sizeof(ids[count]), "%s_%s", parts->language, parts->script);
        tries[count] = ids[count];
        count++;
    }
    if (parts->region[0] != 0)
    {
        snprintf(ids[count], sizeof(ids[count]), "%s_%s", parts->language, parts->region);
        tries[count] = ids[count];
        count++;
    }
    tries[count++] = parts->language;
    for (int i = 0; i < count; i++)
    {
        const char* to = MapGet(&keys->likely, tries[i]);
        if (to != nullptr)
        {
            Parts found = Split(to);
            snprintf(scratch, 5, "%s", found.script);
            return scratch;
        }
    }
    return "";
}

void KeyOf(const Keys* keys, const char* id, char* out, size_t capacity)
{
    if (strcmp(id, "root") == 0)
    {
        snprintf(out, capacity, "root");
        return;
    }
    Parts parts = Split(id);
    char scratch[5];
    char script[5];
    snprintf(script, sizeof(script), "%s",
             parts.script[0] != 0 ? parts.script : LikelyScript(keys, &parts, scratch));
    Parts alone = {0};
    memcpy(alone.language, parts.language, sizeof(alone.language));
    if (strcmp(script, LikelyScript(keys, &alone, scratch)) == 0)
    {
        script[0] = '\0';
    }
    if (strcmp(parts.language, "und") == 0 && script[0] == 0 && parts.region[0] == 0 &&
        parts.variants[0] == 0)
    {
        snprintf(out, capacity, "root");
        return;
    }
    int written = snprintf(out, capacity, "%s%s%s%s%s%s", parts.language, script[0] ? "_" : "",
                           script, parts.region[0] ? "_" : "", parts.region, parts.variants);
    if (written < 0 || (size_t)written >= capacity)
    {
        Die("a locale key longer than the generator holds", id);
    }
    for (char* v = out + strlen(out) - strlen(parts.variants); *v != '\0'; v++)
    {
        *v = *v >= 'a' && *v <= 'z' ? (char)(*v - 32) : *v;
    }
}

static char* Copy(const char* text)
{
    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    if (copy == nullptr)
    {
        Die("out of memory", text);
    }
    return memcpy(copy, text, length);
}

int KeysOpen(Keys* keys, const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    *keys = (Keys){0};
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/likelySubtags.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* list = XmlFindChild(root, "likelySubtags");
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const char* from = XmlGetAttribute(list->children[i], "from");
        const char* to = XmlGetAttribute(list->children[i], "to");
        if (from != nullptr && to != nullptr)
        {
            MapPut(&keys->likely, from, Copy(to));
        }
    }
    XmlFree(root);
    return 1;
}

static void FreeValue(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    free(value);
}

void KeysClose(Keys* keys)
{
    MapEach(&keys->likely, FreeValue, nullptr);
    MapFree(&keys->likely);
}
