// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit preferences (UTS #35 Part 6, Unit Preferences): the unit a locale
// prefers for a usage, chosen from the input unit's quantity, the usage,
// the region and the value, and the value split exactly across a mixed
// unit's parts.

#include "unit_prefs.h"

#include "bytes.h"
#include "data_view.h"
#include "decimal.h"
#include "kinds.h"
#include "locale_keyword.h"
#include "locale_prefs.h"
#include "locale_tables.h"
#include "locale_valid.h"
#include "number_precision.h"
#include "unit_convert.h"
#include "unit_data.h"
#include "unit_id.h"
#include "unit_mixed.h"
#include "unit_tables.h"

#include "maul-locale/unit.h"

#include <string.h>

// A unit's exponents of each dimension in its numerator and denominator.
typedef struct Exponents
{
    int32_t numerator[unitMaxDimensions];
    int32_t denominator[unitMaxDimensions];
} Exponents;

// What the choice needs: the tables, the input unit's scale, the value.
typedef struct Choice
{
    const UnitTable* table;
    UnitScale input;
    UnitValue value;
    bool nan;
} Choice;

static bool Is(const char* value, size_t length, const char* text)
{
    return strlen(text) == length && memcmp(value, text, length) == 0;
}

bool mlocValidUsage(const char* usage, size_t length)
{
    size_t part = 0;
    for (size_t i = 0; i <= length; i++)
    {
        if (i == length || usage[i] == '-')
        {
            if (part < 3 || part > 8)
            {
                return false;
            }
            part = 0;
            continue;
        }
        char c = usage[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        {
            return false;
        }
        part++;
    }
    return true;
}

static mlocResult WriteText(const char* text, size_t length, mlocOutput* output)
{
    output->length = length;
    if (length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (length > 0)
    {
        memcpy(output->buffer, text, length);
    }
    return mloc_success;
}

static mlocResult ScaleOf(const UnitTable* t, const char* id, size_t length, UnitId* parsed,
                          UnitScale* scale)
{
    mlocResult r = mlocParseUnitId(t, id, length, parsed);
    return r == mloc_success ? mlocScaleUnit(t, parsed, scale) : r;
}

// The input unit's exponents: each part's base unit raised to its power,
// a part in the denominator swapping its base unit's sides.
static void ExponentsOf(const UnitTable* t, const UnitId* id, Exponents* out)
{
    *out = (Exponents){0};
    for (uint32_t i = 0; i < id->count; i++)
    {
        const UnitPart* part = &id->parts[i];
        const uint8_t* base = mlocBaseUnit(t, mlocSimpleUnit(t, part->simple)[simpleBase]);
        bool down = part->power < 0;
        int32_t power = down ? -part->power : part->power;
        for (uint32_t d = 0; d < unitMaxDimensions; d++)
        {
            out->numerator[d] += power * base[(down ? baseDenominator : baseNumerator) + d];
            out->denominator[d] += power * base[(down ? baseNumerator : baseDenominator) + d];
        }
    }
}

static bool QuantityMatches(const uint8_t* q, const int32_t* numerator, const int32_t* denominator)
{
    for (uint32_t d = 0; d < unitMaxDimensions; d++)
    {
        if (q[quantityNumerator + d] != numerator[d] ||
            q[quantityDenominator + d] != denominator[d])
        {
            return false;
        }
    }
    return true;
}

static int32_t FindQuantity(const UnitTable* t, const int32_t* numerator,
                            const int32_t* denominator)
{
    for (uint32_t i = 0; i < t->quantityCount; i++)
    {
        if (QuantityMatches(t->quantities + (size_t)i * unitQuantitySize, numerator, denominator))
        {
            return (int32_t)i;
        }
    }
    return -1;
}

// Cancels what the two sides share.
static Exponents Simplified(const Exponents* e)
{
    Exponents s = {0};
    for (uint32_t d = 0; d < unitMaxDimensions; d++)
    {
        int32_t net = e->numerator[d] - e->denominator[d];
        s.numerator[d] = net > 0 ? net : 0;
        s.denominator[d] = net < 0 ? -net : 0;
    }
    return s;
}

// The quantity of exponents as ICU's getUnitQuantity tries them: as they
// are, reciprocal, simplified, simplified and reciprocal; -1 for none.
static int32_t QuantityOf(const UnitTable* t, const Exponents* e)
{
    Exponents s = Simplified(e);
    int32_t q = FindQuantity(t, e->numerator, e->denominator);
    q = q >= 0 ? q : FindQuantity(t, e->denominator, e->numerator);
    q = q >= 0 ? q : FindQuantity(t, s.numerator, s.denominator);
    return q >= 0 ? q : FindQuantity(t, s.denominator, s.numerator);
}

// Writes a dimension's power as an identifier's prefix ("square-",
// "pow4-"), giving its length; 0 for a power the syntax lacks.
static size_t PowerPrefix(int32_t power, char* out)
{
    if (power == 1)
    {
        return 0;
    }
    if (power == 2 || power == 3)
    {
        const char* text = power == 2 ? "square-" : "cubic-";
        size_t n = strlen(text);
        memcpy(out, text, n + 1);
        return n;
    }
    size_t n = 0;
    out[n++] = 'p';
    out[n++] = 'o';
    out[n++] = 'w';
    if (power >= 10)
    {
        out[n++] = (char)('0' + power / 10);
    }
    out[n++] = (char)('0' + power % 10);
    out[n++] = '-';
    return n;
}

// Appends a side of base units to an identifier, in the dimensions'
// normalization order, each with its power; false when it does not fit
// or a power passes 15.
static bool AppendSide(const UnitTable* t, const int32_t* exponents, char* out, size_t* at)
{
    for (uint32_t place = 0; place < 256; place++)
    {
        for (uint32_t d = 0; d < t->dimensionCount; d++)
        {
            const uint8_t* dim = t->dimensions + (size_t)d * unitDimensionSize;
            if (dim[5] != place || exponents[d] == 0)
            {
                continue;
            }
            char prefix[8];
            size_t p = exponents[d] > 15 ? 0 : PowerPrefix(exponents[d], prefix);
            size_t length = dim[4];
            if (exponents[d] > 15 || *at + 1 + p + length >= unitMaxBaseId)
            {
                return false;
            }
            if (*at > 0 && out[*at - 1] != '-')
            {
                out[(*at)++] = '-';
            }
            memcpy(out + *at, prefix, p);
            memcpy(out + *at + p, t->text + mlocLoad32(dim), length);
            *at += p + length;
        }
    }
    return true;
}

// The identifier of exponents' base units, cancelled ("candela-per-bit");
// its length, 0 when it has no dimension or cannot be written.
static size_t BaseIdentifier(const UnitTable* t, const Exponents* e, char* out)
{
    Exponents s = Simplified(e);
    size_t at = 0;
    bool any = false;
    for (uint32_t d = 0; d < unitMaxDimensions; d++)
    {
        any |= s.denominator[d] != 0;
    }
    if (!AppendSide(t, s.numerator, out, &at))
    {
        return 0;
    }
    if (any)
    {
        if (at + 5 >= unitMaxBaseId)
        {
            return 0;
        }
        memcpy(out + at, at > 0 ? "-per-" : "per-", at > 0 ? 5 : 4);
        at += at > 0 ? 5 : 4;
        if (!AppendSide(t, s.denominator, out, &at))
        {
            return 0;
        }
    }
    return at;
}

// Two systems' intersection, except that SI with SI-acceptable gives
// SI-acceptable and metric with metric-adjacent gives metric-adjacent
// (UTS #35 Part 6, Derived Unit System).
static uint32_t Intersect(uint32_t a, uint32_t b)
{
    uint32_t kept = a & b;
    uint32_t si = unitSystemSi | unitSystemSiAcceptable;
    uint32_t metric = unitSystemMetric | unitSystemMetricAdjacent;
    if ((a & si) != 0 && (b & si) != 0 && ((a | b) & si) == si)
    {
        kept |= unitSystemSiAcceptable;
    }
    if ((a & metric) != 0 && (b & metric) != 0 && ((a | b) & metric) == metric)
    {
        kept |= unitSystemMetricAdjacent;
    }
    return kept;
}

// A parsed unit's systems, its parts' intersected; prefixes do not count.
static uint32_t SystemsOf(const UnitTable* t, const UnitId* id)
{
    uint32_t systems = 0;
    for (uint32_t i = 0; i < id->count; i++)
    {
        uint32_t s = id->parts[i].currency
                         ? 0
                         : mlocLoad16(mlocSimpleUnit(t, id->parts[i].simple) + simpleSystems);
        s &= ~(uint32_t)unitSystemPrefixable;
        systems = i == 0 ? s : Intersect(systems, s);
    }
    return systems;
}

// The -u-mu unit's identifier, or null.
static const char* KeyUnit(const mlocLocale* locale)
{
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "mu", &value);
    return Is(value, length, "celsius")    ? "celsius"
           : Is(value, length, "kelvin")   ? "kelvin"
           : Is(value, length, "fahrenhe") ? "fahrenheit"
                                           : nullptr;
}

// The -u-ms key's systems and region, or 0.
static uint32_t KeySystems(const mlocLocale* locale, uint32_t* regionOut)
{
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "ms", &value);
    if (Is(value, length, "metric"))
    {
        *regionOut = mlocPackRegion("001", 3);
        return unitSystemMetric | unitSystemMetricAdjacent;
    }
    if (Is(value, length, "ussystem") || Is(value, length, "uksystem"))
    {
        bool us = value[1] == 's';
        *regionOut = mlocPackRegion(us ? "US" : "GB", 2);
        return us ? unitSystemUs : unitSystemUk;
    }
    return 0;
}

