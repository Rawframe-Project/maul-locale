// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The names (names_gen.h). The codes are those CLDR's locale files name
// at or above the draft threshold, keys and values by their BCP 47 ids
// (common/bcp47's aliases: "calendar" is "ca", "gregorian" "gregory").
// A field's value is CLDR's resolution of it, a field CLDR lacks taking
// the name's. A key files it where the library's lookup along its chain
// (the first key filing the field, else the first filing the name) would
// give another. Locales are taken parent first, so a key's chain is
// filed before it; a locale under its parent's key files nothing and is
// checked to resolve to what the key gives. Subdivisions are left to
// their own file.

#include "names_gen.h"

#include "kinds.h"
#include "names_tables.h"
#include "table_gen.h"
#include "xml.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxCodes = nameMaxCodes,
    maxKey = 64,
    maxText = 128,
};

static const char s_prefix[] = "//ldml/localeDisplayNames/";
static const char s_emptyMarker[] = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";
static const char s_inheritMarker[] = "\xE2\x86\x91\xE2\x86\x91\xE2\x86\x91";

static const uint32_t s_kinds[] = {kindNamesLanguages, kindNamesScripts, kindNamesRegions,
                                   kindNamesVariants,  kindNamesKeys,    kindNamePatterns};

// A code and the CLDR chain of each of its fields; null where no locale
// has the field.
typedef struct Code
{
    char* code;
    char* chains[nameFieldCount];
} Code;

// The fields a locale files, per code; null where none.
typedef struct Filed
{
    const char** rows[maxCodes];
} Filed;

typedef struct Names
{
    const NamesInput* input;
    // An LDML key to its BCP 47 id; "id/value" to the BCP 47 value.
    Map keys;
    Map values;
    // A code to its index plus one.
    Map index;
    Code codes[maxCodes];
    int count;
    Map filed;
} Names;

static char* Copy(const char* text)
{
    size_t length = strlen(text);
    char* copy = GenAllocate(length + 1);
    memcpy(copy, text, length);
    return copy;
}

static void Lower(char* text)
{
    for (; *text != '\0'; text++)
    {
        *text = *text >= 'A' && *text <= 'Z' ? (char)(*text + 32) : *text;
    }
}

// Maps each word of a space-separated list, under a key's id, to a value
// ("ca" and "gregorian" to "gregory"); a word mapped already keeps its
// value. With no id, maps each word as a key to the value.
static void PutWords(Names* n, const char* id, const char* list, const char* value)
{
    for (const char* at = list != nullptr ? list : ""; *at != '\0';)
    {
        size_t length = strcspn(at, " ");
        char word[maxText * 2];
        if (id != nullptr)
        {
            snprintf(word, sizeof(word), "%s/%.*s", id, (int)length, at);
        }
        else
        {
            snprintf(word, sizeof(word), "%.*s", (int)length, at);
        }
        Map* map = id != nullptr ? &n->values : &n->keys;
        if (length > 0 && MapGet(map, word) == nullptr)
        {
            MapPut(map, word, Copy(value));
        }
        at += length;
        at += *at == ' ';
    }
}

// Reads a common/bcp47 file's keys and values.
static int ReadKeys(Names* n, const char* path, char* errorOut, size_t errorCapacity)
{
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    const XmlNode* keyword = root != nullptr ? XmlFindChild(root, "keyword") : nullptr;
    for (int k = 0; keyword != nullptr && k < keyword->childCount; k++)
    {
        const XmlNode* key = keyword->children[k];
        const char* id = XmlGetAttribute(key, "name");
        if (strcmp(key->name, "key") != 0 || id == nullptr)
        {
            continue;
        }
        PutWords(n, nullptr, id, id);
        PutWords(n, nullptr, XmlGetAttribute(key, "alias"), id);
        // A value's own id first, so that no other value's alias takes it.
        for (int pass = 0; pass < 2; pass++)
        {
            for (int t = 0; t < key->childCount; t++)
            {
                const char* value = XmlGetAttribute(key->children[t], "name");
                const char* alias = XmlGetAttribute(key->children[t], "alias");
                if (value != nullptr)
                {
                    PutWords(n, id, pass == 0 ? value : alias, value);
                }
            }
        }
    }
    bool read = keyword != nullptr;
    if (root != nullptr && !read)
    {
        snprintf(errorOut, errorCapacity, "%s has no keyword element", path);
    }
    XmlFree(root);
    return read;
}

