// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names (unit_names_gen.h). Each value is the one ICU's data
// gives (Target): CLDR's production data, built from the development
// data as GenerateProductionData builds it, read along the chain as ICU
// reads it. A key files it where the library's lookup along
// its chain (the first key filing the field, a plural category no key
// files taking the other category's) would give another. Locales are
// taken parent first, so a key's chain is filed before it; a locale under
// its parent's key files nothing and is checked to resolve to what the
// key gives. Only the plural categories of the locale's rules and other
// are filed, the ones the library asks for. A power of a gender is ICU's
// (InflectedPluralSink): per plural category, the first locale along the
// chain with the gender's pattern, neuter's or the plain one's; the
// library looks up the gender's field, then the plain power's.

#include "unit_names_gen.h"

#include "kinds.h"
#include "table_gen.h"
#include "unit_tables.h"
#include "xml.h"

#include "maul-unicode/case.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxIds = 512,
    maxChain = 512,
    widthLong = 0,
    widthShort = 1,
    widthCount = 3,
    // A unit's gender, looked up as a field.
    fieldGender = unitNameFields,
    // A locale's rows: each width's names, then the extras.
    extrasRow = widthCount,
    rowCount = widthCount + 1,
    categoryCount = 6,
    // A pattern's bytes once lowercased.
    maxLowered = 4 * maxTableString,
};

static const char* const s_widths[widthCount] = {"long", "short", "narrow"};
static const uint32_t s_kinds[rowCount] = {kindUnitNamesLong, kindUnitNamesShort,
                                           kindUnitNamesNarrow, kindUnitNameExtras};
static const char* const s_categories[categoryCount] = {"zero", "one",  "two",
                                                        "few",  "many", "other"};
static const char s_prefix[] = "//ldml/units/unitLength[@type=\"";
static const char s_empty[] = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";
static const char s_idBytes[] = "abcdefghijklmnopqrstuvwxyz0123456789-:";
static const char s_inherit[] = "\xE2\x86\x91\xE2\x86\x91\xE2\x86\x91";

// The fields in the order a locale files them: other before the plural
// categories that fall back to it.
static const int s_fieldOrder[unitNameFields] = {
    unitNameDisplay, unitNameOther, unitNamePer, 1, 2, 3, 4, 5,
};

typedef struct UnitName
{
    // The identifier the library knows ("meter", "power2:feminine").
    char* id;
    // CLDR's type ("length-meter", "power2").
    char* type;
    // The type whose data it takes: ICU's "-person" durations take the
    // plain ones' (number_longnames.cpp, ICU-20400).
    char* source;
    // A power's gender, or null.
    char* gender;
    // A power of a gender: the plain power's index.
    int plain;
    // Its unitId kind.
    uint8_t kind;
} UnitName;

// The fields a locale files, per row and identifier; null where none.
typedef struct Filed
{
    const char** rows[rowCount][maxIds];
} Filed;

typedef struct Names
{
    const UnitNamesInput* input;
    UnitName names[maxIds];
    int count;
    // Identifier to its UnitName while collecting, then to its index + 1.
    Map byId;
    // Locale id to the plural categories of its rules (a bit per
    // category, plus 1 so none is null).
    Map categories;
    // Locale id to its Filed, once taken.
    Map filed;
    // Root's unit aliases: a type to the type whose data it takes.
    Map aliases;
    Table table;
} Names;

static char* Copy(const char* text, size_t length)
{
    char* copy = GenAllocate(length + 1);
    memcpy(copy, text, length);
    return copy;
}

// The kind of a compound type.
static uint8_t CompoundKind(const char* type)
{
    if (strcmp(type, "per") == 0 || strcmp(type, "times") == 0)
    {
        return unitIdCompound;
    }
    if (strcmp(type, "power2") == 0 || strcmp(type, "power3") == 0)
    {
        return unitIdPower;
    }
    if (strncmp(type, "10p", 3) == 0 || strncmp(type, "1024p", 5) == 0)
    {
        return unitIdPrefix;
    }
    GenDie("a compound unit the generator does not know", type);
}