// Whether every entry of a run is of the systems.
static mlocResult RunMatches(const UnitTable* t, const UnitPrefsTable* p, uint32_t run,
                             uint32_t systems, bool* matchesOut)
{
    const uint8_t* entry = nullptr;
    uint32_t count = mlocUnitPrefEntries(p, run, &entry);
    *matchesOut = true;
    for (uint32_t i = 0; i < count; i++, entry += unitEntrySize)
    {
        UnitId id;
        if (mlocParseUnitId(t, (const char*)p->text + mlocLoad32(entry), entry[4], &id) !=
            mloc_success)
        {
            return mloc_errorFormat;
        }
        *matchesOut &= (SystemsOf(t, &id) & systems) != 0;
    }
    return mloc_success;
}

// The run of a set for the region, moved to the -u-ms key's region when
// a unit of the region's is of another system.
static mlocResult ChooseRun(const UnitTable* t, const UnitPrefsTable* p, uint32_t set,
                            const mlocLocale* locale, const mlocDataSet* data, uint32_t* runOut)
{
    uint32_t language = 0;
    uint32_t region = mlocPreferenceRegion(data, locale, &language);
    uint32_t fallback = 0;
    uint32_t systems = KeySystems(locale, &fallback);
    uint32_t run = mlocFindUnitPrefRun(p, set, region);
    if (systems != 0 && fallback != region)
    {
        bool matches = true;
        mlocResult r = RunMatches(t, p, run, systems, &matches);
        if (r != mloc_success)
        {
            return r;
        }
        run = matches ? run : mlocFindUnitPrefRun(p, set, fallback);
    }
    *runOut = run;
    return mloc_success;
}

