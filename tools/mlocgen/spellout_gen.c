// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out rules (spellout_gen.h). A group's unit holds its rule
// sets, rules, pieces, plural blocks and decimal patterns in the layout
// of src/spellout_tables.h; each rule's text is cut into pieces at its
// substitutions and plural block, in the order ICU writes them.

#include "spellout_gen.h"

#include "keys.h"
#include "kinds.h"
#include "map.h"
#include "spellout_tables.h"
#include "table_gen.h"
#include "xml.h"

#include "maul-unicode/case.h"
#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

static_assert((int)rbnfMultiplier == (int)spelloutMultiplier &&
                  (int)rbnfModulus == (int)spelloutModulus && (int)rbnfSame == (int)spelloutSame &&
                  (int)rbnfIntegral == (int)spelloutIntegral &&
                  (int)rbnfFraction == (int)spelloutFraction &&
                  (int)rbnfAbsolute == (int)spelloutAbsolute &&
                  (int)rbnfNumerator == (int)spelloutNumerator,
              "the compiler's substitution kinds are the layout's");
static_assert((int)rbnfNegative == (int)spelloutNegative && (int)rbnfNaN == (int)spelloutNaN,
              "the compiler's rule kinds are the layout's");

enum
{
    maxFiles = 512,
    maxSystems = 64,
    maxCodePoint = 0x10FFFF,
};

typedef struct Bytes
{
    uint8_t* bytes;
    size_t size;
    size_t capacity;
} Bytes;

static uint8_t* Grow(Bytes* b, size_t length)
{
    if (b->size + length > b->capacity)
    {
        b->capacity = (b->size + length) * 2;
        b->bytes = realloc(b->bytes, b->capacity);
        if (b->bytes == nullptr)
        {
            GenDie("out of memory", "spellout units");
        }
    }
    uint8_t* at = b->bytes + b->size;
    memset(at, 0, length);
    b->size += length;
    return at;
}

typedef struct Builder
{
    Bytes sets;
    Bytes rules;
    Bytes pieces;
    Bytes pluralBytes;
    Bytes patterns;
    Bytes text;
    // Each text to its offset plus one.
    Map strings;
    // Each pattern to its index plus one.
    Map patternIndexes;
    // Each plural block's forms to its index plus one.
    Map plurals;
    int patternCount;
    int ruleCount;
    int pieceCount;
    int pluralCount;
} Builder;

// The offset of a text in the unit's text, stored once.
static uint32_t AddText(Builder* b, const char* text, size_t length)
{
    char* key = GenAllocate(length + 1);
    memcpy(key, text, length);
    uintptr_t found = (uintptr_t)MapGet(&b->strings, key);
    if (found != 0)
    {
        free(key);
        return (uint32_t)(found - 1);
    }
    uint32_t offset = (uint32_t)b->text.size;
    memcpy(Grow(&b->text, length), text, length);
    if (b->text.size > spelloutMaxText)
    {
        GenDie("a spell-out group's text passes its room", key);
    }
    MapPut(&b->strings, key, (void*)(uintptr_t)(offset + 1));
    free(key);
    return offset;
}

// A decimal pattern ("#,##0.#") as DecimalFormat reads it: grouping from
// the commas (the secondary size from the last two), minimum integer
// digits from the zeros, fraction digits from the zeros and hashes after
// the point.
static void CompilePattern(const char* p, uint8_t* out)
{
    size_t n = strlen(p);
    const char* point = strchr(p, '.');
    size_t integer = point == nullptr ? n : (size_t)(point - p);
    int digits = 0;
    int zeros = 0;
    int last = -1;
    int before = -1;
    for (size_t i = 0; i < integer; i++)
    {
        if (p[i] == '#' && zeros == 0)
        {
            digits++;
        }
        else if (p[i] == '0')
        {
            digits++;
            zeros++;
        }
        else if (p[i] == ',')
        {
            before = last;
            last = digits;
        }
        else
        {
            GenDie("a decimal pattern the library does not read", p);
        }
    }
    int primary = last >= 0 ? digits - last : 0;
    int secondary = before >= 0 ? last - before : primary;
    int minFraction = 0;
    int maxFraction = 0;
    for (size_t i = integer + 1; i < n; i++)
    {
        if (p[i] == '0' && maxFraction == minFraction)
        {
            minFraction++;
            maxFraction++;
        }
        else if (p[i] == '#')
        {
            maxFraction++;
        }
        else
        {
            GenDie("a decimal pattern the library does not read", p);
        }
    }
    if (zeros < 1 || zeros > 9 || maxFraction > 20 ||
        (last >= 0 && (primary == 0 || secondary == 0)))
    {
        GenDie("a decimal pattern the library does not read", p);
    }
    out[0] = (uint8_t)zeros;
    out[1] = (uint8_t)minFraction;
    out[2] = (uint8_t)maxFraction;
    out[3] = (uint8_t)primary;
    out[4] = (uint8_t)secondary;
    out[5] = point != nullptr && maxFraction == 0 ? spelloutPatternPoint : 0;
}