static void AddName(Names* n, const char* type, const char* gender, uint8_t kind)
{
    char id[maxChain];
    const char* core = type;
    if (kind == unitIdUnit)
    {
        const char* dash = strchr(type, '-');
        if (dash == nullptr)
        {
            GenDie("a unit type without its category", type);
        }
        core = dash + 1;
    }
    snprintf(id, sizeof(id), "%s%s%s", core, gender != nullptr ? ":" : "",
             gender != nullptr ? gender : "");
    UnitName* found = MapGet(&n->byId, id);
    if (found != nullptr)
    {
        if (strcmp(found->type, type) != 0)
        {
            GenDie("two unit types of one identifier", id);
        }
        return;
    }
    if (n->count == maxIds)
    {
        GenDie("too many unit identifiers", id);
    }
    size_t length = strlen(type);
    size_t person = sizeof("-person") - 1;
    if (kind == unitIdUnit && length > person && strcmp(type + length - person, "-person") == 0)
    {
        length -= person;
    }
    UnitName* name = &n->names[n->count++];
    *name = (UnitName){
        .id = Copy(id, strlen(id)),
        .type = Copy(type, strlen(type)),
        .source = Copy(type, length),
        .gender = gender != nullptr ? Copy(gender, strlen(gender)) : nullptr,
        .kind = kind,
    };
    MapPut(&n->byId, id, name);
}

// The value of an attribute in a chain's element ("gender" in
// compoundUnitPattern1[@count="one"][@gender="feminine"]), into out.
static bool AttributeOf(const char* element, const char* name, char* out, size_t capacity)
{
    char marker[64];
    snprintf(marker, sizeof(marker), "[@%s=\"", name);
    const char* at = strstr(element, marker);
    if (at == nullptr)
    {
        return false;
    }
    at += strlen(marker);
    size_t length = strcspn(at, "\"");
    if (length >= capacity)
    {
        GenDie("an attribute too long", element);
    }
    memcpy(out, at, length);
    out[length] = '\0';
    return true;
}

// Takes one chain of a locale's file: the unit or compound it names.
// Cases come with the grammar's data, and the coordinate and duration
// units with the list patterns'.
static void CollectChain(const char* chain, void* value, void* context)
{
    (void)value;
    Names* n = context;
    size_t prefix = sizeof(s_prefix) - 1;
    if (strncmp(chain, s_prefix, prefix) != 0)
    {
        return;
    }
    const char* at = strstr(chain + prefix, "\"]/");
    if (at == nullptr)
    {
        return;
    }
    at += 3;
    bool compound = strncmp(at, "compoundUnit[@type=\"", 20) == 0;
    if (!compound && strncmp(at, "unit[@type=\"", 12) != 0)
    {
        return;
    }
    at = strchr(at, '"') + 1;
    size_t length = strcspn(at, "\"");
    char type[maxChain];
    if (strcmp(at + length, "\"]") == 0)
    {
        // An element with no children, which names nothing.
        return;
    }
    if (length >= sizeof(type) || strncmp(at + length, "\"]/", 3) != 0)
    {
        GenDie("a unit chain the generator cannot read", chain);
    }
    memcpy(type, at, length);
    type[length] = '\0';
    const char* element = at + length + 3;
    if (strstr(element, "[@case=") != nullptr)
    {
        return;
    }
    uint8_t kind = compound ? CompoundKind(type) : unitIdUnit;
    char gender[maxKey];
    bool gendered = kind == unitIdPower && AttributeOf(element, "gender", gender, sizeof(gender));
    AddName(n, type, gendered ? gender : nullptr, kind);
}