// The first output whose threshold the value reaches in its unit (a
// mixed unit's largest part); NaN reaches every one.
static mlocResult ChooseEntry(const Choice* c, const UnitRoute* route,
                              const UnitRouteEntry** entryOut)
{
    for (uint32_t i = 0; i < route->count; i++)
    {
        const UnitRouteEntry* entry = &route->entries[i];
        UnitId id;
        if (mlocParseUnitId(c->table, entry->id, entry->length, &id) != mloc_success)
        {
            return mloc_errorFormat;
        }
        bool reaches = false;
        mlocResult r = mlocReachesThreshold(c->table, &c->input, &c->value, &id, entry->threshold,
                                            entry->places, &reaches);
        if (r != mloc_success)
        {
            return r;
        }
        if (c->nan || i + 1 == route->count || reaches)
        {
            *entryOut = entry;
            return mloc_success;
        }
    }
    return mloc_errorFormat;
}

const mlocPrecision* mlocRoutePrecision(const mlocPrecision* given, uint32_t increment,
                                        mlocPrecision* own)
{
    if (given == nullptr || given->kind != mloc_precisionNone)
    {
        return given;
    }
    if (increment > 0)
    {
        *own = (mlocPrecision){
            .kind = mloc_precisionFraction, .mode = given->mode, .increment = (uint16_t)increment};
    }
    else
    {
        *own = (mlocPrecision){.kind = mloc_precisionMorePrecision,
                               .mode = given->mode,
                               .minSignificantDigits = 1,
                               .maxSignificantDigits = 2};
    }
    return own;
}

