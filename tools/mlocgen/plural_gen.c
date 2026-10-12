// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Plural rules (plural_gen.h). A rule set's identity is its rules' text
// without samples, so locales that share rules share a function. A
// condition is compiled relation by relation: `and` binds tighter than
// `or`, `n` equals an integer only when t is 0, and `x != y` is
// `!(x = y)` (UTS #35 Part 3).

#include "plural_gen.h"

#include "kinds.h"
#include "map.h"
#include "plural_rules.h"
#include "xml.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxSets = 128,
    maxRanges = 64,
    maxText = 4096,
    categoryCount = 6,
};

static const char* const s_categories[categoryCount] = {"zero", "one",  "two",
                                                        "few",  "many", "other"};

typedef struct Text
{
    char* bytes;
    size_t size;
    size_t capacity;
} Text;

typedef struct RuleSet
{
    // The rules without samples, "one:...;other:", which identifies it.
    char key[maxText];
    // Each category's condition, empty for one the set does not use.
    char conditions[categoryCount][1024];
    bool used[categoryCount];
    bool ordinal;
    // The locales of its first group, for the function's comment.
    char locales[96];
} RuleSet;

typedef struct Plurals
{
    RuleSet sets[maxSets];
    int setCount;
    uint8_t ranges[maxRanges][categoryCount * categoryCount];
    int rangeCount;
    // Each locale id to its rule (cardinal, ordinal) or table, plus one.
    Map cardinal;
    Map ordinal;
    Map range;
    Map ids;
} Plurals;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: plurals: %s: %s\n", message, detail);
    exit(1);
}

[[gnu::format(printf, 2, 3)]] static void Append(Text* text, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    va_list again;
    va_copy(again, args);
    int length = vsnprintf(nullptr, 0, format, args);
    va_end(args);
    if (text->size + (size_t)length + 1 > text->capacity)
    {
        size_t capacity = (text->capacity + (size_t)length + 1) * 2;
        char* grown = realloc(text->bytes, capacity);
        if (grown == nullptr)
        {
            Die("out of memory", "text");
        }
        text->bytes = grown;
        text->capacity = capacity;
    }
    vsnprintf(text->bytes + text->size, (size_t)length + 1, format, again);
    va_end(again);
    text->size += (size_t)length;
}

static int Category(const char* name)
{
    for (int c = 0; c < categoryCount; c++)
    {
        if (name != nullptr && strcmp(name, s_categories[c]) == 0)
        {
            return c;
        }
    }
    Die("no such category", name == nullptr ? "(none)" : name);
}

// A rule's condition: its text up to the first sample, trimmed.
static void Condition(const char* text, char* out, size_t capacity)
{
    size_t length = strcspn(text, "@");
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\n'))
    {
        length--;
    }
    size_t start = strspn(text, " \n");
    length = length > start ? length - start : 0;
    if (length >= capacity)
    {
        Die("a condition too long", text);
    }
    memcpy(out, text + start, length);
    out[length] = '\0';
}

static uint64_t Number(const char* text, const char* condition)
{
    char* end = nullptr;
    unsigned long long value = strtoull(text, &end, 10);
    if (end == text || value > 1000000000000000000ull)
    {
        Die("not a number of a rule", condition);
    }
    return value;
}

