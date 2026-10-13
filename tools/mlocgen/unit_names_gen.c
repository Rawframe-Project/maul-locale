// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names (unit_names_gen.h), each value NamesTarget's
// (unit_names_model.h). A key files it where the library's lookup along
// its chain (the first key filing the field, a plural category no key
// files taking the other category's) would give another. Locales are
// taken parent first, so a key's chain is filed before it; a locale under
// its parent's key files nothing and is checked to resolve to what the
// key gives. Only the plural categories of the locale's rules and other
// are filed, the ones the library asks for. The library looks up a power
// of a gender's field, then the plain power's.

#include "unit_names_gen.h"

#include "kinds.h"
#include "unit_derivations_gen.h"
#include "unit_names_model.h"

#include "maul-unicode/case.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    // A locale's rows: each width's names, then the extras.
    extrasRow = widthCount,
    rowCount = widthCount + 1,
    // A pattern's bytes once lowercased.
    maxLowered = 4 * maxTableString,
};

static const uint32_t s_kinds[rowCount] = {kindUnitNamesLong, kindUnitNamesShort,
                                           kindUnitNamesNarrow, kindUnitNameExtras};
static const char s_idBytes[] = "abcdefghijklmnopqrstuvwxyz0123456789-:";

// The fields in the order a locale files them: other before the plural
// categories that fall back to it.
static const int s_fieldOrder[unitNameFields] = {
    unitNameDisplay, unitNameOther, unitNamePer, 1, 2, 3, 4, 5,
};

// The fields a locale files, per row and identifier; null where none.
typedef struct Filed
{
    const char** rows[rowCount][maxIds];
} Filed;

// Whether a field is one the locale files.
static bool Wanted(unsigned categories, int field)
{
    return field < unitNameZero || field > unitNameOther ||
           (categories & 1u << (field - unitNameZero)) != 0;
}

static bool Differs(const char* a, const char* b)
{
    return (a == nullptr) != (b == nullptr) || (a != nullptr && strcmp(a, b) != 0);
}

// The first value filed along a locale's chain.
static const char* FiledValue(const Names* n, const char* locale, int row, int id, int field)
{
    for (const char* at = locale; at != nullptr; at = CldrParent(n->input->cldr, at, componentMain))
    {
        const Filed* filed = MapGet(&n->filed, at);
        const char* const* fields = filed != nullptr ? filed->rows[row][id] : nullptr;
        if (fields != nullptr && fields[field] != nullptr)
        {
            return fields[field];
        }
    }
    return nullptr;
}

// What the library gives for a field at a locale.
static const char* LookedUp(const Names* n, const char* locale, int row, int id, int field)
{
    const char* value = FiledValue(n, locale, row, id, field);
    const UnitName* name = &n->names[id];
    if (name->gender != nullptr && row != extrasRow)
    {
        return value != nullptr ? value : LookedUp(n, locale, row, name->plain, field);
    }
    bool category = field >= unitNameZero && field < unitNameOther;
    // A name's empty category is other's (present for compound names).
    if (category && (value == nullptr || (row != extrasRow && value[0] == '\0')))
    {
        value = FiledValue(n, locale, row, id, unitNameOther);
    }
    return value;
}

// Whether a unit's plural category ICU's data has (Given) is filed by no key of the chain: the
// library then takes other's for it, where a compound name needs to know that ICU has a pattern of
// its own (it takes a unit's placeholder from the first category that has one).
static bool Unfiled(const Names* n, const char* locale, int row, int id, int field)
{
    const UnitName* name = &n->names[id];
    return row != extrasRow && name->kind == unitIdUnit && field >= unitNameZero &&
           field < unitNameOther && NamesGiven(n, locale, name, row, field) &&
           FiledValue(n, locale, row, id, field) == nullptr;
}