static int ReadAllKeys(Names* n, char* errorOut, size_t errorCapacity)
{
    static const char* const s_files[] = {
        "calendar",
        "collation",
        "currency",
        "measure",
        "number",
        "segmentation",
        "timezone",
        "transform",
        "transform_hybrid",
        "transform_ime",
        "transform_keyboard",
        "transform_mt",
        "transform_private_use",
        "transform-destination",
        "variant",
    };
    char path[4096];
    for (size_t f = 0; f < sizeof(s_files) / sizeof(s_files[0]); f++)
    {
        snprintf(path, sizeof(path), "%s/bcp47/%s.xml", n->input->commonDirectory, s_files[f]);
        if (!ReadKeys(n, path, errorOut, errorCapacity))
        {
            return 0;
        }
    }
    return 1;
}

// An attribute's value in a chain's last step, or false.
static bool Attribute(const char* step, const char* name, char* out, size_t capacity)
{
    char marker[32];
    snprintf(marker, sizeof(marker), "[@%s=\"", name);
    const char* at = strstr(step, marker);
    if (at == nullptr)
    {
        return false;
    }
    at += strlen(marker);
    size_t length = strcspn(at, "\"");
    if (length >= capacity || at[length] != '"')
    {
        GenDie("a display name attribute too long", step);
    }
    memcpy(out, at, length);
    out[length] = '\0';
    return true;
}

// A language id in BCP 47's case and syntax ("en_GB" is "en-GB",
// "be_TARASK" "be-tarask").
static void LanguageCode(const char* type, char* out, size_t capacity)
{
    size_t at = (size_t)snprintf(out, capacity, "l:");
    for (int subtag = 0; *type != '\0' && at + 1 < capacity; subtag++)
    {
        size_t length = strcspn(type, "_");
        bool digits = length == 3 && type[0] >= '0' && type[0] <= '9';
        for (size_t i = 0; i < length && at + 1 < capacity; i++)
        {
            char c = type[i];
            bool upper = subtag > 0 && ((length == 4 && i == 0) || length == 2 || digits);
            c = upper && c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
            out[at++] = !upper && c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
        }
        type += length;
        if (*type == '_')
        {
            type++;
            out[at++] = '-';
        }
    }
    out[at < capacity ? at : capacity - 1] = '\0';
}

static const char* KeyId(const Names* n, const char* key)
{
    const char* id = MapGet(&n->keys, key);
    if (id == nullptr && (strcmp(key, "t") == 0 || strcmp(key, "x") == 0))
    {
        return key;
    }
    if (id == nullptr)
    {
        GenDie("a key BCP 47 does not name", key);
    }
    return id;
}

static const char* ValueId(const Names* n, const char* key, const char* value)
{
    char both[maxText * 2];
    snprintf(both, sizeof(both), "%s/%s", key, value);
    const char* id = MapGet(&n->values, both);
    if (id == nullptr)
    {
        Lower(both);
        id = MapGet(&n->values, both);
    }
    if (id == nullptr)
    {
        GenDie("a key's value BCP 47 does not name", both);
    }
    return id;
}

// The code of a pattern element, or false for none of them.
static bool PatternCode(const char* element, const char* type, char* out, size_t capacity)
{
    static const char* const s_map[][3] = {
        {"localePattern", "", "p:localePattern"},
        {"localeSeparator", "", "p:localeSeparator"},
        {"localeKeyTypePattern", "", "p:localeKeyTypePattern"},
        {"codePattern", "language", "c:language"},
        {"codePattern", "script", "c:script"},
        {"codePattern", "territory", "c:region"},
        {"measurementSystemName", "metric", "m:metric"},
        {"measurementSystemName", "UK", "m:uksystem"},
        {"measurementSystemName", "US", "m:ussystem"},
        {"typeValue", "yes", "b:yes"},
        {"typeValue", "no", "b:no"},
    };
    for (size_t i = 0; i < sizeof(s_map) / sizeof(s_map[0]); i++)
    {
        if (strcmp(element, s_map[i][0]) == 0 && strcmp(type, s_map[i][1]) == 0)
        {
            snprintf(out, capacity, "%s", s_map[i][2]);
            return true;
        }
    }
    return false;
}

