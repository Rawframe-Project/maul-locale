// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit name patterns (unit_compound.h), following ICU4C 79's
// number_longnames.cpp step by step: forArbitraryUnit splits the
// normalized unit at "per", processPatternTimes builds each side from
// its single units (prefix, power, then "times"), and the per-unit
// pattern joins them.

#include "unit_compound.h"

#include "data_view.h"
#include "kinds.h"
#include "unit_data.h"
#include "unit_id.h"
#include "unit_names.h"
#include "unit_tables.h"

#include <stdio.h>
#include <string.h>

enum
{
    maxId = unitMaxIdLength + 1,
    categoryOne = 1,
    categoryOther = 5,
};

// Where a unit pattern has its number (ICU's PlaceholderPosition).
typedef enum Placeholder
{
    placeholderEmpty,
    placeholderNone,
    placeholderStart,
    placeholderEnd,
    placeholderMiddle,
} Placeholder;

typedef struct Context
{
    const mlocDataSet* data;
    const mlocLocale* locale;
    UnitTable table;
    uint32_t kind;
    // The long width, whose cores ICU lowercases under a prefix or power.
    bool lower;
} Context;

// The space joining a unit's placeholder and core, as UTF-8.
typedef struct Joiner
{
    char text[4];
    uint8_t length;
} Joiner;

// What a product's units share while it is built.
typedef struct Product
{
    UnitPattern times;
    Placeholder placeholder[unitCategoryCount];
    Joiner joiner;
    uint8_t value0;
    uint8_t value1;
} Product;

// What one single unit brings: its patterns, lowercased ones, a power's
// and a prefix's.
typedef struct Single
{
    UnitForms unit;
    UnitForms lowered;
    UnitForms power;
    UnitPattern prefix;
    bool hasPrefix;
    bool hasPower;
    // The category its patterns are taken in, or unitDerivedCompound for
    // each category's own.
    uint8_t category;
} Single;

static bool Put(UnitPattern* p, const void* text, size_t length)
{
    if (length > unitPatternRoom)
    {
        return false;
    }
    memcpy(p->text, text, length);
    p->length = (uint16_t)length;
    p->present = true;
    return true;
}

static bool Add(UnitPattern* p, const void* text, size_t length)
{
    if (length > (size_t)(unitPatternRoom - p->length))
    {
        return false;
    }
    memcpy(p->text + p->length, text, length);
    p->length = (uint16_t)(p->length + length);
    return true;
}

// Writes a pattern with "{0}" and "{1}" replaced by the arguments (the
// second may be null), as ICU's SimpleFormatter formats.
static bool Format(const UnitPattern* pattern, const UnitPattern* first, const UnitPattern* second,
                   UnitPattern* out)
{
    UnitPattern result = {.present = true};
    size_t literal = 0;
    for (size_t at = 0; at + 3 <= pattern->length; at++)
    {
        const char* t = pattern->text + at;
        const UnitPattern* argument = t[0] != '{' || t[2] != '}' ? nullptr
                                      : t[1] == '0'              ? first
                                      : t[1] == '1'              ? second
                                                                 : nullptr;
        if (argument == nullptr)
        {
            continue;
        }
        if (!Add(&result, pattern->text + literal, at - literal) ||
            !Add(&result, argument->text, argument->length))
        {
            return false;
        }
        literal = at + 3;
        at += 2;
    }
    if (!Add(&result, pattern->text + literal, pattern->length - literal))
    {
        return false;
    }
    *out = result;
    return true;
}

// The pattern of a category: its own when present, else other's.
static const UnitPattern* WithPlural(const UnitForms* forms, uint32_t category)
{
    return forms->form[category].present ? &forms->form[category] : &forms->form[categoryOther];
}