// Files a value at a locale, or checks a locale that files nothing.
static void Take(Names* n, Filed* filed, const char* locale, int row, int id, int field,
                 const char* value)
{
    bool same = !Differs(LookedUp(n, locale, row, id, field), value);
    if (same && !Unfiled(n, locale, row, id, field))
    {
        return;
    }
    // A category the library would take from other's anyway is filed
    // empty, which costs no text: it only says ICU has it.
    value = same && row != extrasRow && value != nullptr ? "" : value;
    if (value == nullptr)
    {
        GenDie("a unit name CLDR leaves out that the chain gives", locale);
    }
    if (filed == nullptr)
    {
        GenDie("a locale's unit names differ from those of its parent's key", locale);
    }
    // A unit's pattern may leave the number out (Arabic's dual of a
    // revolution), and ICU then writes none; a pattern that builds a name
    // may not.
    if (row != extrasRow && (field == unitNamePer || n->names[id].kind != unitIdUnit) &&
        value[0] != '\0' && strstr(value, "{0}") == nullptr)
    {
        GenDie("a unit pattern without {0}", value);
    }
    const char** fields = filed->rows[row][id];
    if (fields == nullptr)
    {
        fields = GenAllocate(sizeof(char*) * unitNameFields);
        filed->rows[row][id] = fields;
    }
    fields[field] = value;
}

// The pattern lowercased by the locale's language's rules, or "" when
// that changes nothing.
static const char* Lowered(const char* locale, const char* pattern)
{
    muniCaseLanguage language = muni_caseDefault;
    if (strncmp(locale, "tr", 2) == 0 || strncmp(locale, "az", 2) == 0)
    {
        language = (locale[2] == '\0' || locale[2] == '_') ? muni_caseTurkic : language;
    }
    else if (strncmp(locale, "lt", 2) == 0 && (locale[2] == '\0' || locale[2] == '_'))
    {
        language = muni_caseLithuanian;
    }
    char lowered[maxLowered];
    size_t needed = 0;
    muniTextResult r = muniConvertCase(pattern, strlen(pattern), muni_caseLower, language,
                                       muni_convertStrict, lowered, sizeof(lowered), &needed);
    if (r.status != muni_success)
    {
        GenDie("a unit pattern that does not lowercase", pattern);
    }
    if (needed == strlen(pattern) && memcmp(lowered, pattern, needed) == 0)
    {
        return "";
    }
    return NamesCopy(lowered, needed);
}

// A locale's extras: the long width's genders, and the lowercased
// patterns of the units and prefixes.
static void TakeExtras(Names* n, Filed* filed, const char* locale, unsigned categories)
{
    for (int id = 0; id < n->count; id++)
    {
        const UnitName* name = &n->names[id];
        if (name->kind != unitIdUnit && name->kind != unitIdPrefix)
        {
            continue;
        }
        if (name->kind == unitIdUnit)
        {
            char chain[maxChain];
            (void)NamesChainOf(name, name->source, nullptr, widthLong, fieldGender, chain,
                               sizeof(chain));
            const char* gender = NamesResolved(n, locale, chain);
            if (gender != nullptr || LookedUp(n, locale, extrasRow, id, unitExtraGender) != nullptr)
            {
                Take(n, filed, locale, extrasRow, id, unitExtraGender, gender);
            }
        }
        // The patterns in filing order, other first.
        for (int f = 1; f < unitNameFields; f++)
        {
            int field = s_fieldOrder[f];
            bool applies =
                field != unitNamePer &&
                (name->kind == unitIdUnit ? Wanted(categories, field) : field == unitNameOther);
            const char* pattern = applies ? LookedUp(n, locale, widthLong, id, field) : nullptr;
            if (pattern != nullptr)
            {
                Take(n, filed, locale, extrasRow, id, field, Lowered(locale, pattern));
            }
        }
    }
}

// Files a locale's names (filed null: checks them).
static void TakeLocale(Names* n, Filed* filed, const char* locale)
{
    unsigned categories = NamesCategoriesOf(n, locale);
    char chain[maxChain];
    for (int width = 0; width < widthCount; width++)
    {
        for (int id = 0; id < n->count; id++)
        {
            for (int f = 0; f < unitNameFields; f++)
            {
                int field = s_fieldOrder[f];
                const UnitName* name = &n->names[id];
                if (Wanted(categories, field) && NamesChainOf(name, name->type, name->gender, width,
                                                              field, chain, sizeof(chain)))
                {
                    Take(n, filed, locale, width, id, field,
                         NamesTarget(n, locale, id, width, field));
                }
            }
        }
    }
    TakeExtras(n, filed, locale, categories);
}