// Takes one of root's aliases: a unit whose data is another's.
static void CollectAlias(const char* chain, void* value, void* context)
{
    Names* n = context;
    const char* path = value;
    size_t prefix = sizeof(s_prefix) - 1;
    const char* at =
        strncmp(chain, s_prefix, prefix) == 0 ? strstr(chain + prefix, "\"]/") : nullptr;
    if (at == nullptr || strncmp(at + 3, "unit[@type=\"", 12) != 0)
    {
        return;
    }
    char type[maxChain];
    char target[maxChain];
    at += 3 + 12;
    size_t length = strcspn(at, "\"");
    size_t targetLength = strncmp(path, "../unit[@type='", 15) == 0 ? strcspn(path + 15, "'") : 0;
    if (length >= sizeof(type) || strcmp(at + length, "\"]") != 0 || targetLength == 0 ||
        targetLength >= sizeof(target) || strcmp(path + 15 + targetLength, "']") != 0)
    {
        GenDie("a unit alias the generator cannot read", chain);
    }
    memcpy(type, at, length);
    type[length] = '\0';
    memcpy(target, path + 15, targetLength);
    target[targetLength] = '\0';
    AddName(n, type, nullptr, unitIdUnit);
    MapPut(&n->aliases, type, Copy(target, targetLength));
}

static int CompareNames(const void* a, const void* b)
{
    return strcmp(((const UnitName*)a)->id, ((const UnitName*)b)->id);
}

static void CollectNames(Names* n)
{
    for (int i = 0; i < n->input->count; i++)
    {
        const LdmlFile* own = CldrFile(n->input->cldr, n->input->ids[i], componentMain);
        if (own != nullptr)
        {
            MapEach(&own->leaves, CollectChain, n);
        }
    }
    const LdmlFile* root = CldrFile(n->input->cldr, "root", componentMain);
    if (root != nullptr)
    {
        MapEach(&root->aliases, CollectAlias, n);
    }
    qsort(n->names, (size_t)n->count, sizeof(UnitName), CompareNames);
    // The sort moved the names: the map now gives each one's index + 1.
    for (int i = 0; i < n->count; i++)
    {
        MapPut(&n->byId, n->names[i].id, (void*)(uintptr_t)(i + 1));
    }
    for (int i = 0; i < n->count; i++)
    {
        UnitName* name = &n->names[i];
        if (name->gender != nullptr)
        {
            name->plain = (int)(uintptr_t)MapGet(&n->byId, name->type) - 1;
            if (name->plain < 0)
            {
                GenDie("a power of a gender without its plain power", name->id);
            }
        }
    }
}

// Reads the cardinal rules' categories per locale from plurals.xml.
static int ReadCategories(Names* n, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/plurals.xml", n->input->commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* plurals = XmlFindChild(root, "plurals");
    for (int i = 0; plurals != nullptr && i < plurals->childCount; i++)
    {
        const XmlNode* rules = plurals->children[i];
        const char* locales = XmlGetAttribute(rules, "locales");
        if (strcmp(rules->name, "pluralRules") != 0 || locales == nullptr)
        {
            continue;
        }
        uintptr_t mask = 1u << (categoryCount - 1);
        for (int r = 0; r < rules->childCount; r++)
        {
            const char* count = XmlGetAttribute(rules->children[r], "count");
            for (int c = 0; count != nullptr && c < categoryCount; c++)
            {
                mask |= strcmp(count, s_categories[c]) == 0 ? 1u << c : 0;
            }
        }
        char id[maxKey];
        for (const char* at = locales; *at != '\0';)
        {
            size_t length = strcspn(at, " ");
            if (length > 0 && length < sizeof(id))
            {
                memcpy(id, at, length);
                id[length] = '\0';
                MapPut(&n->categories, id, (void*)(mask + 1));
            }
            at += length + strspn(at + length, " ");
        }
    }
    XmlFree(root);
    if (MapGet(&n->categories, "root") == nullptr)
    {
        snprintf(errorOut, errorCapacity, "plurals.xml gives root no rules");
        return 0;
    }
    return 1;
}

// A locale's categories, its rules found by truncation as the library's.
static unsigned RuleCategories(const Names* n, const char* locale)
{
    char id[maxKey];
    snprintf(id, sizeof(id), "%s", locale);
    for (;;)
    {
        void* found = MapGet(&n->categories, id);
        if (found != nullptr)
        {
            return (unsigned)((uintptr_t)found - 1);
        }
        char* cut = strrchr(id, '_');
        if (cut == nullptr)
        {
            return (unsigned)((uintptr_t)MapGet(&n->categories, "root") - 1);
        }
        *cut = '\0';
    }
}