// Writes one relation's C expression: the operand (with its remainder)
// against each value or range of the list.
static void Relation(Text* out, char operand, uint64_t modulus, bool equal, char* list,
                     const char* condition)
{
    static const char s_operands[] = "nivwftce";
    static const char* const s_fields[] = {"o->i", "o->i", "o->v", "o->w",
                                           "o->f", "o->t", "o->c", "o->c"};
    const char* found = strchr(s_operands, operand);
    if (operand == '\0' || found == nullptr)
    {
        Die("no such operand", condition);
    }
    bool wide = operand == 'n' || operand == 'i' || operand == 'f' || operand == 't';
    if (modulus != 0 && wide && 1000000000000000000ull % modulus != 0)
    {
        // The wide operands keep remainders by powers of ten up to 10^18.
        Die("a remainder the operands do not keep", condition);
    }
    char value[64];
    if (modulus != 0)
    {
        snprintf(value, sizeof(value), "%s %% %lluu", s_fields[found - s_operands],
                 (unsigned long long)modulus);
    }
    else
    {
        snprintf(value, sizeof(value), "%s", s_fields[found - s_operands]);
    }
    Append(out, "%s(%s(", equal ? "" : "!", operand == 'n' ? "o->t == 0 && " : "");
    bool first = true;
    for (char* item = strtok(list, ","); item != nullptr; item = strtok(nullptr, ","))
    {
        char* dots = strstr(item, "..");
        Append(out, "%s", first ? "" : " || ");
        if (dots != nullptr)
        {
            *dots = '\0';
            uint64_t low = Number(item, condition);
            uint64_t high = Number(dots + 2, condition);
            // An unsigned operand is never below 0, which compilers warn of.
            if (low == 0)
            {
                Append(out, "%s <= %lluu", value, (unsigned long long)high);
            }
            else
            {
                Append(out, "(%s >= %lluu && %s <= %lluu)", value, (unsigned long long)low, value,
                       (unsigned long long)high);
            }
        }
        else
        {
            Append(out, "%s == %lluu", value, (unsigned long long)Number(item, condition));
        }
        first = false;
    }
    Append(out, "))");
}

// Compiles a condition into a C expression over o.
static void Compile(const char* condition, Text* out)
{
    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", condition);
    char* words[128];
    int count = 0;
    for (char* w = strtok(copy, " "); w != nullptr && count < 128; w = strtok(nullptr, " "))
    {
        words[count++] = w;
    }
    // Each or-term in parentheses, so that and binds tighter visibly.
    Append(out, "(");
    for (int at = 0; at < count;)
    {
        char operand = strlen(words[at]) == 1 ? words[at][0] : '\0';
        at++;
        uint64_t modulus = 0;
        if (at + 1 < count && strcmp(words[at], "%") == 0)
        {
            modulus = Number(words[at + 1], condition);
            at += 2;
        }
        if (at + 1 >= count || (strcmp(words[at], "=") != 0 && strcmp(words[at], "!=") != 0))
        {
            Die("a relation without = or !=", condition);
        }
        Relation(out, operand, modulus, strcmp(words[at], "=") == 0, words[at + 1], condition);
        at += 2;
        if (at < count)
        {
            bool isAnd = strcmp(words[at], "and") == 0;
            if (!isAnd && strcmp(words[at], "or") != 0)
            {
                Die("neither and nor or", condition);
            }
            Append(out, isAnd ? " && " : ") || (");
            at++;
        }
    }
    Append(out, ")");
}

// The set of a group's rules, added when no set has them yet.
static int FindSet(Plurals* p, const XmlNode* group, bool ordinal)
{
    RuleSet set = {.ordinal = ordinal};
    for (int k = 0; k < group->childCount; k++)
    {
        const XmlNode* rule = group->children[k];
        if (strcmp(rule->name, "pluralRule") != 0)
        {
            continue;
        }
        int c = Category(XmlGetAttribute(rule, "count"));
        Condition(rule->text, set.conditions[c], sizeof(set.conditions[c]));
        set.used[c] = true;
        if ((c == categoryCount - 1) != (set.conditions[c][0] == '\0'))
        {
            Die("other has a condition, or another category none", rule->text);
        }
    }
    if (!set.used[categoryCount - 1])
    {
        Die("a rule set without other", XmlGetAttribute(group, "locales"));
    }
    size_t at = 0;
    for (int c = 0; c < categoryCount; c++)
    {
        if (set.used[c])
        {
            at += (size_t)snprintf(set.key + at, sizeof(set.key) - at, "%s:%s;", s_categories[c],
                                   set.conditions[c]);
        }
    }
    for (int s = 0; s < p->setCount; s++)
    {
        if (p->sets[s].ordinal == ordinal && strcmp(p->sets[s].key, set.key) == 0)
        {
            return s;
        }
    }
    if (p->setCount == maxSets)
    {
        Die("too many rule sets", set.key);
    }
    snprintf(set.locales, sizeof(set.locales), "%.95s", XmlGetAttribute(group, "locales"));
    p->sets[p->setCount] = set;
    return p->setCount++;
}