// The code a chain's element and attributes name, or false for a
// subdivision.
static bool CodeOf(const Names* n, const char* element, const char* step, char* out,
                   size_t capacity)
{
    char type[maxText] = "";
    char key[maxText] = "";
    (void)Attribute(step, "type", type, sizeof(type));
    if (strcmp(element, "language") == 0)
    {
        LanguageCode(type, out, capacity);
    }
    else if (strcmp(element, "script") == 0 || strcmp(element, "territory") == 0)
    {
        snprintf(out, capacity, "%c:%s", element[0] == 's' ? 's' : 'r', type);
    }
    else if (strcmp(element, "variant") == 0)
    {
        Lower(type);
        snprintf(out, capacity, "v:%s", type);
    }
    else if (strcmp(element, "key") == 0)
    {
        snprintf(out, capacity, "k:%s", KeyId(n, type));
    }
    else if (strcmp(element, "type") == 0 && Attribute(step, "key", key, sizeof(key)))
    {
        const char* id = KeyId(n, key);
        snprintf(out, capacity, "t:%s-%s", id, ValueId(n, id, type));
    }
    else if (strcmp(element, "subdivision") == 0)
    {
        return false;
    }
    else if (!PatternCode(element, type, out, capacity))
    {
        GenDie("a display name element the generator does not know", step);
    }
    return true;
}

// The field a chain's attributes name; an alt that is no field goes
// after the code.
static NameField FieldOf(const char* step, char* code, size_t capacity)
{
    static const char* const s_alts[nameFieldCount] = {
        "", "short", "long", "variant", "menu", "", "", "stand-alone",
    };
    char alt[maxText] = "";
    char menu[maxText] = "";
    char scope[maxText] = "";
    bool hasAlt = Attribute(step, "alt", alt, sizeof(alt));
    bool hasMenu = Attribute(step, "menu", menu, sizeof(menu));
    bool hasScope = Attribute(step, "scope", scope, sizeof(scope));
    if (hasAlt + hasMenu + hasScope > 1)
    {
        GenDie("a display name of two alternates", step);
    }
    if (hasMenu || hasScope)
    {
        const char* value = hasMenu ? menu : scope;
        if (strcmp(value, "core") == 0 || (hasMenu && strcmp(value, "extension") == 0))
        {
            return strcmp(value, "core") == 0 ? nameFieldCore : nameFieldExtension;
        }
        GenDie("a menu or scope the generator does not know", step);
    }
    for (int f = 1; hasAlt && f < nameFieldCount; f++)
    {
        if (strcmp(alt, s_alts[f]) == 0)
        {
            return (NameField)f;
        }
    }
    if (hasAlt)
    {
        size_t length = strlen(code);
        snprintf(code + length, capacity - length, "%%%s", alt);
    }
    return nameFieldName;
}

// Whether a code is of the bytes and length the codes unit takes.
static bool ValidCode(const char* code)
{
    size_t length = strlen(code);
    for (size_t i = 0; i < length; i++)
    {
        char c = code[i];
        bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (!letter && !(c >= '0' && c <= '9') && c != '-' && c != ':' && c != '%')
        {
            return false;
        }
    }
    return length > 2 && length <= nameMaxId && code[1] == ':';
}

static void Collect(const char* chain, void* value, void* context)
{
    Names* n = context;
    const Leaf* leaf = value;
    if (strncmp(chain, s_prefix, sizeof(s_prefix) - 1) != 0 ||
        leaf->draft > n->input->cldr->threshold || strcmp(leaf->text, s_inheritMarker) == 0)
    {
        return;
    }
    const char* step = strchr(chain + sizeof(s_prefix) - 1, '/');
    if (step == nullptr)
    {
        GenDie("a display name chain the generator does not know", chain);
    }
    step++;
    char element[maxText];
    size_t length = strcspn(step, "[");
    snprintf(element, sizeof(element), "%.*s", (int)length, step);
    char code[maxText];
    if (strchr(element, '/') != nullptr || !CodeOf(n, element, step, code, sizeof(code)))
    {
        if (strcmp(element, "subdivision") != 0)
        {
            GenDie("a display name chain the generator does not know", chain);
        }
        return;
    }
    NameField field = FieldOf(step, code, sizeof(code));
    if (!ValidCode(code))
    {
        GenDie("a display name code too long or of other bytes", code);
    }
    uintptr_t index = (uintptr_t)MapGet(&n->index, code);
    if (index == 0)
    {
        if (n->count == maxCodes)
        {
            GenDie("too many display name codes", code);
        }
        n->codes[n->count].code = Copy(code);
        index = (uintptr_t)++n->count;
        MapPut(&n->index, code, (void*)index);
    }
    Code* c = &n->codes[index - 1];
    if (c->chains[field] == nullptr)
    {
        c->chains[field] = Copy(chain);
    }
    else if (strcmp(c->chains[field], chain) != 0)
    {
        GenDie("two display name chains of one code and field", chain);
    }
}