// A locale's categories and one, which compound names read whatever the
// rules.
static unsigned CategoriesOf(const Names* n, const char* locale)
{
    return RuleCategories(n, locale) | 1u << 1;
}

// Whether a field is one the locale files.
static bool Wanted(unsigned categories, int field)
{
    return field < unitNameZero || field > unitNameOther ||
           (categories & 1u << (field - unitNameZero)) != 0;
}

// The chain of a name's field in a width, for a type and a gender, or
// false where the field does not apply: a compound key has its pattern in
// the other field, a power one per plural category, a unit its gender.
static bool ChainOf(const UnitName* name, const char* type, const char* gender, int width,
                    int field, char* out, size_t capacity)
{
    int at = snprintf(out, capacity, "%s%s\"]/%s[@type=\"%s\"]/", s_prefix, s_widths[width],
                      name->kind == unitIdUnit ? "unit" : "compoundUnit", type);
    const char* element = nullptr;
    bool count = field >= unitNameZero && field <= unitNameOther;
    switch (name->kind)
    {
    case unitIdUnit:
        element = field == unitNameDisplay ? "displayName"
                  : field == unitNamePer   ? "perUnitPattern"
                  : field == fieldGender   ? "gender"
                                           : "unitPattern";
        break;
    case unitIdPrefix:
        element = field == unitNameOther ? "unitPrefixPattern" : nullptr;
        count = false;
        break;
    case unitIdCompound:
        element = field == unitNameOther ? "compoundUnitPattern" : nullptr;
        count = false;
        break;
    case unitIdPower:
        element = count ? "compoundUnitPattern1" : nullptr;
        break;
    }
    if (element == nullptr)
    {
        return false;
    }
    at += snprintf(out + at, capacity - (size_t)at, "%s", element);
    if (count)
    {
        at += snprintf(out + at, capacity - (size_t)at, "[@count=\"%s\"]",
                       s_categories[field - unitNameZero]);
    }
    if (gender != nullptr)
    {
        snprintf(out + at, capacity - (size_t)at, "[@gender=\"%s\"]", gender);
    }
    return true;
}

// CLDR's resolution of a chain (CldrResolve: the locale's chain, then
// root's aliases of widths and units looked up again from the locale).
static const char* Resolved(const Names* n, const char* locale, const char* chain)
{
    const Leaf* leaf = CldrResolve(n->input->cldr, locale, chain, componentMain);
    return leaf == nullptr || strcmp(leaf->text, s_empty) == 0 ? nullptr : leaf->text;
}

// The value a locale's own file gives a chain, or null: a marker, an
// explicit empty value or a draft below the threshold gives none.
static const char* OwnValue(const Names* n, const char* locale, const char* chain)
{
    Cldr* cldr = n->input->cldr;
    const LdmlFile* file = CldrFile(cldr, locale, componentMain);
    const Leaf* leaf = file == nullptr ? nullptr : MapGet(&file->leaves, chain);
    if (leaf == nullptr || leaf->draft > cldr->threshold || strcmp(leaf->text, s_inherit) == 0 ||
        strcmp(leaf->text, s_empty) == 0)
    {
        return nullptr;
    }
    return leaf->text;
}

static bool Differs(const char* a, const char* b)
{
    return (a == nullptr) != (b == nullptr) || (a != nullptr && strcmp(a, b) != 0);
}

// A locale's own leaf of a chain, or null for none or a draft below the
// threshold.
static const Leaf* OwnLeaf(const Names* n, const char* locale, const char* chain)
{
    Cldr* cldr = n->input->cldr;
    const LdmlFile* file = CldrFile(cldr, locale, componentMain);
    const Leaf* leaf = file == nullptr ? nullptr : MapGet(&file->leaves, chain);
    return leaf == nullptr || leaf->draft > cldr->threshold ? nullptr : leaf;
}