// Files a value under each locale id of a list, and the ids themselves.
static void FileLocales(Plurals* p, Map* map, const char* locales, int value)
{
    char copy[maxText];
    snprintf(copy, sizeof(copy), "%s", locales == nullptr ? "" : locales);
    for (char* id = strtok(copy, " \n\t"); id != nullptr; id = strtok(nullptr, " \n\t"))
    {
        if (MapGet(map, id) != nullptr)
        {
            Die("a locale given twice", id);
        }
        MapPut(map, id, (void*)(uintptr_t)(value + 1));
        MapPut(&p->ids, id, (void*)(uintptr_t)1);
    }
}

static XmlNode* ReadFile(const char* commonDirectory, const char* name, char* errorOut,
                         size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/%s", commonDirectory, name);
    return XmlParseFile(path, errorOut, errorCapacity);
}

static void ReadRules(Plurals* p, const XmlNode* root, bool ordinal)
{
    const XmlNode* plurals = XmlFindChild(root, "plurals");
    for (int g = 0; plurals != nullptr && g < plurals->childCount; g++)
    {
        const XmlNode* group = plurals->children[g];
        if (strcmp(group->name, "pluralRules") == 0)
        {
            int set = FindSet(p, group, ordinal);
            FileLocales(p, ordinal ? &p->ordinal : &p->cardinal, XmlGetAttribute(group, "locales"),
                        set);
        }
    }
}

static void ReadRanges(Plurals* p, const XmlNode* root)
{
    const XmlNode* plurals = XmlFindChild(root, "plurals");
    for (int g = 0; plurals != nullptr && g < plurals->childCount; g++)
    {
        const XmlNode* group = plurals->children[g];
        if (strcmp(group->name, "pluralRanges") != 0)
        {
            continue;
        }
        if (p->rangeCount == maxRanges)
        {
            Die("too many range tables", XmlGetAttribute(group, "locales"));
        }
        uint8_t* table = p->ranges[p->rangeCount];
        memset(table, 0xFF, categoryCount * categoryCount);
        for (int k = 0; k < group->childCount; k++)
        {
            const XmlNode* range = group->children[k];
            if (strcmp(range->name, "pluralRange") == 0)
            {
                int start = Category(XmlGetAttribute(range, "start"));
                int end = Category(XmlGetAttribute(range, "end"));
                table[start * categoryCount + end] =
                    (uint8_t)Category(XmlGetAttribute(range, "result"));
            }
        }
        FileLocales(p, &p->range, XmlGetAttribute(group, "locales"), p->rangeCount);
        p->rangeCount++;
    }
}

// The value filed under an id or the first of its truncations, else
// root's, else 0xFF.
static uint8_t Resolve(const Map* map, const char* id)
{
    char key[64];
    snprintf(key, sizeof(key), "%s", id);
    for (;;)
    {
        uintptr_t value = (uintptr_t)MapGet(map, key);
        if (value != 0)
        {
            return (uint8_t)(value - 1);
        }
        char* cut = strrchr(key, '_');
        if (cut == nullptr)
        {
            break;
        }
        *cut = '\0';
    }
    uintptr_t root = (uintptr_t)MapGet(map, "root");
    return root != 0 ? (uint8_t)(root - 1) : 0xFF;
}

typedef struct IdList
{
    char** ids;
    size_t count;
} IdList;

static void CollectId(const char* key, void* value, void* context)
{
    (void)value;
    IdList* list = context;
    list->ids[list->count++] = (char*)key;
}