static int CompareCodes(const void* a, const void* b)
{
    return strcmp(((const Code*)a)->code, ((const Code*)b)->code);
}

static void CollectAll(Names* n)
{
    for (int i = 0; i < n->input->count; i++)
    {
        const LdmlFile* file = CldrFile(n->input->cldr, n->input->ids[i], componentMain);
        if (file != nullptr)
        {
            MapEach(&file->leaves, Collect, n);
        }
    }
    qsort(n->codes, (size_t)n->count, sizeof(Code), CompareCodes);
}

// CLDR's value of a chain at a locale, or null.
static const char* Resolved(Names* n, const char* locale, const char* chain)
{
    const Leaf* leaf = CldrResolve(n->input->cldr, locale, chain, componentMain);
    if (leaf != nullptr && strcmp(leaf->text, s_emptyMarker) == 0)
    {
        GenDie("a display name explicitly empty", chain);
    }
    return leaf != nullptr ? leaf->text : nullptr;
}

// A field's value at a locale: CLDR's resolution of it, else the name's.
static const char* Target(Names* n, const char* locale, int code, int field)
{
    const Code* c = &n->codes[code];
    const char* value =
        c->chains[field] != nullptr ? Resolved(n, locale, c->chains[field]) : nullptr;
    if (value == nullptr && field != nameFieldName && c->chains[nameFieldName] != nullptr)
    {
        value = Resolved(n, locale, c->chains[nameFieldName]);
    }
    return value;
}

// The first value of a field filed along a locale's chain.
static const char* FiledValue(Names* n, const char* locale, int code, int field)
{
    for (const char* at = locale; at != nullptr; at = CldrParent(n->input->cldr, at, componentMain))
    {
        const Filed* filed = MapGet(&n->filed, at);
        const char* const* fields = filed != nullptr ? filed->rows[code] : nullptr;
        if (fields != nullptr && fields[field] != nullptr)
        {
            return fields[field];
        }
    }
    return nullptr;
}

// What the library gives for a field at a locale.
static const char* LookedUp(Names* n, const char* locale, int code, int field)
{
    const char* value = FiledValue(n, locale, code, field);
    return value != nullptr ? value : FiledValue(n, locale, code, nameFieldName);
}

// Whether a value is text the names units hold, a pattern holding its
// placeholders.
static bool ValidValue(const char* code, const char* value)
{
    for (const unsigned char* at = (const unsigned char*)value; *at != '\0';)
    {
        int length = *at < 0x20 ? 0 : GenCharLength(at);
        if (length == 0)
        {
            return false;
        }
        at += length;
    }
    bool zero = strstr(value, "{0}") != nullptr;
    bool one = strstr(value, "{1}") != nullptr;
    return value[0] != '\0' && (code[0] != 'p' || (zero && one)) && (code[0] != 'c' || zero);
}

// Files a value at a locale, or checks a locale that files nothing.
static void Take(Names* n, Filed* filed, const char* locale, int code, int field, const char* value)
{
    const char* looked = LookedUp(n, locale, code, field);
    if ((looked == nullptr) == (value == nullptr) &&
        (value == nullptr || strcmp(looked, value) == 0))
    {
        return;
    }
    if (value == nullptr)
    {
        GenDie("a display name CLDR leaves out that the chain gives", locale);
    }
    if (filed == nullptr)
    {
        GenDie("a locale's display names differ from those of its parent's key", locale);
    }
    if (!ValidValue(n->codes[code].code, value))
    {
        GenDie("a display name empty, of a control or malformed byte, or a pattern without its "
               "placeholders",
               value);
    }
    if (filed->rows[code] == nullptr)
    {
        filed->rows[code] = GenAllocate(sizeof(char*) * nameFieldCount);
    }
    filed->rows[code][field] = value;
}

// Files a locale's names (filed null: checks them); the name first, which
// the other fields fall back to.
static void TakeLocale(Names* n, Filed* filed, const char* locale)
{
    for (int code = 0; code < n->count; code++)
    {
        for (int field = 0; field < nameFieldCount; field++)
        {
            if (n->codes[code].chains[field] != nullptr)
            {
                Take(n, filed, locale, code, field, Target(n, locale, code, field));
            }
        }
    }
}