// Whether CLDR's production data keeps a locale's leaf as it is
// (GenerateProductionData): a value, unless a locale above it below root
// gives the same at the same chain.
static bool Kept(const Names* n, const char* locale, const char* chain, const Leaf* leaf)
{
    if (strcmp(leaf->text, s_inherit) == 0 || strcmp(leaf->text, s_empty) == 0)
    {
        return false;
    }
    Cldr* cldr = n->input->cldr;
    for (const char* at = CldrParent(cldr, locale, componentMain);
         at != nullptr && strcmp(at, "root") != 0; at = CldrParent(cldr, at, componentMain))
    {
        const Leaf* above = OwnLeaf(n, at, chain);
        if (above != nullptr && strcmp(above->text, s_inherit) != 0)
        {
            return strcmp(above->text, leaf->text) != 0;
        }
    }
    return true;
}

// The grammatical cases CLDR's unit patterns take.
static const char* const s_cases[] = {
    "abessive",      "ablative",           "accusative",  "adessive",    "allative",
    "causal",        "comitative",         "dative",      "elative",     "ergative",
    "essive",        "genitive",           "illative",    "inessive",    "instrumental",
    "locative",      "locativecopulative", "nominative",  "oblique",     "partitive",
    "prepositional", "sociative",          "terminative", "translative", "vocative"};

// Whether a locale keeps a long unit pattern of a case, in a plural
// category of its rules: the logical group of a long unit pattern holds
// its cases as well.
static bool CaseKept(const Names* n, const char* locale, const char* type, unsigned rules)
{
    char chain[maxChain];
    for (size_t k = 0; k < sizeof(s_cases) / sizeof(s_cases[0]); k++)
    {
        for (int c = 0; c < categoryCount; c++)
        {
            snprintf(chain, sizeof(chain),
                     "%s%s\"]/unit[@type=\"%s\"]/unitPattern[@case=\"%s\"][@count=\"%s\"]",
                     s_prefix, s_widths[widthLong], type, s_cases[k], s_categories[c]);
            const Leaf* leaf = (rules & 1u << c) != 0 ? OwnLeaf(n, locale, chain) : nullptr;
            if (leaf != nullptr && Kept(n, locale, chain, leaf))
            {
                return true;
            }
        }
    }
    return false;
}

// Whether a locale's file in CLDR's production data has a field of a type
// in a width: a leaf kept, or a leaf it dropped (an inheritance marker, a
// value its parents give) that it restores as a plural category of its
// rules beside a category kept, or a case of a long pattern kept (the
// logical group, restored only where the file has a leaf).
static bool ProducedAt(const Names* n, const char* locale, const UnitName* name, const char* type,
                       int width, int field)
{
    char chain[maxChain];
    if (!ChainOf(name, type, nullptr, width, field, chain, sizeof(chain)))
    {
        return false;
    }
    const Leaf* leaf = OwnLeaf(n, locale, chain);
    if (leaf == nullptr)
    {
        return false;
    }
    if (Kept(n, locale, chain, leaf))
    {
        return true;
    }
    unsigned rules = RuleCategories(n, locale);
    bool category = field >= unitNameZero && field <= unitNameOther;
    if (!category || (rules & 1u << (field - unitNameZero)) == 0)
    {
        return false;
    }
    for (int c = 0; c < categoryCount; c++)
    {
        if (c != field - unitNameZero && (rules & 1u << c) != 0 &&
            ChainOf(name, type, nullptr, width, unitNameZero + c, chain, sizeof(chain)))
        {
            const Leaf* other = OwnLeaf(n, locale, chain);
            if (other != nullptr && Kept(n, locale, chain, other))
            {
                return true;
            }
        }
    }
    return width == widthLong && name->kind == unitIdUnit && CaseKept(n, locale, type, rules);
}

// The value a locale's file in CLDR's production data gives a field it
// has: the leaf kept, or the one restored, CLDR's resolution at that
// locale (frozen there: "{0} Jigger" for Swiss German's long jigger from
// German's file, where the resolution from Swiss German would give
// "{0} Schuss").
static const char* ProducedValue(const Names* n, const char* locale, const UnitName* name,
                                 const char* type, int width, int field)
{
    char chain[maxChain];
    (void)ChainOf(name, type, nullptr, width, field, chain, sizeof(chain));
    const Leaf* leaf = OwnLeaf(n, locale, chain);
    if (Kept(n, locale, chain, leaf))
    {
        return leaf->text;
    }
    const char* value = Resolved(n, locale, chain);
    if (value == nullptr &&
        ChainOf(name, type, nullptr, width, unitNameOther, chain, sizeof(chain)))
    {
        value = Resolved(n, locale, chain);
    }
    return value;
}