static uint16_t PatternIndex(Builder* b, const char* pattern)
{
    uintptr_t found = (uintptr_t)MapGet(&b->patternIndexes, pattern);
    if (found != 0)
    {
        return (uint16_t)(found - 1);
    }
    CompilePattern(pattern, Grow(&b->patterns, spelloutPatternSize));
    MapPut(&b->patternIndexes, pattern, (void*)(uintptr_t)(b->patternCount + 1));
    return (uint16_t)b->patternCount++;
}

static void PutPiece(Builder* b, uint8_t type, uint8_t detail, uint16_t index, uint32_t offset)
{
    uint8_t* p = Grow(&b->pieces, spelloutPieceSize);
    p[0] = type;
    p[1] = detail;
    GenPut(p + 2, index, 2);
    GenPut(p + 4, offset, 2);
    b->pieceCount++;
}

static void TextPiece(Builder* b, const RbnfRule* rule, size_t from, size_t to)
{
    if (to <= from)
    {
        return;
    }
    if (to - from >= spelloutNone)
    {
        GenDie("a rule's text is too long", rule->text);
    }
    PutPiece(b, spelloutText, 0, (uint16_t)(to - from), AddText(b, rule->text + from, to - from));
}

static void SubPiece(Builder* b, const RbnfSub* sub, int setFirst)
{
    uint8_t detail = (uint8_t)sub->kind;
    detail |= sub->triple ? spelloutSubTriple : 0;
    detail |= sub->zeros ? spelloutSubZeros : 0;
    detail |= sub->byDigits ? spelloutSubByDigits : 0;
    uint16_t index = 0;
    if (sub->set < 0)
    {
        detail |= spelloutSubPattern;
        index = PatternIndex(b, sub->pattern);
    }
    else
    {
        index = (uint16_t)sub->set;
    }
    uint32_t rule = sub->kind == rbnfModulus && sub->triple && sub->predecessor >= 0
                        ? (uint32_t)(setFirst + sub->predecessor)
                        : spelloutNone;
    PutPiece(b, spelloutSubstitution, detail, index, rule);
}

// A plural block's record, stored once per distinct set of forms.
static void PluralPiece(Builder* b, const RbnfRule* rule)
{
    char key[2048] = "";
    size_t used = 0;
    for (int c = 0; c < rbnfCategories; c++)
    {
        const char* form = rule->forms[c] == nullptr ? "\x02" : rule->forms[c];
        int n = snprintf(key + used, sizeof(key) - used, "%s\x01", form);
        if (n < 0 || (size_t)n >= sizeof(key) - used)
        {
            GenDie("a plural block is too long", rule->text);
        }
        used += (size_t)n;
    }
    // The order, a nibble per category written, 0xF past the last.
    uint32_t order = 0xFFFFFFFFu;
    for (int k = 0; k < rule->formCount; k++)
    {
        order = (order & ~(0xFu << (4 * k))) | (uint32_t)rule->order[k] << (4 * k);
    }
    if (snprintf(key + used, sizeof(key) - used, "%08x", order) != 8)
    {
        GenDie("a plural block is too long", rule->text);
    }
    uintptr_t found = (uintptr_t)MapGet(&b->plurals, key);
    if (found == 0)
    {
        uint8_t* p = Grow(&b->pluralBytes, spelloutPluralSize);
        for (int c = 0; c < rbnfCategories; c++)
        {
            const char* form = rule->forms[c];
            size_t length = form == nullptr ? 0 : strlen(form);
            if (length >= spelloutNone)
            {
                GenDie("a plural form is too long", form);
            }
            GenPut(p + 4 * c, form == nullptr ? 0 : AddText(b, form, length), 2);
            GenPut(p + 4 * c + 2, form == nullptr ? spelloutNone : length, 2);
        }
        GenPut(p + 24, order, 4);
        found = (uintptr_t)++b->pluralCount;
        MapPut(&b->plurals, key, (void*)found);
    }
    PutPiece(b, spelloutPlural, rule->ordinal ? 1 : 0, (uint16_t)(found - 1), 0);
}