// A code point's length and value at text (valid UTF-8).
static size_t DecodeAt(const char* text, size_t length, uint32_t* out)
{
    const uint8_t* t = (const uint8_t*)text;
    size_t n = t[0] < 0x80 ? 1 : t[0] < 0xE0 ? 2 : t[0] < 0xF0 ? 3 : 4;
    if (n > length)
    {
        *out = 0;
        return 0;
    }
    static const uint8_t s_masks[5] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t c = t[0] & s_masks[n];
    for (size_t i = 1; i < n; i++)
    {
        c = c << 6 | (t[i] & 0x3F);
    }
    *out = c;
    return n;
}

// The length of the code point ending text.
static size_t LastLength(const char* text, size_t length)
{
    size_t n = 1;
    while (n < 4 && n < length && ((uint8_t)text[length - n] & 0xC0) == 0x80)
    {
        n++;
    }
    return n <= length ? n : 0;
}

// Whether a code point is a Java space character (Zs, Zl, Zp), as ICU's
// u_isJavaSpaceChar.
static bool JavaSpace(uint32_t c)
{
    return c == 0x20 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
           c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

// Splits a unit pattern into its core and placeholder, the space joining
// them when the placeholder is at an end (ICU's extractCorePattern).
static Placeholder ExtractCore(const UnitPattern* p, UnitPattern* core, Joiner* joiner)
{
    joiner->length = 0;
    size_t length = p->length;
    uint32_t c = 0;
    if (length >= 3 && memcmp(p->text, "{0}", 3) == 0)
    {
        size_t n = length > 3 ? DecodeAt(p->text + 3, length - 3, &c) : 0;
        size_t skip = n > 0 && JavaSpace(c) ? n : 0;
        memcpy(joiner->text, p->text + 3, skip);
        joiner->length = (uint8_t)skip;
        (void)Put(core, p->text + 3 + skip, length - 3 - skip);
        return placeholderStart;
    }
    if (length >= 3 && memcmp(p->text + length - 3, "{0}", 3) == 0)
    {
        size_t n = length > 3 ? LastLength(p->text, length - 3) : 0;
        if (n > 0)
        {
            (void)DecodeAt(p->text + length - 3 - n, n, &c);
        }
        size_t skip = n > 0 && JavaSpace(c) ? n : 0;
        memcpy(joiner->text, p->text + length - 3 - skip, skip);
        joiner->length = (uint8_t)skip;
        (void)Put(core, p->text, length - 3 - skip);
        return placeholderEnd;
    }
    (void)Put(core, p->text, length);
    for (size_t at = 1; at + 3 < length; at++)
    {
        if (memcmp(p->text + at, "{0}", 3) == 0)
        {
            return placeholderMiddle;
        }
    }
    return placeholderNone;
}

// Loads an identifier's fields of a kind as forms: each category present
// where a key gives it, the per-unit pattern too.
static mlocResult LoadForms(const Context* c, const char* id, size_t length, uint32_t kind,
                            UnitForms* out)
{
    UnitNameFields f;
    if (!mlocGatherUnitNames(c->data, c->locale, kind, id, length, &f))
    {
        return mloc_errorNotFound;
    }
    memset(out, 0, sizeof(*out));
    for (uint32_t k = 0; k < unitCategoryCount; k++)
    {
        uint32_t field = unitNameZero + k;
        if ((f.found & 1u << field) != 0 && !Put(&out->form[k], f.text[field], f.length[field]))
        {
            return mloc_errorCapacity;
        }
    }
    // A name's empty category is other's: the data says only that ICU
    // has it.
    for (uint32_t k = 0; kind != kindUnitNameExtras && k < categoryOther; k++)
    {
        if (out->form[k].present && out->form[k].length == 0)
        {
            out->form[k] = out->form[categoryOther];
            out->form[k].present = true;
        }
    }
    // An empty per pattern is none.
    if (kind != kindUnitNameExtras && (f.found & 1u << unitNamePer) != 0 &&
        f.length[unitNamePer] != 0 && !Put(&out->per, f.text[unitNamePer], f.length[unitNamePer]))
    {
        return mloc_errorCapacity;
    }
    return mloc_success;
}

// Loads a compound key's pattern (its other field) of a kind.
static mlocResult LoadOther(const Context* c, const char* id, uint32_t kind, UnitPattern* out)
{
    UnitForms forms;
    mlocResult r = LoadForms(c, id, strlen(id), kind, &forms);
    if (r == mloc_success && !forms.form[categoryOther].present)
    {
        r = mloc_errorNotFound;
    }
    if (r == mloc_success)
    {
        *out = forms.form[categoryOther];
    }
    return r;
}

// A simple unit's gender in the long width, into out ("" for none).
static void LoadGender(const Context* c, const char* id, size_t length, char* out, size_t room)
{
    UnitNameFields f;
    out[0] = '\0';
    if (mlocGatherUnitNames(c->data, c->locale, kindUnitNameExtras, id, length, &f) &&
        (f.found & 1u << unitExtraGender) != 0 && f.length[unitExtraGender] < room)
    {
        memcpy(out, f.text[unitExtraGender], f.length[unitExtraGender]);
        out[f.length[unitExtraGender]] = '\0';
    }
}

// A power's patterns for a gender: the gender's field where a key gives
// it, else the plain power's (its other for a category it lacks), as the
// data is filed.
static mlocResult LoadPower(const Context* c, int power, const char* gender, UnitForms* out)
{
    char id[32];
    snprintf(id, sizeof(id), "power%d", power);
    mlocResult r = LoadForms(c, id, strlen(id), c->kind, out);
    if (r != mloc_success || gender[0] == '\0')
    {
        return r;
    }
    UnitForms gendered;
    snprintf(id, sizeof(id), "power%d:%s", power, gender);
    if (LoadForms(c, id, strlen(id), c->kind, &gendered) != mloc_success)
    {
        return mloc_success;
    }
    for (uint32_t k = 0; k < unitCategoryCount; k++)
    {
        if (gendered.form[k].present)
        {
            out->form[k] = gendered.form[k];
        }
        else if (!out->form[k].present && out->form[categoryOther].present && k != categoryOther)
        {
            out->form[k] = out->form[categoryOther];
        }
    }
    return mloc_success;
}

// The text of a simple unit and of a prefix, as identifiers write them.
static void SimpleText(const UnitTable* t, uint16_t simple, const char** text, size_t* length)
{
    const uint8_t* s = mlocSimpleUnit(t, simple);
    *text = (const char*)t->text +
            ((uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24);
    *length = s[simpleLength];
}

static void PrefixText(const UnitTable* t, int8_t prefix, const char** text, size_t* length)
{
    const uint8_t* p = mlocUnitPrefix(t, (uint32_t)prefix);
    *text = (const char*)t->text +
            ((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
    *length = p[4];
}

// A prefix's names key ("10p3", "1024p2").
static void PrefixKey(const UnitTable* t, int8_t prefix, char* out, size_t room)
{
    const uint8_t* p = mlocUnitPrefix(t, (uint32_t)prefix);
    if (p[6] != 0)
    {
        snprintf(out, room, "1024p%d", p[6]);
    }
    else
    {
        snprintf(out, room, "10p%d", (int8_t)p[5]);
    }
}

// Loads what one single unit of a product brings.
static mlocResult LoadSingle(const Context* c, const UnitPart* part, Single* s)
{
    const char* text = nullptr;
    size_t length = 0;
    SimpleText(&c->table, part->simple, &text, &length);
    uint8_t kind = 0;
    if (!mlocUnitNameKind(c->data, text, length, &kind) || kind != unitIdUnit)
    {
        return mloc_errorNotFound;
    }
    int power = part->power < 0 ? -part->power : part->power;
    s->hasPower = power != 1;
    s->hasPrefix = part->prefix >= 0;
    mlocResult r = LoadForms(c, text, length, c->kind, &s->unit);
    if (r == mloc_success && s->hasPower)
    {
        char gender[32];
        LoadGender(c, text, length, gender, sizeof(gender));
        r = power > 3 ? mloc_errorNotFound : LoadPower(c, power, gender, &s->power);
    }
    if (r == mloc_success && s->hasPrefix)
    {
        char key[16];
        PrefixKey(&c->table, part->prefix, key, sizeof(key));
        r = LoadOther(c, key, c->kind, &s->prefix);
        UnitPattern lowered;
        // Under a power the prefix is lowercased with the core.
        if (r == mloc_success && c->lower && s->hasPower &&
            LoadOther(c, key, kindUnitNameExtras, &lowered) == mloc_success && lowered.length > 0)
        {
            s->prefix = lowered;
        }
    }
    if (r == mloc_success && c->lower && (s->hasPower || s->hasPrefix) &&
        LoadForms(c, text, length, kindUnitNameExtras, &s->lowered) != mloc_success)
    {
        memset(&s->lowered, 0, sizeof(s->lowered));
    }
    return r;
}

// Builds one category of a product with a single unit (ICU's steps 4.6
// to 4.12).
static mlocResult ApplyCategory(const Context* c, Product* p, const Single* s, uint32_t k,
                                UnitForms* out)
{
    if (!out->form[k].present)
    {
        if (!s->unit.form[k].present)
        {
            return mloc_success;
        }
        out->form[k] = *WithPlural(out, k);
    }
    uint32_t plural = s->category == unitDerivedCompound ? k : s->category;
    const UnitPattern* pattern = WithPlural(&s->unit, plural);
    const UnitPattern* lowered = WithPlural(&s->lowered, plural);
    if (!pattern->present)
    {
        return mloc_errorNotFound;
    }
    bool lower =
        c->lower && (s->hasPrefix || s->hasPower) && lowered->present && lowered->length > 0;
    UnitPattern core;
    Joiner joiner;
    Placeholder position = ExtractCore(lower ? lowered : pattern, &core, &joiner);
    if (position == placeholderMiddle)
    {
        return mloc_errorInvalid;
    }
    if (p->placeholder[k] == placeholderEmpty)
    {
        p->placeholder[k] = position;
        p->joiner = joiner;
    }
    if ((s->hasPrefix && !Format(&s->prefix, &core, nullptr, &core)) ||
        (s->hasPower && !Format(WithPlural(&s->power, plural), &core, nullptr, &core)))
    {
        return mloc_errorCapacity;
    }
    if (out->form[k].length == 0)
    {
        out->form[k] = core;
        return mloc_success;
    }
    return Format(&p->times, &out->form[k], &core, &out->form[k]) ? mloc_success
                                                                  : mloc_errorCapacity;
}

// A structure's category for a component, given the compound's
// (DerivedComponents: compound passes it on, no rule gives none).
static uint8_t Derive(uint8_t value, uint8_t compound)
{
    return value == unitDerivedCompound ? compound
           : value == unitDerivedNone   ? unitDerivedCompound
                                        : value;
}

// A constant denominator through "times" before the first category
// present (ICU's step 5).
static mlocResult PutConstant(const Product* p, uint64_t constant, UnitForms* out)
{
    uint32_t first = 0;
    while (first < unitCategoryCount && !out->form[first].present)
    {
        first++;
    }
    if (constant <= 1 || first == unitCategoryCount)
    {
        return mloc_success;
    }
    UnitPattern number = {.present = true};
    number.length =
        (uint16_t)snprintf(number.text, sizeof(number.text), "%llu", (unsigned long long)constant);
    UnitPattern* form = &out->form[first];
    bool fits = form->length == 0 ? Put(form, number.text, number.length)
                                  : Format(&p->times, &number, form, form);
    return fits ? mloc_success : mloc_errorCapacity;
}

// The constant, then the placeholder put back at its side with the
// joiner (the end of processPatternTimes).
static mlocResult Finish(const Product* p, uint64_t constant, UnitForms* out)
{
    mlocResult r = PutConstant(p, constant, out);
    const Joiner* j = &p->joiner;
    for (uint32_t k = 0; r == mloc_success && k < unitCategoryCount; k++)
    {
        UnitPattern placed = {.present = out->form[k].present};
        bool start = p->placeholder[k] == placeholderStart;
        bool end = p->placeholder[k] == placeholderEnd;
        if (!start && !end)
        {
            continue;
        }
        if (!(start ? Add(&placed, "{0}", 3) && Add(&placed, j->text, j->length) : true) ||
            !Add(&placed, out->form[k].text, out->form[k].length) ||
            !(end ? Add(&placed, j->text, j->length) && Add(&placed, "{0}", 3) : true))
        {
            return mloc_errorCapacity;
        }
        out->form[k] = placed;
    }
    return r;
}

// A product of single units built from their patterns
// (processPatternTimes past a product CLDR names).
static mlocResult BuildTimes(const Context* c, const UnitPart* parts, uint32_t count,
                             uint64_t constant, UnitForms* out)
{
    static const Product s_empty;
    Product p = s_empty;
    mlocResult r = LoadOther(c, "times", c->kind, &p.times);
    if (r != mloc_success)
    {
        return r;
    }
    if (!mlocUnitDerivation(c->data, c->locale, unitDeriveTimes, &p.value0, &p.value1))
    {
        p.value0 = unitDerivedNone;
        p.value1 = unitDerivedNone;
    }
    memset(out, 0, sizeof(*out));
    out->form[categoryOther].present = true;
    uint8_t compound = unitDerivedCompound;
    for (uint32_t i = 0; i < count && r == mloc_success; i++)
    {
        Single s;
        memset(&s, 0, sizeof(s));
        if (i + 1 < count)
        {
            s.category = Derive(p.value0, compound);
            compound = Derive(p.value1, compound);
        }
        else
        {
            s.category = Derive(p.value1, compound);
        }
        r = LoadSingle(c, &parts[i], &s);
        for (uint32_t k = 0; k < unitCategoryCount && r == mloc_success; k++)
        {
            r = ApplyCategory(c, &p, &s, k, out);
        }
    }
    return r == mloc_success ? Finish(&p, constant, out) : r;
}

// Appends text to an identifier being written.
static bool AppendId(char* out, size_t room, size_t* at, const char* text, size_t length)
{
    if (length > room - *at)
    {
        return false;
    }
    memcpy(out + *at, text, length);
    *at += length;
    return true;
}

// Writes a single unit as ICU's appendNeutralIdentifier does.
static bool WritePart(const UnitTable* t, const UnitPart* part, char* out, size_t room, size_t* at)
{
    int power = part->power < 0 ? -part->power : part->power;
    char head[16] = "";
    if (power == 2 || power == 3)
    {
        snprintf(head, sizeof(head), "%s", power == 2 ? "square-" : "cubic-");
    }
    else if (power > 3)
    {
        snprintf(head, sizeof(head), "pow%d-", power);
    }
    const char* text = nullptr;
    size_t length = 0;
    if (!AppendId(out, room, at, head, strlen(head)))
    {
        return false;
    }
    if (part->prefix >= 0)
    {
        PrefixText(t, part->prefix, &text, &length);
        if (!AppendId(out, room, at, text, length))
        {
            return false;
        }
    }
    SimpleText(t, part->simple, &text, &length);
    return AppendId(out, room, at, text, length);
}

// A constant as ICU writes it: its digits, or 1eN for a power of ten past
// 1000.
static size_t ConstantText(uint64_t constant, char* out, size_t room)
{
    int n = snprintf(out, room, "%llu", (unsigned long long)constant);
    size_t zeros = strspn(out + 1, "0");
    if (constant > 1000 && out[0] == '1' && zeros == (size_t)n - 1)
    {
        n = snprintf(out, room, "1e%zu", zeros);
    }
    return (size_t)n;
}

// Writes a normalized product as ICU's MeasureUnitImpl::serialize does:
// "per-" before the first denominator unit, a constant after it (or at
// the end).
static bool WriteProduct(const UnitTable* t, const UnitPart* parts, uint32_t count,
                         uint64_t constant, char* out, size_t room, size_t* lengthOut)
{
    size_t at = 0;
    bool beforePer = true;
    bool constantWritten = false;
    char number[24];
    size_t numberLength = constant > 1 ? ConstantText(constant, number, sizeof(number)) : 0;
    for (uint32_t i = 0; i < count; i++)
    {
        bool firstDenominator = beforePer && parts[i].power < 0;
        beforePer = beforePer && !firstDenominator;
        bool ok = true;
        if (firstDenominator)
        {
            ok = AppendId(out, room, &at, at == 0 ? "per-" : "-per-", at == 0 ? 4 : 5);
            if (ok && numberLength > 0)
            {
                ok = AppendId(out, room, &at, number, numberLength) &&
                     AppendId(out, room, &at, "-", 1);
                constantWritten = true;
            }
        }
        else if (at != 0)
        {
            ok = AppendId(out, room, &at, "-", 1);
        }
        if (!ok || !WritePart(t, &parts[i], out, room, &at))
        {
            return false;
        }
    }
    if (!constantWritten && numberLength > 0 &&
        (!AppendId(out, room, &at, "-per-", 5) || !AppendId(out, room, &at, number, numberLength)))
    {
        return false;
    }
    *lengthOut = at;
    return true;
}

// A prefix's power for ordering (a binary power counts thrice) and its
// base.
static int PrefixOrder(const UnitTable* t, int8_t prefix, int* base)
{
    if (prefix < 0)
    {
        *base = 10;
        return 0;
    }
    const uint8_t* p = mlocUnitPrefix(t, (uint32_t)prefix);
    *base = p[6] != 0 ? 1024 : 10;
    return p[6] != 0 ? 3 * p[6] : (int8_t)p[5];
}

// ICU's SingleUnitImpl::compareTo: the numerator first, then the
// quantity's order, the simple unit's index, the larger prefix.
static int ComparePart(const UnitTable* t, const UnitPart* a, const UnitPart* b)
{
    if ((a->power < 0) != (b->power < 0))
    {
        return a->power < 0 ? 1 : -1;
    }
    int qa = mlocSimpleUnit(t, a->simple)[simpleOrder];
    int qb = mlocSimpleUnit(t, b->simple)[simpleOrder];
    if (qa != qb)
    {
        return qa < qb ? -1 : 1;
    }
    if (a->simple != b->simple)
    {
        return a->simple < b->simple ? -1 : 1;
    }
    int baseA = 0;
    int baseB = 0;
    int pa = PrefixOrder(t, a->prefix, &baseA);
    int pb = PrefixOrder(t, b->prefix, &baseB);
    if (pa != pb)
    {
        return pa < pb ? 1 : -1;
    }
    return baseA == baseB ? 0 : (baseA < baseB ? 1 : -1);
}

// Merges single units of one simple unit, prefix and side, then sorts
// them (ICU's appendSingleUnit and serialize).
static void Normalize(const UnitTable* t, UnitId* id)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < id->count; i++)
    {
        UnitPart part = id->parts[i];
        uint32_t j = 0;
        while (j < count &&
               !(id->parts[j].simple == part.simple && id->parts[j].prefix == part.prefix &&
                 (id->parts[j].power < 0) == (part.power < 0)))
        {
            j++;
        }
        if (j < count)
        {
            id->parts[j].power = (int8_t)(id->parts[j].power + part.power);
            continue;
        }
        id->parts[count++] = part;
    }
    id->count = (uint8_t)count;
    for (uint32_t i = 1; i < count; i++)
    {
        UnitPart part = id->parts[i];
        uint32_t j = i;
        for (; j > 0 && ComparePart(t, &id->parts[j - 1], &part) > 0; j--)
        {
            id->parts[j] = id->parts[j - 1];
        }
        id->parts[j] = part;
    }
}

// ICU's parser knows kilogram as kilo- and gram (measunit_extra.cpp
// leaves it out of its simple units), so its names are built from
// gram's and the prefix's, and it sorts as gram.
static void SplitKilogram(const UnitTable* t, UnitId* id)
{
    int32_t kilogram = mlocFindSimpleUnit(t, "kilogram", 8);
    int32_t gram = mlocFindSimpleUnit(t, "gram", 4);
    int32_t kilo = mlocFindUnitPrefix(t, "kilogram", 8);
    for (uint32_t i = 0; gram >= 0 && kilo >= 0 && i < id->count; i++)
    {
        if (id->parts[i].simple == (uint16_t)kilogram && id->parts[i].prefix < 0)
        {
            id->parts[i].simple = (uint16_t)gram;
            id->parts[i].prefix = (int8_t)kilo;
        }
    }
}

// Parses and normalizes an identifier into a context's table.
static mlocResult ParseUnit(const mlocDataSet* data, const char* unit, size_t length,
                            UnitTable* table, UnitId* id)
{
    DataSpan span;
    if (!mlocFindDataSetUnit(data, kindUnits, nullptr, 0, &span))
    {
        return mloc_errorNotFound;
    }
    *table = mlocReadUnitTable(span);
    mlocResult r = mlocParseUnitId(table, unit, length, id);
    if (r != mloc_success)
    {
        return r;
    }
    if (id->mixed != 0 || id->numeratorConstant != 1)
    {
        return mloc_errorInvalid;
    }
    for (uint32_t i = 0; i < id->count; i++)
    {
        int power = id->parts[i].power < 0 ? -id->parts[i].power : id->parts[i].power;
        if (id->parts[i].currency != 0 || power > 15)
        {
            return mloc_errorInvalid;
        }
    }
    SplitKilogram(table, id);
    Normalize(table, id);
    return mloc_success;
}

mlocResult mlocNormalizeUnitName(const mlocDataSet* data, const char* unit, size_t length,
                                 char* out, size_t capacity, size_t* lengthOut)
{
    UnitTable table;
    UnitId id;
    mlocResult r = ParseUnit(data, unit, length, &table, &id);
    if (r != mloc_success)
    {
        return r;
    }
    return WriteProduct(&table, id.parts, id.count, id.denominatorConstant, out, capacity,
                        lengthOut)
               ? mloc_success
               : mloc_errorCapacity;
}

// A product's forms: a product CLDR names takes its own; nothing gives
// empty patterns, every category present (ICU's untouched arrays).
static mlocResult ProcessTimes(const Context* c, const UnitPart* parts, uint32_t count,
                               uint64_t constant, UnitForms* out)
{
    memset(out, 0, sizeof(*out));
    if (count == 0 && constant <= 1)
    {
        for (uint32_t k = 0; k < unitCategoryCount; k++)
        {
            out->form[k].present = true;
        }
        out->per.present = true;
        return mloc_success;
    }
    char id[maxId];
    size_t length = 0;
    uint8_t kind = 0;
    if (!WriteProduct(&c->table, parts, count, constant, id, sizeof(id), &length))
    {
        return mloc_errorCapacity;
    }
    if (mlocUnitNameKind(c->data, id, length, &kind) && kind == unitIdUnit)
    {
        return LoadForms(c, id, length, c->kind, out);
    }
    return BuildTimes(c, parts, count, constant, out);
}

// The per-unit pattern: the denominator's own, else "per" with the
// denominator's one pattern, its placeholder out and trimmed.
static mlocResult PerPattern(const Context* c, const UnitForms* denominator, UnitPattern* out)
{
    if (denominator->per.present)
    {
        *out = denominator->per;
        return mloc_success;
    }
    UnitPattern per;
    mlocResult r = LoadOther(c, "per", c->kind, &per);
    if (r != mloc_success)
    {
        return r;
    }
    const UnitPattern* one = WithPlural(denominator, categoryOne);
    UnitPattern text = {.present = true};
    UnitPattern empty = {.present = true};
    (void)Format(one, &empty, nullptr, &text);
    size_t start = 0;
    size_t end = text.length;
    uint32_t cp = 0;
    for (size_t n = 0;
         start < end && (n = DecodeAt(text.text + start, end - start, &cp)) > 0 && JavaSpace(cp);)
    {
        start += n;
    }
    for (size_t n = 0; end > start && (n = LastLength(text.text + start, end - start)) > 0 &&
                       DecodeAt(text.text + end - n, n, &cp) > 0 && JavaSpace(cp);)
    {
        end -= n;
    }
    UnitPattern trimmed = {.present = true};
    UnitPattern number = {.present = true};
    (void)Put(&trimmed, text.text + start, end - start);
    (void)Put(&number, "{0}", 3);
    return Format(&per, &number, &trimmed, out) ? mloc_success : mloc_errorCapacity;
}

// A unit CLDR does not name, from its sides (forArbitraryUnit).
static mlocResult BuildArbitrary(const Context* c, const UnitId* id, UnitForms* out)
{
    UnitPart numerator[unitMaxFactors];
    UnitPart denominator[unitMaxFactors];
    uint32_t n = 0;
    uint32_t d = 0;
    for (uint32_t i = 0; i < id->count; i++)
    {
        UnitPart part = id->parts[i];
        if (part.power > 0)
        {
            numerator[n++] = part;
            continue;
        }
        part.power = (int8_t)-part.power;
        denominator[d++] = part;
    }
    UnitForms lead;
    UnitForms trail;
    UnitPattern per;
    mlocResult r = ProcessTimes(c, numerator, n, 1, &lead);
    r = r == mloc_success ? ProcessTimes(c, denominator, d, id->denominatorConstant, &trail) : r;
    r = r == mloc_success ? PerPattern(c, &trail, &per) : r;
    if (r != mloc_success)
    {
        return r;
    }
    memset(out, 0, sizeof(*out));
    for (uint32_t k = 0; k < unitCategoryCount; k++)
    {
        const UnitPattern* form = WithPlural(&lead, k);
        if (!form->present)
        {
            return mloc_errorNotFound;
        }
        bool joined = per.length > 0 && form->length > 0;
        if (!(joined ? Format(&per, form, nullptr, &out->form[k])
                     : Put(&out->form[k], per.length > 0 ? per.text : form->text,
                           per.length > 0 ? per.length : form->length)))
        {
            return mloc_errorCapacity;
        }
    }
    return mloc_success;
}

mlocResult mlocBuildUnitForms(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                              size_t length, uint32_t kind, UnitForms* out)
{
    Context c = {.data = data, .locale = locale, .kind = kind, .lower = kind == kindUnitNamesLong};
    uint8_t idKind = 0;
    if (mlocUnitNameKind(data, unit, length, &idKind) && idKind == unitIdUnit)
    {
        return LoadForms(&c, unit, length, kind, out);
    }
    UnitId id;
    mlocResult r = ParseUnit(data, unit, length, &c.table, &id);
    if (r != mloc_success)
    {
        return r;
    }
    char name[maxId];
    size_t nameLength = 0;
    if (!WriteProduct(&c.table, id.parts, id.count, id.denominatorConstant, name, sizeof(name),
                      &nameLength))
    {
        return mloc_errorCapacity;
    }
    if (mlocUnitNameKind(data, name, nameLength, &idKind) && idKind == unitIdUnit)
    {
        return LoadForms(&c, name, nameLength, kind, out);
    }
    return BuildArbitrary(&c, &id, out);
}