static int CompareIds(const void* a, const void* b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

static void AddUnits(Plurals* p, DataFile* file)
{
    MapPut(&p->ids, "root", (void*)(uintptr_t)1);
    IdList list = {calloc(p->ids.count, sizeof(char*)), 0};
    if (list.ids == nullptr)
    {
        Die("out of memory", "ids");
    }
    MapEach(&p->ids, CollectId, &list);
    qsort(list.ids, list.count, sizeof(char*), CompareIds);
    for (size_t i = 0; i < list.count; i++)
    {
        uint8_t unit[pluralUnitSize] = {0};
        unit[0] = Resolve(&p->cardinal, list.ids[i]);
        unit[1] = Resolve(&p->ordinal, list.ids[i]);
        unit[2] = Resolve(&p->range, list.ids[i]);
        DataFileAdd(file, kindPlurals, list.ids[i], unit, sizeof(unit));
    }
    free(list.ids);
    size_t size = pluralRangesHeaderSize + (size_t)p->rangeCount * pluralRangeSize;
    uint8_t* ranges = calloc(size, 1);
    if (ranges == nullptr)
    {
        Die("out of memory", "ranges");
    }
    ranges[0] = (uint8_t)p->rangeCount;
    for (int r = 0; r < p->rangeCount; r++)
    {
        memcpy(ranges + pluralRangesHeaderSize + (size_t)r * pluralRangeSize, p->ranges[r],
               pluralRangeSize);
    }
    DataFileAdd(file, kindPluralRanges, nullptr, ranges, size);
    free(ranges);
}

static void Source(const Plurals* p, Text* out)
{
    Append(out, "// SPDX-License-Identifier: MIT\n"
                "// Copyright (c) 2026 Sirac Ozmen\n"
                "//\n"
                "// The plural rule sets of CLDR (record mloc-0020), cardinal ones first.\n"
                "// Generated by tools/mlocgen; do not edit.\n"
                "// clang-format off\n\n"
                "#include \"plural_rules.h\"\n\n");
    for (int s = 0; s < p->setCount; s++)
    {
        const RuleSet* set = &p->sets[s];
        Append(out, "// %s: %s\nstatic uint8_t Rule%d(const mlocPluralOperands* o)\n{\n",
               set->ordinal ? "ordinal" : "cardinal", set->locales, s);
        bool any = false;
        for (int c = 0; c < categoryCount - 1; c++)
        {
            if (set->used[c])
            {
                Append(out, "    if (");
                Compile(set->conditions[c], out);
                Append(out, ")\n    {\n        return %d;\n    }\n", c);
                any = true;
            }
        }
        if (!any)
        {
            Append(out, "    (void)o;\n");
        }
        Append(out, "    return %d;\n}\n\n", categoryCount - 1);
    }
    Append(out, "const uint32_t mlocPluralRuleSetCount = %d;\n\n", p->setCount);
    Append(out, "const PluralRule mlocPluralRuleSets[%d] = {\n", p->setCount);
    for (int s = 0; s < p->setCount; s++)
    {
        unsigned categories = 0;
        for (int c = 0; c < categoryCount; c++)
        {
            categories |= p->sets[s].used[c] ? 1u << c : 0u;
        }
        Append(out, "    {Rule%d, %d, 0x%02X},\n", s, p->sets[s].ordinal ? 1 : 0, categories);
    }
    Append(out, "};\n");
}

static void FreePlurals(Plurals* p)
{
    MapFree(&p->cardinal);
    MapFree(&p->ordinal);
    MapFree(&p->range);
    MapFree(&p->ids);
}

// Reads the three files into p; 0 and a message on failure.
static int ReadPlurals(const char* commonDirectory, Plurals* p, char* errorOut,
                       size_t errorCapacity)
{
    static const char* const s_files[3] = {"plurals.xml", "ordinals.xml", "pluralRanges.xml"};
    for (int f = 0; f < 3; f++)
    {
        XmlNode* root = ReadFile(commonDirectory, s_files[f], errorOut, errorCapacity);
        if (root == nullptr)
        {
            return 0;
        }
        if (f < 2)
        {
            ReadRules(p, root, f == 1);
        }
        else
        {
            ReadRanges(p, root);
        }
        XmlFree(root);
    }
    return 1;
}

int GeneratePluralUnits(const char* commonDirectory, DataFile* file, char** sourceOut,
                        size_t* sourceSizeOut, char* errorOut, size_t errorCapacity)
{
    static Plurals s_plurals;
    memset(&s_plurals, 0, sizeof(s_plurals));
    if (!ReadPlurals(commonDirectory, &s_plurals, errorOut, errorCapacity))
    {
        FreePlurals(&s_plurals);
        return 0;
    }
    // Cardinal sets come first, as plurals.xml is read first.
    AddUnits(&s_plurals, file);
    Text source = {0};
    Source(&s_plurals, &source);
    FreePlurals(&s_plurals);
    *sourceOut = source.bytes;
    *sourceSizeOut = source.size;
    return 1;
}

// A sample's digits without its point, as an integer.
static long long Scaled(const char* text)
{
    long long value = 0;
    for (const char* c = text; *c != '\0'; c++)
    {
        if (*c >= '0' && *c <= '9')
        {
            value = value * 10 + (*c - '0');
        }
        else if (*c != '.')
        {
            Die("a sample that is no decimal", text);
        }
    }
    return value;
}

// Prints the samples after one keyword ("@integer" or "@decimal").
static void PrintSamples(const char* type, const char* id, const char* category,
                         const char* samples)
{
    char copy[maxText];
    size_t length = strcspn(samples, "@");
    snprintf(copy, sizeof(copy), "%.*s", (int)length, samples);
    for (char* item = strtok(copy, ", \n"); item != nullptr; item = strtok(nullptr, ", \n"))
    {
        if (strcmp(item, "\xE2\x80\xA6") == 0 || strcmp(item, "...") == 0)
        {
            continue;
        }
        char* tilde = strchr(item, '~');
        if (tilde == nullptr)
        {
            printf("%s\t%s\t%s\t%s\n", type, id, category, item);
            continue;
        }
        *tilde = '\0';
        const char* point = strchr(item, '.');
        int fraction = point == nullptr ? 0 : (int)strlen(point + 1);
        const char* endPoint = strchr(tilde + 1, '.');
        if ((endPoint == nullptr ? 0 : (int)strlen(endPoint + 1)) != fraction ||
            strchr(item, 'c') != nullptr)
        {
            Die("a sample range of two forms", item);
        }
        // The range's ends as integers of their fraction digits.
        long long scale = 1;
        for (int k = 0; k < fraction; k++)
        {
            scale *= 10;
        }
        long long from = Scaled(item);
        long long to = Scaled(tilde + 1);
        for (long long v = from; v <= to; v++)
        {
            if (fraction == 0)
            {
                printf("%s\t%s\t%s\t%lld\n", type, id, category, v);
            }
            else
            {
                printf("%s\t%s\t%s\t%lld.%0*lld\n", type, id, category, v / scale, fraction,
                       v % scale);
            }
        }
    }
}

static void PrintGroup(const XmlNode* group, const char* type)
{
    // The ids are split by hand: the samples use strtok.
    const char* ids = XmlGetAttribute(group, "locales");
    for (const char* at = ids; at != nullptr && *at != '\0';)
    {
        at += strspn(at, " \n\t");
        size_t length = strcspn(at, " \n\t");
        if (length == 0)
        {
            break;
        }
        char id[64];
        snprintf(id, sizeof(id), "%.*s", (int)length, at);
        at += length;
        for (int k = 0; k < group->childCount; k++)
        {
            const XmlNode* rule = group->children[k];
            const char* category = XmlGetAttribute(rule, "count");
            for (const char* sample = strchr(rule->text, '@'); sample != nullptr;
                 sample = strchr(sample + 1, '@'))
            {
                const char* keyword = strncmp(sample, "@integer", 8) == 0 ? "@integer" : "@decimal";
                PrintSamples(type, id, category, sample + strlen(keyword));
            }
        }
    }
}

int PrintPluralSamples(const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    static const char* const s_files[2] = {"plurals.xml", "ordinals.xml"};
    for (int f = 0; f < 2; f++)
    {
        XmlNode* root = ReadFile(commonDirectory, s_files[f], errorOut, errorCapacity);
        if (root == nullptr)
        {
            return 0;
        }
        const XmlNode* plurals = XmlFindChild(root, "plurals");
        for (int g = 0; plurals != nullptr && g < plurals->childCount; g++)
        {
            if (strcmp(plurals->children[g]->name, "pluralRules") == 0)
            {
                PrintGroup(plurals->children[g], f == 0 ? "cardinal" : "ordinal");
            }
        }
        XmlFree(root);
    }
    return 1;
}