// A rule's pieces: its text cut at each substitution and at its plural
// block; a substitution where the block starts comes before it (ICU
// inserts both there), one where it ends after it.
static int AddPieces(Builder* b, const RbnfRule* rule, int setFirst)
{
    int first = b->pieceCount;
    size_t cursor = 0;
    int sub = 0;
    bool pluralDone = !rule->plural;
    for (;;)
    {
        size_t nextSub = sub < rule->subCount ? rule->subs[sub].pos : SIZE_MAX;
        size_t nextPlural = pluralDone ? SIZE_MAX : rule->pluralStart;
        size_t next = nextSub < nextPlural ? nextSub : nextPlural;
        if (next == SIZE_MAX)
        {
            TextPiece(b, rule, cursor, rule->length);
            break;
        }
        TextPiece(b, rule, cursor, next);
        cursor = next;
        if (nextSub <= nextPlural)
        {
            SubPiece(b, &rule->subs[sub++], setFirst);
        }
        else
        {
            PluralPiece(b, rule);
            cursor = rule->pluralEnd;
            pluralDone = true;
        }
    }
    if (b->pieceCount - first > 255)
    {
        GenDie("a rule has too many pieces", rule->text);
    }
    return first;
}

// The exponent of a power of ten, or -1.
static int PowerOfTen(uint64_t divisor)
{
    int exponent = 0;
    for (uint64_t p = 1; p <= divisor; p *= 10, exponent++)
    {
        if (p == divisor)
        {
            return exponent;
        }
        if (p > UINT64_MAX / 10)
        {
            break;
        }
    }
    return -1;
}

static void AddRule(Builder* b, const RbnfRule* rule, int setFirst)
{
    uint64_t divisor = RbnfDivisor(rule);
    int power = PowerOfTen(divisor);
    bool normal = rule->kind == rbnfNormal;
    // A divisor not a power of ten is the radix to the exponent, the
    // radix held in 16 bits.
    if (divisor == 0 || rule->decimalPoint > 0xFFFF ||
        (normal && power < 0 && (rule->radix < 2 || rule->radix > 0xFFFF)))
    {
        GenDie("a rule the layout cannot hold", rule->text);
    }
    bool modulus = false;
    for (int i = 0; i < rule->subCount; i++)
    {
        modulus |= rule->subs[i].kind == rbnfModulus;
    }
    uint8_t flags = 0;
    flags |= modulus && normal && (uint64_t)rule->base % divisor != 0 ? spelloutRollback : 0;
    flags |= power >= 0 ? spelloutPowerOfTen : 0;
    int first = AddPieces(b, rule, setFirst);
    uint8_t* r = Grow(&b->rules, spelloutRuleSize);
    GenPut(r, normal ? (uint64_t)rule->base : 0, 8);
    GenPut(r + 8, (uint64_t)first, 2);
    GenPut(r + 10, normal ? (uint64_t)(power >= 0 ? 10 : rule->radix) : rule->decimalPoint, 2);
    r[12] = (uint8_t)rule->kind;
    r[13] = flags;
    r[14] = (uint8_t)(b->pieceCount - first);
    r[15] = (uint8_t)(power >= 0 ? power : rule->exponent);
    b->ruleCount++;
}