// Whether ICU's data has a field's own value, and the value
// (PluralTableSink): the first locale along the chain with the field in
// the width, or in short (root's alias of the other widths), or in short
// for the unit root aliases.
static bool Located(const Names* n, const char* locale, const UnitName* name, int width, int field,
                    const char** valueOut)
{
    Cldr* cldr = n->input->cldr;
    const char* types[3] = {name->source, name->source, MapGet(&n->aliases, name->source)};
    int widths[3] = {width, widthShort, widthShort};
    for (int i = 0; i < 3; i++)
    {
        for (const char* at = locale; types[i] != nullptr && at != nullptr;
             at = CldrParent(cldr, at, componentMain))
        {
            if (ProducedAt(n, at, name, types[i], widths[i], field))
            {
                *valueOut = ProducedValue(n, at, name, types[i], widths[i], field);
                return true;
            }
        }
    }
    return false;
}

// Whether ICU's data has a plural category's own pattern.
static bool Given(const Names* n, const char* locale, const UnitName* name, int width, int field)
{
    const char* value;
    return Located(n, locale, name, width, field, &value);
}

// A power of a gender's field as ICU loads it (InflectedPluralSink): at
// the first locale along the chain with the gender's, neuter's or the
// plain one's; root's long and narrow widths are aliases of the short
// one.
static const char* PowerField(const Names* n, const char* locale, int id, int width, int field)
{
    const UnitName* name = &n->names[id];
    const char* genders[3] = {name->gender, "neuter", nullptr};
    char chain[maxChain];
    for (const char* at = locale; at != nullptr; at = CldrParent(n->input->cldr, at, componentMain))
    {
        for (int g = strcmp(name->gender, "neuter") == 0 ? 1 : 0; g < 3; g++)
        {
            (void)ChainOf(name, name->source, genders[g], width, field, chain, sizeof(chain));
            const char* value = OwnValue(n, at, chain);
            if (value != nullptr)
            {
                return value;
            }
        }
    }
    return width == widthShort ? nullptr : PowerField(n, locale, id, widthShort, field);
}