static mlocResult DecimalOf(const UnitValue* v, const mlocPrecision* precision, mlocDecimal* out)
{
    bool exact = true;
    mlocResult r = mlocUnitValueDecimal(v, out, &exact);
    return r == mloc_success ? mlocRoundConverted(out, exact, precision) : r;
}

// Rounds a mixed unit's last amount by the precision.
static mlocResult RoundLast(void* context, mlocDecimal* amount, bool exact)
{
    return mlocRoundConverted(amount, exact, context);
}

// The amounts of a chosen unit.
static mlocResult AmountsIn(const Choice* c, const UnitId* id, const mlocPrecision* precision,
                            mlocUnitAmounts* out)
{
    if (id->mixed)
    {
        return mlocSplitMixed(c->table, id, &c->input, &c->value, c->nan, RoundLast,
                              (void*)precision, out);
    }
    UnitScale scale = {0};
    UnitValue v;
    if (mlocScaleUnit(c->table, id, &scale) != mloc_success)
    {
        return mloc_errorFormat;
    }
    if (!mlocUnitValueIn(c->table, &c->input, &c->value, &scale, &v))
    {
        return mloc_errorLimit;
    }
    *out = (mlocUnitAmounts){.count = 1};
    if (c->nan)
    {
        out->amounts[0] = (mlocDecimal){.kind = mloc_decimalNaN};
        return mloc_success;
    }
    return DecimalOf(&v, precision, &out->amounts[0]);
}

// The unit chosen without preferences: a quantity's base unit, or the
// input's base units.
static mlocResult ChooseBase(const UnitTable* t, int32_t quantity, const Exponents* e, char* idOut,
                             size_t* lengthOut)
{
    if (quantity >= 0)
    {
        const uint8_t* q = t->quantities + (size_t)quantity * unitQuantitySize;
        memcpy(idOut, t->text + mlocLoad32(q + 4), q[9]);
        *lengthOut = q[9];
        return mloc_success;
    }
    *lengthOut = BaseIdentifier(t, e, idOut);
    return *lengthOut > 0 ? mloc_success : mloc_errorInvalid;
}

// A route of one output.
static mlocResult RouteOne(UnitRoute* out, const char* id, size_t length)
{
    out->count = 1;
    out->entries[0] = (UnitRouteEntry){.id = id, .length = (uint8_t)length};
    return mloc_success;
}

// The route's base output: a quantity's base unit, or the input's base
// units.
static mlocResult RouteBase(const UnitTable* t, int32_t quantity, const Exponents* e,
                            UnitRoute* out)
{
    size_t length = 0;
    mlocResult r = ChooseBase(t, quantity, e, out->base, &length);
    return r == mloc_success ? RouteOne(out, out->base, length) : r;
}

// The set of a quantity's preferences for the usage, then each shorter by
// its last part, then "default".
static int32_t FindSet(const UnitPrefsTable* p, const char* category, uint8_t length,
                       const char* usage, size_t usageLength)
{
    int32_t set = -1;
    for (size_t n = usageLength; set < 0; n = n == 0 ? 0 : n - 1)
    {
        while (n > 0 && n < usageLength && usage[n] != '-')
        {
            n--;
        }
        set = n == 0 ? mlocFindUnitPrefSet(p, category, length, "default", 7)
                     : mlocFindUnitPrefSet(p, category, length, usage, n);
        if (n == 0)
        {
            break;
        }
    }
    return set;
}