// Whether ICU keeps a special rule: every fraction rule, and the last
// negative, infinity and NaN rule.
static bool Kept(const RbnfSet* set, int index)
{
    RbnfRuleKind kind = set->specials[index].kind;
    if (kind == rbnfImproper || kind == rbnfProper || kind == rbnfDefault)
    {
        return true;
    }
    for (int i = index + 1; i < set->specialCount; i++)
    {
        if (set->specials[i].kind == kind)
        {
            return false;
        }
    }
    return true;
}

static int KeptCount(const RbnfSet* set)
{
    int count = 0;
    for (int i = 0; i < set->specialCount; i++)
    {
        count += Kept(set, i) ? 1 : 0;
    }
    return count;
}

static void Concatenate(Bytes* out, const Bytes* part)
{
    if (part->size > 0)
    {
        memcpy(Grow(out, part->size), part->bytes, part->size);
    }
}

uint8_t* SpelloutPayload(const RbnfGroup* group, size_t* sizeOut)
{
    Builder b = {0};
    int total = 0;
    for (int s = 0; s < group->setCount; s++)
    {
        const RbnfSet* set = &group->sets[s];
        int specials = KeptCount(set);
        size_t nameLength = strlen(set->name);
        if (nameLength > 255 || specials > 0xFF)
        {
            GenDie("a rule set the layout cannot hold", set->name);
        }
        uint8_t* record = Grow(&b.sets, spelloutSetSize);
        GenPut(record, AddText(&b, set->name, nameLength), 2);
        record[2] = (uint8_t)nameLength;
        record[3] = (uint8_t)((set->isPublic ? spelloutSetPublic : 0) |
                              (set->fraction ? spelloutSetFraction : 0) |
                              (set->parseable ? spelloutSetParseable : 0));
        GenPut(record + 4, (uint64_t)total, 2);
        GenPut(record + 6, (uint64_t)set->ruleCount, 2);
        GenPut(record + 8, (uint64_t)specials, 2);
        for (int i = 0; i < set->ruleCount; i++)
        {
            AddRule(&b, &set->rules[i], total);
        }
        for (int i = 0; i < set->specialCount; i++)
        {
            if (Kept(set, i))
            {
                AddRule(&b, &set->specials[i], total);
            }
        }
        total += set->ruleCount + specials;
    }
    if (group->setCount >= spelloutNone || b.ruleCount >= spelloutNone ||
        b.pieceCount >= spelloutNone || b.pluralCount >= spelloutNone ||
        b.patternCount >= spelloutNone || b.text.size > spelloutMaxText)
    {
        GenDie("a spell-out group passes the layout's counts", group->sets[0].name);
    }
    Bytes out = {0};
    uint8_t* header = Grow(&out, spelloutHeaderSize);
    GenPut(header, (uint64_t)group->setCount, 2);
    GenPut(header + 2, (uint64_t)b.ruleCount, 2);
    GenPut(header + 4, (uint64_t)b.pieceCount, 2);
    GenPut(header + 6, (uint64_t)b.pluralCount, 2);
    GenPut(header + 8, (uint64_t)b.patternCount, 2);
    GenPut(header + 10, (uint64_t)group->defaultSet, 2);
    GenPut(header + 12, b.text.size, 2);
    GenPut(header + 14, group->unparseable ? spelloutGroupUnparseable : 0, 2);
    Concatenate(&out, &b.sets);
    Concatenate(&out, &b.rules);
    Concatenate(&out, &b.pieces);
    Concatenate(&out, &b.pluralBytes);
    Concatenate(&out, &b.patterns);
    Concatenate(&out, &b.text);
    free(b.sets.bytes);
    free(b.rules.bytes);
    free(b.pieces.bytes);
    free(b.pluralBytes.bytes);
    free(b.patterns.bytes);
    free(b.text.bytes);
    MapFree(&b.strings);
    MapFree(&b.patternIndexes);
    MapFree(&b.plurals);
    *sizeOut = out.size;
    return out.bytes;
}