// Adds a locale key's names of a kind.
static void AddUnit(const Names* n, const Filed* filed, uint32_t kind, const char* key,
                    DataFile* file)
{
    static uint16_t s_keys[maxCodes * nameFieldCount];
    static const char* s_texts[maxCodes * nameFieldCount];
    int count = 0;
    size_t text = 0;
    for (int code = 0; code < n->count; code++)
    {
        const char* const* fields = filed->rows[code];
        const char* id = n->codes[code].code;
        for (int f = 0;
             fields != nullptr && mlocNameKind(id, strlen(id)) == kind && f < nameFieldCount; f++)
        {
            if (fields[f] != nullptr)
            {
                s_keys[count] = (uint16_t)(code * nameFieldCount + f);
                s_texts[count++] = fields[f];
                text += strlen(fields[f]);
            }
        }
    }
    if (count == 0)
    {
        return;
    }
    if (text > 0xFFFF)
    {
        GenDie("a names unit too large", key);
    }
    size_t size = nameUnitHeaderSize + 2 * (size_t)count + 2 * ((size_t)count + 1) + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)count, 2);
    GenPut(unit + 2, text, 2);
    uint8_t* offsets = unit + nameUnitHeaderSize + 2 * (size_t)count;
    uint8_t* textAt = offsets + 2 * ((size_t)count + 1);
    size_t at = 0;
    for (int i = 0; i < count; i++)
    {
        size_t length = strlen(s_texts[i]);
        GenPut(unit + nameUnitHeaderSize + 2 * (size_t)i, s_keys[i], 2);
        GenPut(offsets + 2 * (size_t)i, at, 2);
        memcpy(textAt + at, s_texts[i], length);
        at += length;
    }
    GenPut(offsets + 2 * (size_t)count, at, 2);
    DataFileAdd(file, kind, key, unit, size);
    free(unit);
}

// Takes a locale after its parent.
static void TakeChain(Names* n, const char* locale, DataFile* file)
{
    if (MapGet(&n->filed, locale) != nullptr)
    {
        return;
    }
    const NamesInput* input = n->input;
    const char* parent = CldrParent(input->cldr, locale, componentMain);
    if (parent != nullptr)
    {
        TakeChain(n, parent, file);
    }
    char key[maxKey];
    char parentKey[maxKey] = "";
    KeyOf(input->keys, locale, key, sizeof(key));
    if (parent != nullptr)
    {
        KeyOf(input->keys, parent, parentKey, sizeof(parentKey));
    }
    Filed* filed = GenAllocate(sizeof(Filed));
    bool own = strcmp(key, parentKey) != 0;
    // Filed first, so that the locale's own fields count in its lookups.
    MapPut(&n->filed, locale, filed);
    TakeLocale(n, own ? filed : nullptr, locale);
    for (size_t k = 0; own && k < sizeof(s_kinds) / sizeof(s_kinds[0]); k++)
    {
        AddUnit(n, filed, s_kinds[k], key, file);
    }
}

// The codes' unit: a header, the text offsets, the text.
static void AddCodes(const Names* n, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < n->count; i++)
    {
        text += strlen(n->codes[i].code);
    }
    size_t offsets = 4 * ((size_t)n->count + 1);
    size_t size = nameIdsHeaderSize + offsets + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)n->count, 2);
    GenPut(unit + 4, text, 4);
    uint8_t* textAt = unit + nameIdsHeaderSize + offsets;
    size_t at = 0;
    for (int i = 0; i < n->count; i++)
    {
        size_t length = strlen(n->codes[i].code);
        GenPut(unit + nameIdsHeaderSize + 4 * (size_t)i, at, 4);
        memcpy(textAt + at, n->codes[i].code, length);
        at += length;
    }
    GenPut(unit + nameIdsHeaderSize + 4 * (size_t)n->count, at, 4);
    DataFileAdd(file, kindNameIds, nullptr, unit, size);
    free(unit);
}

int GenerateNames(const NamesInput* input, DataFile* file, char* errorOut, size_t errorCapacity)
{
    static Names s_names;
    Names* n = &s_names;
    *n = (Names){.input = input};
    if (!ReadAllKeys(n, errorOut, errorCapacity))
    {
        return 0;
    }
    CollectAll(n);
    AddCodes(n, file);
    for (int i = 0; i < input->count; i++)
    {
        TakeChain(n, input->ids[i], file);
    }
    return 1;
}