// A field's value. CLDR's resolution gives it, but a plural category
// ICU's data lacks takes other's in the same width, as ICU's sinks
// leave it to other: CLDR would follow root's alias to short first
// ("1 kg" for Vietnamese's long "one", which no locale gives, where ICU
// writes "1 kilôgam"). A power of a gender is PowerField's.
static const char* Target(const Names* n, const char* locale, int id, int width, int field)
{
    const UnitName* name = &n->names[id];
    bool category = field >= unitNameZero && field < unitNameOther;
    if (name->gender != nullptr)
    {
        const char* value = PowerField(n, locale, id, width, field);
        return value != nullptr || !category ? value
                                             : PowerField(n, locale, id, width, unitNameOther);
    }
    const char* value;
    if (Located(n, locale, name, width, field, &value) ||
        (category && Located(n, locale, name, width, unitNameOther, &value)))
    {
        return value;
    }
    field = category ? unitNameOther : field;
    char chain[maxChain];
    return ChainOf(name, name->source, nullptr, width, field, chain, sizeof(chain))
               ? Resolved(n, locale, chain)
               : nullptr;
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
           field < unitNameOther && Given(n, locale, name, row, field) &&
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
    return Copy(lowered, needed);
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
            (void)ChainOf(name, name->source, nullptr, widthLong, fieldGender, chain,
                          sizeof(chain));
            const char* gender = Resolved(n, locale, chain);
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
    unsigned categories = CategoriesOf(n, locale);
    char chain[maxChain];
    for (int width = 0; width < widthCount; width++)
    {
        for (int id = 0; id < n->count; id++)
        {
            for (int f = 0; f < unitNameFields; f++)
            {
                int field = s_fieldOrder[f];
                const UnitName* name = &n->names[id];
                if (Wanted(categories, field) &&
                    ChainOf(name, name->type, name->gender, width, field, chain, sizeof(chain)))
                {
                    Take(n, filed, locale, width, id, field, Target(n, locale, id, width, field));
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

// A derivation's value: a category's index, compound or none.
static uint8_t DerivedValue(const char* value, const char* where)
{
    if (value == nullptr)
    {
        GenDie("a derivation without its value", where);
    }
    if (strcmp(value, "compound") == 0)
    {
        return unitDerivedCompound;
    }
    for (int c = 0; c < categoryCount; c++)
    {
        if (strcmp(value, s_categories[c]) == 0)
        {
            return (uint8_t)c;
        }
    }
    GenDie("a plural derivation of no category", value);
}

// Fills a language's record from its grammaticalDerivations element.
static void ReadDerivation(const XmlNode* node, uint8_t* record)
{
    static const char* const s_structures[unitDeriveStructures] = {"per", "times", "power",
                                                                   "prefix"};
    memset(record + unitDerivationLanguage, unitDerivedNone, 2 * unitDeriveStructures);
    for (int i = 0; i < node->childCount; i++)
    {
        const XmlNode* rule = node->children[i];
        const char* feature = XmlGetAttribute(rule, "feature");
        const char* structure = XmlGetAttribute(rule, "structure");
        if (strcmp(rule->name, "deriveComponent") != 0 || feature == nullptr ||
            strcmp(feature, "plural") != 0)
        {
            continue;
        }
        int s = 0;
        while (s < unitDeriveStructures && structure != nullptr &&
               strcmp(structure, s_structures[s]) != 0)
        {
            s++;
        }
        if (s == unitDeriveStructures || structure == nullptr)
        {
            GenDie("a plural derivation of no structure", structure == nullptr ? "" : structure);
        }
        record[unitDerivationLanguage + 2 * s] =
            DerivedValue(XmlGetAttribute(rule, "value0"), structure);
        record[unitDerivationLanguage + 2 * s + 1] =
            DerivedValue(XmlGetAttribute(rule, "value1"), structure);
    }
}

static int CompareRecords(const void* a, const void* b)
{
    return memcmp(a, b, unitDerivationLanguage);
}

// The derivations' unit from supplemental/grammaticalFeatures.xml.
static int AddDerivations(const Names* n, DataFile* file, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/grammaticalFeatures.xml",
             n->input->commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    enum
    {
        maxLanguages = 64,
    };
    uint8_t unit[unitDerivationsHeaderSize + maxLanguages * unitDerivationSize] = {0};
    uint32_t count = 0;
    const XmlNode* data = XmlFindChild(root, "grammaticalData");
    for (int i = 0; data != nullptr && i < data->childCount; i++)
    {
        const XmlNode* node = data->children[i];
        const char* locales = XmlGetAttribute(node, "locales");
        for (const char* at = locales;
             strcmp(node->name, "grammaticalDerivations") == 0 && at != nullptr && *at != '\0';)
        {
            size_t length = strcspn(at, " ");
            if (length == 0 || length > unitDerivationLanguage || count == maxLanguages)
            {
                GenDie("a derivation's language too long, or too many", at);
            }
            uint8_t* record = unit + unitDerivationsHeaderSize + count++ * unitDerivationSize;
            memcpy(record, at, length);
            ReadDerivation(node, record);
            at += length + strspn(at + length, " ");
        }
    }
    XmlFree(root);
    qsort(unit + unitDerivationsHeaderSize, count, unitDerivationSize, CompareRecords);
    GenPut(unit, count, 2);
    DataFileAdd(file, kindUnitDerivations, nullptr, unit,
                unitDerivationsHeaderSize + count * unitDerivationSize);
    return 1;
}

int GenerateUnitNames(const UnitNamesInput* input, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    static Names s_names;
    Names* n = &s_names;
    *n = (Names){.input = input};
    if (!ReadCategories(n, errorOut, errorCapacity))
    {
        return 0;
    }
    CollectNames(n);
    AddIds(n, file);
    if (!AddDerivations(n, file, errorOut, errorCapacity))
    {
        return 0;
    }
    for (int i = 0; i < input->count; i++)
    {
        TakeChain(n, input->ids[i], file);
    }
    return 1;
}