static int CompareNames(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

// The ids of common/rbnf's files, sorted; the caller frees each and the
// list.
static char** ListFiles(const char* commonDirectory, int* countOut)
{
    char** ids = GenAllocate(sizeof(char*) * maxFiles);
    int count = 0;
    char path[4096];
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s/rbnf/*.xml", commonDirectory);
    struct _finddata_t found;
    intptr_t handle = _findfirst(path, &found);
    for (int more = handle != -1; more; more = _findnext(handle, &found) == 0)
    {
        const char* name = found.name;
#else
    snprintf(path, sizeof(path), "%s/rbnf", commonDirectory);
    DIR* directory = opendir(path);
    if (directory == nullptr)
    {
        GenDie("cannot list", path);
    }
    for (struct dirent* item = readdir(directory); item != nullptr; item = readdir(directory))
    {
        const char* name = item->d_name;
#endif
        size_t length = strlen(name);
        if (length < 5 || strcmp(name + length - 4, ".xml") != 0)
        {
            continue;
        }
        if (count == maxFiles)
        {
            GenDie("too many files in", path);
        }
        ids[count] = GenAllocate(length - 3);
        memcpy(ids[count], name, length - 4);
        count++;
    }
#ifdef _WIN32
    if (handle != -1)
    {
        _findclose(handle);
    }
#else
    closedir(directory);
#endif
    qsort(ids, (size_t)count, sizeof(char*), CompareNames);
    *countOut = count;
    return ids;
}

static const char* const s_groups[spelloutGroupCount] = {"SpelloutRules", "OrdinalRules",
                                                         "NumberingSystemRules"};
static const uint32_t s_kinds[spelloutGroupCount] = {kindSpelloutRules, kindSpelloutOrdinals,
                                                     kindSpelloutNumbering};

static int GroupOf(const char* type)
{
    for (int g = 0; g < spelloutGroupCount; g++)
    {
        if (type != nullptr && strcmp(type, s_groups[g]) == 0)
        {
            return g;
        }
    }
    return -1;
}

// Notes each public rule set of a group as "id|group|name".
static void NoteSets(Map* sets, const char* id, int group, const RbnfGroup* compiled)
{
    for (int s = 0; s < compiled->setCount; s++)
    {
        if (!compiled->sets[s].isPublic)
        {
            continue;
        }
        char key[512];
        snprintf(key, sizeof(key), "%s|%d|%s", id, group, compiled->sets[s].name);
        MapPut(sets, key, (void*)(uintptr_t)1);
    }
}

// The lowercase letters of every rule text, which the titlecase table
// lists.
static uint8_t s_lowercase[(maxCodePoint + 1) / 8];

bool SpelloutCasesAsIcu(const char* id, const char* text, size_t length)
{
    size_t language = strcspn(id, "_");
    bool dutch = language == 2 && memcmp(id, "nl", 2) == 0;
    bool lithuanian = language == 2 && memcmp(id, "lt", 2) == 0;
    bool armenian = language == 2 && memcmp(id, "hy", 2) == 0;
    bool wordStart = true;
    for (size_t at = 0; at < length;)
    {
        uint32_t cp = 0;
        size_t size = 0;
        if (muniDecodeUtf8(text + at, length - at, &cp, &size) != muni_success ||
            (dutch && wordStart && length - at >= 2 && memcmp(text + at, "ij", 2) == 0) ||
            (lithuanian && cp == 0x0307) || (armenian && cp == 0x0587))
        {
            return false;
        }
        muniGeneralCategory gc = muniGetGeneralCategory(cp);
        wordStart = gc != muni_gcLl && gc != muni_gcLu && gc != muni_gcLt && gc != muni_gcLm &&
                    gc != muni_gcLo && gc != muni_gcMn;
        at += size;
    }
    return true;
}

// Notes the lowercase letters of well-formed text.
static void NoteLetters(const char* text, size_t length)
{
    for (size_t at = 0; at < length;)
    {
        uint32_t cp = 0;
        size_t size = 0;
        if (muniDecodeUtf8(text + at, length - at, &cp, &size) == muni_success &&
            muniGetGeneralCategory(cp) == muni_gcLl)
        {
            s_lowercase[cp / 8] |= (uint8_t)(1u << (cp % 8));
        }
        at += size;
    }
}

// Files the titlecase table: each noted letter whose full titlecase
// differs.
static void AddTitlecase(DataFile* file)
{
    Bytes unit = {0};
    Bytes text = {0};
    uint32_t count = 0;
    Grow(&unit, spelloutTitlecaseHeaderSize);
    for (uint32_t cp = 0; cp <= maxCodePoint; cp++)
    {
        if ((s_lowercase[cp / 8] & (1u << (cp % 8))) == 0)
        {
            continue;
        }
        char letter[4];
        char title[16];
        size_t length = 0;
        size_t needed = 0;
        if (muniEncodeUtf8(cp, letter, &length) != muni_success ||
            muniConvertCase(letter, length, muni_caseTitle, muni_caseDefault, muni_convertStrict,
                            title, sizeof(title), &needed)
                    .status != muni_success)
        {
            GenDie("a titlecase mapping failed", "");
        }
        if (needed == length && memcmp(letter, title, length) == 0)
        {
            continue;
        }
        uint8_t* record = Grow(&unit, spelloutTitlecaseSize);
        GenPut(record, cp, 4);
        GenPut(record + 4, text.size, 2);
        record[6] = (uint8_t)needed;
        record[7] = 0;
        memcpy(Grow(&text, needed), title, needed);
        count++;
    }
    if (count > 0xFFFF || text.size > spelloutMaxText)
    {
        GenDie("the titlecase table passes its offsets", "");
    }
    GenPut(unit.bytes, count, 2);
    GenPut(unit.bytes + 2, text.size, 2);
    Concatenate(&unit, &text);
    DataFileAdd(file, kindSpelloutTitlecase, nullptr, unit.bytes, unit.size);
    free(unit.bytes);
    free(text.bytes);
}

// Compiles and files a file's groups.
static int AddFile(const char* commonDirectory, const Keys* keys, const char* id, DataFile* file,
                   Map* sets, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/rbnf/%s.xml", commonDirectory, id);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    char key[64];
    KeyOf(keys, id, key, sizeof(key));
    const XmlNode* rbnf = XmlFindChild(root, "rbnf");
    for (int i = 0; rbnf != nullptr && i < rbnf->childCount; i++)
    {
        const XmlNode* grouping = rbnf->children[i];
        int group = GroupOf(XmlGetAttribute(grouping, "type"));
        const XmlNode* rules = XmlFindChild(grouping, "rbnfRules");
        if (strcmp(grouping->name, "rulesetGrouping") != 0 || group < 0 || rules == nullptr ||
            XmlGetAttribute(rules, "alt") != nullptr || XmlGetAttribute(rules, "draft") != nullptr)
        {
            snprintf(errorOut, errorCapacity, "%s: an rbnf element the generator does not read",
                     path);
            XmlFree(root);
            return 0;
        }
        if (!SpelloutCasesAsIcu(id, rules->text, rules->textLength))
        {
            snprintf(errorOut, errorCapacity,
                     "%s %s: text the library cannot titlecase as ICU does", path, s_groups[group]);
            XmlFree(root);
            return 0;
        }
        NoteLetters(rules->text, rules->textLength);
        RbnfGroup compiled;
        char error[512];
        if (!RbnfCompile(rules->text, rules->textLength, &compiled, error, sizeof(error)))
        {
            snprintf(errorOut, errorCapacity, "%s %s: %s", path, s_groups[group], error);
            XmlFree(root);
            return 0;
        }
        size_t size = 0;
        uint8_t* payload = SpelloutPayload(&compiled, &size);
        DataFileAdd(file, s_kinds[group], key, payload, size);
        NoteSets(sets, id, group, &compiled);
        free(payload);
        RbnfFree(&compiled);
    }
    XmlFree(root);
    return 1;
}

typedef struct System
{
    char id[16];
    // The locale whose rules it names, "" for the locale asked for.
    char locale[16];
    char name[64];
    int group;
} System;

static int CompareSystems(const void* a, const void* b)
{
    return strcmp(((const System*)a)->id, ((const System*)b)->id);
}

// A system's rules attribute: "roman-upper" (the numbering system rules
// of the locale asked for, which root holds) or "zh/SpelloutRules/
// spellout-cardinal".
static bool ReadSystem(const char* id, const char* rules, System* out)
{
    *out = (System){.group = spelloutGroupNumbering};
    const char* first = strchr(rules, '/');
    const char* last = strrchr(rules, '/');
    if (strlen(id) >= sizeof(out->id))
    {
        return false;
    }
    strcpy(out->id, id);
    if (first != nullptr && last > first)
    {
        size_t length = (size_t)(first - rules);
        char type[64];
        size_t typeLength = (size_t)(last - first - 1);
        if (length >= sizeof(out->locale) || typeLength >= sizeof(type))
        {
            return false;
        }
        memcpy(out->locale, rules, length);
        memcpy(type, first + 1, typeLength);
        type[typeLength] = '\0';
        out->group = GroupOf(type);
        rules = last + 1;
    }
    return out->group >= 0 && strlen(rules) + 1 < sizeof(out->name) &&
           snprintf(out->name, sizeof(out->name), "%%%s", rules) > 0;
}

static int AddSystems(const char* commonDirectory, const Map* sets, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/numberingSystems.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* list = XmlFindChild(root, "numberingSystems");
    System systems[maxSystems];
    int count = 0;
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const XmlNode* node = list->children[i];
        const char* type = XmlGetAttribute(node, "type");
        if (strcmp(node->name, "numberingSystem") != 0 || type == nullptr ||
            strcmp(type, "algorithmic") != 0)
        {
            continue;
        }
        const char* id = XmlGetAttribute(node, "id");
        const char* rules = XmlGetAttribute(node, "rules");
        char key[512];
        if (count == maxSystems || id == nullptr || rules == nullptr ||
            !ReadSystem(id, rules, &systems[count]))
        {
            snprintf(errorOut, errorCapacity, "%s: a numbering system the generator does not read",
                     path);
            XmlFree(root);
            return 0;
        }
        System* s = &systems[count];
        snprintf(key, sizeof(key), "%s|%d|%s", s->locale[0] == '\0' ? "root" : s->locale, s->group,
                 s->name);
        if (MapGet(sets, key) == nullptr)
        {
            snprintf(errorOut, errorCapacity, "%s: %s names no public rule set (%s)", path, id,
                     rules);
            XmlFree(root);
            return 0;
        }
        count++;
    }
    XmlFree(root);
    qsort(systems, (size_t)count, sizeof(System), CompareSystems);
    Bytes text = {0};
    Bytes unit = {0};
    GenPut(Grow(&unit, spelloutSystemHeaderSize), (uint64_t)count, 2);
    for (int i = 0; i < count; i++)
    {
        const char* parts[3] = {systems[i].id, systems[i].locale, systems[i].name};
        uint8_t* record = Grow(&unit, spelloutSystemSize);
        for (int k = 0; k < 3; k++)
        {
            size_t length = strlen(parts[k]);
            GenPut(record + 2 * k, text.size, 2);
            record[6 + k] = (uint8_t)length;
            memcpy(Grow(&text, length), parts[k], length);
        }
        record[9] = (uint8_t)systems[i].group;
    }
    GenPut(unit.bytes + 2, text.size, 2);
    Concatenate(&unit, &text);
    DataFileAdd(file, kindSpelloutSystems, nullptr, unit.bytes, unit.size);
    free(unit.bytes);
    free(text.bytes);
    return 1;
}

int GenerateSpellout(const char* commonDirectory, DataFile* file, char* errorOut,
                     size_t errorCapacity)
{
    Keys keys;
    if (!KeysOpen(&keys, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    int count = 0;
    char** ids = ListFiles(commonDirectory, &count);
    Map sets = {0};
    int ok = 1;
    for (int i = 0; i < count && ok; i++)
    {
        ok = AddFile(commonDirectory, &keys, ids[i], file, &sets, errorOut, errorCapacity);
    }
    ok = ok && AddSystems(commonDirectory, &sets, file, errorOut, errorCapacity);
    if (ok)
    {
        AddTitlecase(file);
    }
    for (int i = 0; i < count; i++)
    {
        free(ids[i]);
    }
    free(ids);
    MapFree(&sets);
    KeysClose(&keys);
    return ok;
}
