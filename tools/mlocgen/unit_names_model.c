// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names as ICU's data gives them (unit_names_model.h).

#include "unit_names_model.h"

#include "unit_derivations_gen.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const s_widths[widthCount] = {"long", "short", "narrow"};
static const char* const s_categories[categoryCount] = {"zero", "one",  "two",
                                                        "few",  "many", "other"};
static const char s_prefix[] = "//ldml/units/unitLength[@type=\"";
static const char s_empty[] = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";
static const char s_inherit[] = "\xE2\x86\x91\xE2\x86\x91\xE2\x86\x91";

char* NamesCopy(const char* text, size_t length)
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
        .id = NamesCopy(id, strlen(id)),
        .type = NamesCopy(type, strlen(type)),
        .source = NamesCopy(type, length),
        .gender = gender != nullptr ? NamesCopy(gender, strlen(gender)) : nullptr,
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
    MapPut(&n->aliases, type, NamesCopy(target, targetLength));
}

static int CompareNames(const void* a, const void* b)
{
    return strcmp(((const UnitName*)a)->id, ((const UnitName*)b)->id);
}

void NamesCollect(Names* n)
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
int NamesReadCategories(Names* n, char* errorOut, size_t errorCapacity)
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
unsigned NamesCategoriesOf(const Names* n, const char* locale)
{
    return RuleCategories(n, locale) | 1u << 1;
}

// The chain of a name's field in a width, for a type and a gender, or
// false where the field does not apply: a compound key has its pattern in
// the other field, a power one per plural category, a unit its gender.
bool NamesChainOf(const UnitName* name, const char* type, const char* gender, int width, int field,
                  char* out, size_t capacity)
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
const char* NamesResolved(const Names* n, const char* locale, const char* chain)
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

// Whether a locale keeps a long unit pattern of a case, in a plural
// category of its rules: the logical group of a long unit pattern holds
// its cases as well.
static bool CaseKept(const Names* n, const char* locale, const char* type, unsigned rules)
{
    char chain[maxChain];
    for (int k = 1; k < unitCaseCount; k++)
    {
        for (int c = 0; c < categoryCount; c++)
        {
            snprintf(chain, sizeof(chain),
                     "%s%s\"]/unit[@type=\"%s\"]/unitPattern[@case=\"%s\"][@count=\"%s\"]",
                     s_prefix, s_widths[widthLong], type, UnitCaseName(k), s_categories[c]);
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
    if (!NamesChainOf(name, type, nullptr, width, field, chain, sizeof(chain)))
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
            NamesChainOf(name, type, nullptr, width, unitNameZero + c, chain, sizeof(chain)))
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
    (void)NamesChainOf(name, type, nullptr, width, field, chain, sizeof(chain));
    const Leaf* leaf = OwnLeaf(n, locale, chain);
    if (Kept(n, locale, chain, leaf))
    {
        return leaf->text;
    }
    const char* value = NamesResolved(n, locale, chain);
    if (value == nullptr &&
        NamesChainOf(name, type, nullptr, width, unitNameOther, chain, sizeof(chain)))
    {
        value = NamesResolved(n, locale, chain);
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
bool NamesGiven(const Names* n, const char* locale, const UnitName* name, int width, int field)
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
            (void)NamesChainOf(name, name->source, genders[g], width, field, chain, sizeof(chain));
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
const char* NamesTarget(const Names* n, const char* locale, int id, int width, int field)
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
    return NamesChainOf(name, name->source, nullptr, width, field, chain, sizeof(chain))
               ? NamesResolved(n, locale, chain)
               : nullptr;
}