// A string's index in the unit's table, checked as a sparse unit's.
static uint16_t FieldString(Table* t, const char* text, const char* key)
{
    for (const unsigned char* at = (const unsigned char*)text; *at != '\0';)
    {
        int length = *at < 0x20 ? 0 : GenCharLength(at);
        if (length == 0)
        {
            GenDie("a unit name with a control or malformed byte", key);
        }
        at += length;
    }
    return TableAddString(t, text, strlen(text));
}

static void AddRow(Names* n, const Filed* filed, int row, const char* key, DataFile* file)
{
    static uint16_t s_codes[maxIds];
    static uint8_t s_rows[maxIds][1 + 2 * unitNameFields];
    static uint8_t* s_rowPointers[maxIds];
    static size_t s_rowSizes[maxIds];
    int count = 0;
    for (int id = 0; id < n->count; id++)
    {
        const char* const* fields = filed->rows[row][id];
        if (fields == nullptr)
        {
            continue;
        }
        uint8_t* bytes = s_rows[count];
        size_t size = 1;
        bytes[0] = 0;
        for (int f = 0; f < unitNameFields; f++)
        {
            if (fields[f] != nullptr)
            {
                bytes[0] |= (uint8_t)(1u << f);
                GenPut(bytes + size, FieldString(&n->table, fields[f], key), 2);
                size += 2;
            }
        }
        s_codes[count] = (uint16_t)id;
        s_rowPointers[count] = bytes;
        s_rowSizes[count] = size;
        count++;
    }
    if (count > 0)
    {
        TableAddSparseUnit(&n->table, s_codes, s_rowPointers, s_rowSizes, count, nullptr,
                           s_kinds[row], key, file);
    }
    TableClear(&n->table);
}

// Takes a locale after its parent.
static void TakeChain(Names* n, const char* locale, DataFile* file)
{
    if (MapGet(&n->filed, locale) != nullptr)
    {
        return;
    }
    const UnitNamesInput* input = n->input;
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
    for (int row = 0; own && row < rowCount; row++)
    {
        AddRow(n, filed, row, key, file);
    }
}

// The identifiers' unit: a header, the text offsets, the kinds, the
// text.
static void AddIds(const Names* n, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < n->count; i++)
    {
        size_t length = strlen(n->names[i].id);
        if (length > unitMaxNameId || strspn(n->names[i].id, s_idBytes) != length)
        {
            GenDie("a unit identifier too long or of other bytes", n->names[i].id);
        }
        text += length;
    }
    size_t offsets = 4 * ((size_t)n->count + 1);
    size_t size = unitIdsHeaderSize + offsets + (size_t)n->count + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)n->count, 2);
    GenPut(unit + 4, text, 4);
    uint8_t* kinds = unit + unitIdsHeaderSize + offsets;
    uint8_t* textAt = kinds + n->count;
    size_t at = 0;
    for (int i = 0; i < n->count; i++)
    {
        size_t length = strlen(n->names[i].id);
        GenPut(unit + unitIdsHeaderSize + 4 * (size_t)i, at, 4);
        kinds[i] = n->names[i].kind;
        memcpy(textAt + at, n->names[i].id, length);
        at += length;
    }
    GenPut(unit + unitIdsHeaderSize + 4 * (size_t)n->count, at, 4);
    DataFileAdd(file, kindUnitIds, nullptr, unit, size);
    free(unit);
}

int GenerateUnitNames(const UnitNamesInput* input, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    static Names s_names;
    Names* n = &s_names;
    *n = (Names){.input = input};
    if (!NamesReadCategories(n, errorOut, errorCapacity))
    {
        return 0;
    }
    NamesCollect(n);
    AddIds(n, file);
    if (!GenerateUnitDerivations(input->commonDirectory, file, errorOut, errorCapacity))
    {
        return 0;
    }
    for (int i = 0; i < input->count; i++)
    {
        TakeChain(n, input->ids[i], file);
    }
    return 1;
}