mlocResult mlocRouteUnit(const mlocDataSet* data, const mlocLocale* locale, const UnitTable* t,
                         const UnitId* input, const UnitScale* inputScale, const char* usage,
                         size_t usageLength, UnitRoute* out)
{
    const char* key = KeyUnit(locale);
    UnitId keyId;
    UnitScale keyScale = {0};
    bool reciprocal = false;
    *out = (UnitRoute){0};
    if (key != nullptr && ScaleOf(t, key, strlen(key), &keyId, &keyScale) == mloc_success &&
        mlocSameDimensions(inputScale, &keyScale, &reciprocal) && !reciprocal)
    {
        return RouteOne(out, key, strlen(key));
    }
    Exponents e;
    ExponentsOf(t, input, &e);
    int32_t quantity = QuantityOf(t, &e);
    DataSpan unit;
    if (quantity < 0)
    {
        return RouteBase(t, quantity, &e, out);
    }
    if (!mlocFindDataSetUnit(data, kindUnitPreferences, nullptr, 0, &unit))
    {
        return mloc_errorNotFound;
    }
    UnitPrefsTable p = mlocReadUnitPrefs(unit);
    const uint8_t* q = t->quantities + (size_t)quantity * unitQuantitySize;
    const char* category = (const char*)t->text + mlocLoad32(q);
    if (mlocFindUnitPrefSet(&p, category, q[8], nullptr, 0) < 0)
    {
        return RouteBase(t, quantity, &e, out);
    }
    int32_t set = FindSet(&p, category, q[8], usage, usageLength);
    uint32_t run = 0;
    mlocResult r = set < 0 ? mloc_errorFormat : ChooseRun(t, &p, (uint32_t)set, locale, data, &run);
    if (r != mloc_success)
    {
        return r;
    }
    const uint8_t* entry = nullptr;
    uint32_t count = mlocUnitPrefEntries(&p, run, &entry);
    if (count == 0 || count > unitMaxOutputs)
    {
        return mloc_errorFormat;
    }
    for (uint32_t i = 0; i < count; i++, entry += unitEntrySize)
    {
        out->entries[i] = (UnitRouteEntry){
            .id = (const char*)p.text + mlocLoad32(entry),
            .threshold = mlocLoad32(entry + 8),
            .increment = mlocLoad16(entry + 6),
            .length = entry[4],
            .places = entry[5],
        };
    }
    out->count = count;
    return mloc_success;
}

mlocResult mlocGetUnitPreference(const mlocDataSet* data, const mlocLocale* locale,
                                 const char* unit, size_t unitLength, const char* usage,
                                 size_t usageLength, const mlocDecimal* value,
                                 const mlocPrecision* precision, mlocOutput* unitOut,
                                 mlocUnitAmounts* amountsOut)
{
    if (data == nullptr || !mlocLocaleValid(locale) || unit == nullptr || value == nullptr ||
        unitOut == nullptr || amountsOut == nullptr || (usage == nullptr && usageLength > 0) ||
        (unitOut->buffer == nullptr && unitOut->capacity > 0) || !mlocValidDecimal(value) ||
        (precision != nullptr && !mlocValidPrecision(precision)))
    {
        return mloc_errorInvalid;
    }
    if (usageLength > unitMaxUsage || (usageLength > 0 && !mlocValidUsage(usage, usageLength)))
    {
        return mloc_errorSyntax;
    }
    DataSpan table;
    if (!mlocFindDataSetUnit(data, kindUnits, nullptr, 0, &table))
    {
        return mloc_errorNotFound;
    }
    UnitTable t = mlocReadUnitTable(table);
    Choice c = {.table = &t, .nan = value->kind == mloc_decimalNaN};
    UnitId input;
    mlocResult r = ScaleOf(&t, unit, unitLength, &input, &c.input);
    if (r != mloc_success)
    {
        return r;
    }
    // NaN chooses as the largest value would.
    mlocDecimal chosenValue = *value;
    chosenValue.kind = c.nan ? mloc_decimalInfinity : value->kind;
    if (!mlocUnitValueOf(&chosenValue, &c.value))
    {
        return mloc_errorLimit;
    }
    static UnitRoute s_zero;
    UnitRoute route = s_zero;
    const UnitRouteEntry* entry = nullptr;
    r = mlocRouteUnit(data, locale, &t, &input, &c.input, usage, usageLength, &route);
    r = r == mloc_success ? ChooseEntry(&c, &route, &entry) : r;
    UnitId chosen;
    mlocUnitAmounts amounts;
    mlocPrecision own;
    if (r == mloc_success)
    {
        r = mlocParseUnitId(&t, entry->id, entry->length, &chosen) == mloc_success
                ? mloc_success
                : mloc_errorFormat;
    }
    if (r == mloc_success)
    {
        r = AmountsIn(&c, &chosen, mlocRoutePrecision(precision, entry->increment, &own), &amounts);
    }
    if (r == mloc_success)
    {
        r = WriteText(entry->id, entry->length, unitOut);
    }
    if (r == mloc_success)
    {
        amounts.increment = entry->increment;
        *amountsOut = amounts;
    }
    return r;
}
