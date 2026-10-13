// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The currency matcher (parse_currency.h), ICU4C 79's
// CombinedCurrencyMatcher: the currency's symbol as written, its code in
// any case, then its long names case folded; for any currency,
// uprv_parseCurrency over the chain's currencies: the longest of their
// names uppercased as the input is, or of their symbols (with root's
// equivalent signs) and codes as written, a name winning a tie.

#include "parse_currency.h"

#include "bytes.h"
#include "currency_tables.h"
#include "number_data.h"
#include "parse_tables.h"

#include <string.h>

enum
{
    // The UTF-16 units of the input uprv_parseCurrency reads.
    maxInputUnits = 100,
};

// The currency spacing root inserts (currencySpacing's insertBetween),
// the only one CLDR gives.
static const uint8_t s_nbsp[2] = {0xC2, 0xA0};

static bool SeenNumber(const Parsed* p)
{
    return p->quantity.present || (p->flags & (parsedNaN | parsedInfinity)) != 0;
}

static void Take(Segment* s, Parsed* p, size_t bytes, const char* code)
{
    s->at += bytes;
    p->charEnd = s->at;
    memcpy(p->code, code, 3);
    p->hasCode = true;
}

// Whether the segment begins with the bytes of a text.
static bool Begins(const Segment* s, const uint8_t* text, size_t length)
{
    return length > 0 && s->length - s->at >= length && memcmp(s->text + s->at, text, length) == 0;
}

// The UTF-16 units of UTF-8 text, a byte that does not begin a code point
// one (as its replacement character).
static uint32_t UnitsOf(const uint8_t* text, size_t length)
{
    uint32_t units = 0;
    for (size_t at = 0; at < length;)
    {
        size_t size = 0;
        uint32_t cp = mlocParseCodePoint(text, length, at, &size);
        units += cp != parseNoCodePoint && cp >= 0x10000 ? 2 : 1;
        at += size;
    }
    return units;
}

// The bytes of UTF-8 text that hold a count of UTF-16 units.
static size_t BytesOf(const uint8_t* text, size_t length, uint32_t units)
{
    size_t at = 0;
    while (at < length && units > 0)
    {
        size_t size = 0;
        uint32_t cp = mlocParseCodePoint(text, length, at, &size);
        uint32_t n = cp != parseNoCodePoint && cp >= 0x10000 ? 2 : 1;
        units = units > n ? units - n : 0;
        at += size;
    }
    return at;
}

// UTF-16 units of uppercased text, at most a limit.
typedef struct Units
{
    uint16_t unit[maxInputUnits];
    uint32_t count;
} Units;

static void Append(Units* out, uint32_t cp, uint32_t limit)
{
    if (cp >= 0x10000 && cp != parseNoCodePoint)
    {
        if (out->count + 2 <= limit)
        {
            out->unit[out->count++] = (uint16_t)(0xD7C0 + (cp >> 10));
            out->unit[out->count++] = (uint16_t)(0xDC00 | (cp & 0x3FF));
        }
        else
        {
            out->count = limit;
        }
        return;
    }
    if (out->count < limit)
    {
        out->unit[out->count++] = cp == parseNoCodePoint ? 0xFFFD : (uint16_t)cp;
    }
}

// Uppercases text as ICU's u_strToUpper does for the locale (the Turkic
// way for tr and az), into at most a limit of units.
static void Upper(const uint8_t* sets, bool turkic, const uint8_t* text, size_t length,
                  uint32_t limit, Units* out)
{
    out->count = 0;
    for (size_t at = 0; at < length && out->count < limit;)
    {
        size_t size = 0;
        uint32_t cp = mlocParseCodePoint(text, length, at, &size);
        at += size;
        uint32_t upper[3] = {cp};
        int n = cp == parseNoCodePoint ? 1 : mlocParseUpper(sets, cp, upper);
        upper[0] = turkic && cp == 'i' ? 0x130 : upper[0];
        for (int i = 0; i < n; i++)
        {
            Append(out, upper[i], limit);
        }
    }
}

// The best matches of uprv_parseCurrency, each the first of the longest
// in its list.
typedef struct Search
{
    const uint8_t* sets;
    bool turkic;
    const uint8_t* input;
    size_t inputLength;
    uint32_t inputUnits;
    Units upper;
    uint32_t nameUnits;
    char nameCode[3];
    uint32_t symbolUnits;
    char symbolCode[3];
    // Whether the input may begin with a currency sign class's member, so
    // that the equivalent signs are worth trying.
    bool equivalents;
} Search;

// Whether a code point's uppercase continues the input's uppercase at a
// unit, moving the unit on; the name is cut at its own length.
static bool UpperMatches(const Search* q, uint32_t cp, uint32_t* at, uint32_t units)
{
    if (cp < 0x80 && !(q->turkic && cp == 'i'))
    {
        // ASCII uppercases by itself.
        uint32_t upper = cp >= 'a' && cp <= 'z' ? cp - 32 : cp;
        if (*at >= units)
        {
            return true;
        }
        uint32_t unit = q->upper.unit[*at];
        (*at)++;
        return upper == unit;
    }
    uint32_t upper[3] = {cp};
    int n = cp == parseNoCodePoint ? 1 : mlocParseUpper(q->sets, cp, upper);
    upper[0] = q->turkic && cp == 'i' ? 0x130 : upper[0];
    for (int i = 0; i < n && *at < units; i++)
    {
        Units one = {.count = 0};
        Append(&one, upper[i], 2);
        for (uint32_t u = 0; u < one.count && *at < units; u++)
        {
            if (one.unit[u] != q->upper.unit[(*at)++])
            {
                return false;
            }
        }
    }
    return true;
}

// A name: uppercased and cut to its own length in units, as ICU stores
// it, compared unit by unit as it is uppercased.
static void TryName(Search* q, const uint8_t* text, size_t length, const char* code)
{
    // Most names differ at the first character.
    uint32_t first = 0;
    size_t firstSize = 0;
    uint32_t lead = length > 0 ? mlocParseCodePoint(text, length, 0, &firstSize) : parseNoCodePoint;
    if (length == 0 || q->upper.count == 0 || !UpperMatches(q, lead, &first, 1))
    {
        return;
    }
    uint32_t units = UnitsOf(text, length);
    if (units <= q->nameUnits || units > q->upper.count)
    {
        return;
    }
    uint32_t at = 0;
    for (size_t i = 0; i < length && at < units;)
    {
        size_t size = 0;
        uint32_t cp = mlocParseCodePoint(text, length, i, &size);
        i += size;
        if (!UpperMatches(q, cp, &at, units))
        {
            return;
        }
    }
    if (at == units)
    {
        q->nameUnits = units;
        memcpy(q->nameCode, code, 3);
    }
}

// A symbol or a code, as written.
static void TrySymbol(Search* q, const uint8_t* text, size_t length, const char* code)
{
    if (length == 0 || length > q->inputLength || text[0] != q->input[0])
    {
        return;
    }
    uint32_t units = UnitsOf(text, length);
    if (units > q->symbolUnits && units <= q->inputUnits && length <= q->inputLength &&
        memcmp(text, q->input, length) == 0)
    {
        q->symbolUnits = units;
        memcpy(q->symbolCode, code, 3);
    }
}

// The code point of a class range as UTF-8.
static size_t Encode(uint32_t cp, uint8_t* out)
{
    if (cp < 0x80)
    {
        out[0] = (uint8_t)cp;
        return 1;
    }
    if (cp < 0x800)
    {
        out[0] = (uint8_t)(0xC0 | cp >> 6);
        out[1] = (uint8_t)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000)
    {
        out[0] = (uint8_t)(0xE0 | cp >> 12);
        out[1] = (uint8_t)(0x80 | (cp >> 6 & 0x3F));
        out[2] = (uint8_t)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (uint8_t)(0xF0 | cp >> 18);
    out[1] = (uint8_t)(0x80 | (cp >> 12 & 0x3F));
    out[2] = (uint8_t)(0x80 | (cp >> 6 & 0x3F));
    out[3] = (uint8_t)(0x80 | (cp & 0x3F));
    return 4;
}

// Whether text that begins with a code point and a byte may begin with a
// member of a currency sign class.
static bool MayBeSign(const uint8_t* sets, uint32_t lead, uint8_t first)
{
    for (uint32_t set = parseDollar; set <= parseRupee; set++)
    {
        ParseClassView c = mlocParseClass(sets, set);
        bool string = false;
        for (uint32_t i = 0; i < c.stringCount && !string; i++)
        {
            size_t n = 0;
            string = mlocParseClassString(&c, i, &n)[0] == first;
        }
        if (string || mlocParseHas(sets, set, lead))
        {
            return true;
        }
    }
    return false;
}

// The signs root makes equivalent to a symbol (ICU's
// getCurrSymbolsEquiv): the other members of the currency class that
// holds it.
static void TryEquivalents(Search* q, const uint8_t* symbol, size_t length, const char* code)
{
    // A class holds code points and two-letter strings.
    for (uint32_t set = parseDollar; length <= 4 && set <= parseRupee; set++)
    {
        if (!mlocParseHoldsSymbol(q->sets, set, symbol, length))
        {
            continue;
        }
        ParseClassView c = mlocParseClass(q->sets, set);
        for (uint32_t i = 0; i < c.rangeCount; i++)
        {
            for (uint32_t cp = mlocLoad32(c.ranges + (size_t)i * parseRangeSize);
                 cp <= mlocLoad32(c.ranges + (size_t)i * parseRangeSize + 4); cp++)
            {
                uint8_t bytes[4];
                size_t n = Encode(cp, bytes);
                if (n != length || memcmp(bytes, symbol, n) != 0)
                {
                    TrySymbol(q, bytes, n, code);
                }
            }
        }
        for (uint32_t i = 0; i < c.stringCount; i++)
        {
            size_t n = 0;
            const uint8_t* string = mlocParseClassString(&c, i, &n);
            if (n != length || memcmp(string, symbol, n) != 0)
            {
                TrySymbol(q, string, n, code);
            }
        }
    }
}

// A string field of a row; null when the row does not give it.
static const uint8_t* RowField(const SparseTable* t, const uint8_t* row, uint32_t field,
                               size_t* lengthOut)
{
    uint16_t value = 0;
    if (row == nullptr || !mlocSparseField(row, field, &value))
    {
        return nullptr;
    }
    uint16_t start = mlocLoad16(t->offsets + (size_t)value * 2);
    *lengthOut = (size_t)(mlocLoad16(t->offsets + (size_t)value * 2 + 2) - start);
    return t->text + start;
}

// A field of a code from the first of a kind's units, from a step on,
// that gives it; null when none does.
static const uint8_t* Resolve(const DataSpan* units, const uint8_t* steps, uint32_t count,
                              uint32_t step, uint16_t code, uint32_t field, size_t* lengthOut)
{
    for (uint32_t i = 0; i < count; i++)
    {
        SparseTable t = mlocReadSparseTable(units[i]);
        const uint8_t* text =
            steps[i] < step ? nullptr : RowField(&t, mlocFindSparseRow(&t, code), field, lengthOut);
        if (text != nullptr)
        {
            return text;
        }
    }
    return nullptr;
}

// A unit of a step, walked in code order.
typedef struct Cursor
{
    SparseTable t;
    uint32_t index;
    bool names;
} Cursor;

typedef struct Step
{
    Cursor cursor[2 * currencyChainUnits];
    uint32_t count;
} Step;

static void OpenStep(const CurrencyChain* c, uint32_t step, Step* out)
{
    out->count = 0;
    for (uint32_t i = 0; i < c->symbolCount; i++)
    {
        if (c->symbolSteps[i] == step)
        {
            out->cursor[out->count++] = (Cursor){mlocReadSparseTable(c->symbols[i]), 0, false};
        }
    }
    for (uint32_t i = 0; i < c->nameCount; i++)
    {
        if (c->nameSteps[i] == step)
        {
            out->cursor[out->count++] = (Cursor){mlocReadSparseTable(c->names[i]), 0, true};
        }
    }
}

static uint16_t CodeAt(const Cursor* k)
{
    return k->index < k->t.entryCount ? mlocLoad16(k->t.codes + (size_t)k->index * 2) : 0;
}

// The least code the step's cursors are at, 0 when they are done.
static uint16_t Least(const Step* st)
{
    uint16_t least = 0;
    for (uint32_t i = 0; i < st->count; i++)
    {
        uint16_t code = CodeAt(&st->cursor[i]);
        least = code != 0 && (least == 0 || code < least) ? code : least;
    }
    return least;
}

static const uint8_t* RowAt(const Cursor* k, uint16_t code)
{
    return CodeAt(k) == code ? k->t.rows + mlocLoad16(k->t.rowOffsets + (size_t)k->index * 2)
                             : nullptr;
}

static void Advance(Step* st, uint16_t code)
{
    for (uint32_t i = 0; i < st->count; i++)
    {
        st->cursor[i].index += CodeAt(&st->cursor[i]) == code ? 1 : 0;
    }
}

// A currency's fields a step's own units give: its symbol, its name, and
// the names units' rows for its plural names.
typedef struct Given
{
    const uint8_t* symbol;
    size_t symbolLength;
    const uint8_t* name;
    size_t nameLength;
    const Cursor* plurals[currencyChainUnits];
    const uint8_t* pluralRows[currencyChainUnits];
    uint32_t pluralCount;
} Given;

static void GivenAt(const Step* st, uint16_t code, Given* out)
{
    *out = (Given){.symbol = nullptr};
    for (uint32_t i = 0; i < st->count; i++)
    {
        const Cursor* k = &st->cursor[i];
        const uint8_t* row = RowAt(k, code);
        if (row == nullptr)
        {
            continue;
        }
        if (!k->names)
        {
            out->symbol = out->symbol != nullptr
                              ? out->symbol
                              : RowField(&k->t, row, currencySymbol, &out->symbolLength);
            continue;
        }
        out->name =
            out->name != nullptr ? out->name : RowField(&k->t, row, currencyName, &out->nameLength);
        if ((row[0] & ~1u) != 0 && out->pluralCount < currencyChainUnits)
        {
            out->plurals[out->pluralCount] = k;
            out->pluralRows[out->pluralCount++] = row;
        }
    }
}

// A currency of ICU's Currencies table at a step: its symbol resolved
// from the step (the code without one), the equivalent signs and the
// code; its name resolved the same way.
static void TryListed(Search* q, const CurrencyChain* c, uint32_t step, uint16_t packed,
                      const Given* g)
{
    char code[3];
    mlocUnpackCurrency(packed, code);
    size_t symbolLength = g->symbolLength;
    const uint8_t* symbol = g->symbol;
    if (symbol == nullptr)
    {
        symbol = Resolve(c->symbols, c->symbolSteps, c->symbolCount, step + 1, packed,
                         currencySymbol, &symbolLength);
    }
    if (symbol == nullptr)
    {
        symbol = (const uint8_t*)code;
        symbolLength = 3;
    }
    TrySymbol(q, symbol, symbolLength, code);
    if (q->equivalents)
    {
        TryEquivalents(q, symbol, symbolLength, code);
    }
    TrySymbol(q, (const uint8_t*)code, 3, code);
    size_t nameLength = g->nameLength;
    const uint8_t* name = g->name;
    if (name == nullptr)
    {
        name = Resolve(c->names, c->nameSteps, c->nameCount, step + 1, packed, currencyName,
                       &nameLength);
    }
    TryName(q, name == nullptr ? (const uint8_t*)code : name, name == nullptr ? 3 : nameLength,
            code);
}

// A step's plural names of a currency; whether it gives any.
static bool TryPlurals(Search* q, uint16_t packed, const Given* g)
{
    char code[3];
    mlocUnpackCurrency(packed, code);
    bool any = false;
    for (uint32_t i = 0; i < g->pluralCount; i++)
    {
        for (uint32_t f = currencyNameZero; f <= currencyNameOther; f++)
        {
            size_t length = 0;
            const uint8_t* name = RowField(&g->plurals[i]->t, g->pluralRows[i], f, &length);
            if (name != nullptr)
            {
                TryName(q, name, length, code);
                any = true;
            }
        }
    }
    return any;
}

static bool Seen(const uint8_t* bits, uint16_t code)
{
    return (bits[code / 8] >> (code % 8) & 1) != 0;
}

static void Mark(uint8_t* bits, uint16_t code)
{
    bits[code / 8] = (uint8_t)(bits[code / 8] | 1u << (code % 8));
}

// Takes a step's best plural name over the best name so far, which wins
// a tie: ICU lists earlier steps first, and a step's table before its
// plurals.
static void TakeBest(Search* q, uint32_t units, const char* code)
{
    if (units > q->nameUnits)
    {
        q->nameUnits = units;
        memcpy(q->nameCode, code, 3);
    }
}

// ICU's collectCurrencyNames and searchCurrencyName: step by step, the
// currencies of its Currencies table (a symbol or a name of the step's
// own) not listed at an earlier step, and those its plurals give.
static void SearchChain(Search* q, const CurrencyChain* c)
{
    uint8_t listed[currencyMaxCode / 8 + 1] = {0};
    uint8_t plurals[currencyMaxCode / 8 + 1] = {0};
    uint32_t last = 0;
    for (uint32_t i = 0; i < c->symbolCount; i++)
    {
        last = c->symbolSteps[i] > last ? c->symbolSteps[i] : last;
    }
    for (uint32_t i = 0; i < c->nameCount; i++)
    {
        last = c->nameSteps[i] > last ? c->nameSteps[i] : last;
    }
    for (uint32_t step = 0; step <= last; step++)
    {
        Step st;
        OpenStep(c, step, &st);
        // The step's table and its plurals are searched in one walk, each
        // with a best of its own.
        Search pluralSearch = *q;
        for (uint16_t code = Least(&st); code != 0; Advance(&st, code), code = Least(&st))
        {
            Given g;
            GivenAt(&st, code, &g);
            if ((g.symbol != nullptr || g.name != nullptr) && !Seen(listed, code))
            {
                Mark(listed, code);
                TryListed(q, c, step, code, &g);
            }
            if (g.pluralCount > 0 && !Seen(plurals, code) && TryPlurals(&pluralSearch, code, &g))
            {
                Mark(plurals, code);
            }
        }
        TakeBest(q, pluralSearch.nameUnits, pluralSearch.nameCode);
    }
}

// uprv_parseCurrency at the segment: the first 100 units of the input,
// uppercased for the names; the end moves by the units matched, as ICU
// moves it (by the uppercased text's units for a name).
static void MatchAny(const Parser* r, const CurrencyChain* c, Segment* s, Parsed* p)
{
    // No currency symbol or name begins with a digit or an ignorable but
    // a bidi control (the generator checks it): the search, the longest
    // step of a parse, is passed where a number or a space begins.
    size_t size = 0;
    uint32_t lead = mlocParseCodePoint(s->text, s->length, s->at, &size);
    if (mlocParseDigit(r->sets, lead) >= 0 || (mlocParseHas(r->sets, parseIgnorables, lead) &&
                                               !mlocParseHas(r->sets, parseStrictIgnorables, lead)))
    {
        return;
    }
    Search q = {.sets = r->sets, .turkic = c->turkic, .input = s->text + s->at};
    size_t rest = s->length - s->at;
    q.inputLength = BytesOf(q.input, rest, maxInputUnits);
    q.inputUnits = UnitsOf(q.input, q.inputLength);
    Upper(r->sets, c->turkic, q.input, q.inputLength, maxInputUnits, &q.upper);
    q.equivalents = MayBeSign(r->sets, lead, q.input[0]);
    SearchChain(&q, c);
    if (q.nameUnits > 0 && q.nameUnits >= q.symbolUnits)
    {
        Take(s, p, BytesOf(q.input, rest, q.nameUnits), q.nameCode);
    }
    else if (q.symbolUnits > 0)
    {
        Take(s, p, BytesOf(q.input, rest, q.symbolUnits), q.symbolCode);
    }
}

// The longest of the currency's long names, case folded.
static void MatchNames(const Parser* r, const ParseCurrency* c, Segment* s, Parsed* p)
{
    uint32_t longest = 0;
    size_t bytes = 0;
    for (int i = 0; i < 6; i++)
    {
        size_t n = c->names[i] == nullptr
                       ? 0
                       : mlocParsePrefix(r->sets, s, c->names[i], c->nameLengths[i]);
        uint32_t units = n == 0 ? 0 : UnitsOf(c->names[i], c->nameLengths[i]);
        if (units > longest)
        {
            longest = units;
            bytes = n;
        }
    }
    if (longest > 0)
    {
        Take(s, p, bytes, c->code);
    }
}

// Whether a code point after the code can go on a name: no name has a
// digit or an ignorable but a bidi control past its first letters where
// it begins with the code.
static bool NameGoesOn(const Parser* r, const Segment* s)
{
    if (s->at == s->length)
    {
        return false;
    }
    size_t size = 0;
    uint32_t next = mlocParseCodePoint(s->text, s->length, s->at, &size);
    return mlocParseDigit(r->sets, next) < 0 &&
           (!mlocParseHas(r->sets, parseIgnorables, next) ||
            mlocParseHas(r->sets, parseStrictIgnorables, next));
}

// ICU's matchCurrency: the symbol as written, the text shown when it is
// another, the code in any case, then any currency or the long names.
// Where a name begins with the code, the longer is taken: ICU takes the
// code, leaving "o" of "Euro" unread (record mloc-0033).
static void MatchText(const Parser* r, const ParseCurrency* c, Segment* s, Parsed* p)
{
    size_t code = mlocParsePrefix(r->sets, s, (const uint8_t*)c->code, 3);
    if (Begins(s, c->symbol, c->symbolLength))
    {
        Take(s, p, c->symbolLength, c->code);
        return;
    }
    if (Begins(s, c->shown, c->shownLength))
    {
        Take(s, p, c->shownLength, c->code);
        return;
    }
    size_t initial = s->at;
    s->at += code;
    bool names = code == 0 || NameGoesOn(r, s);
    s->at = initial;
    if (names && c->chain != nullptr)
    {
        MatchAny(r, c->chain, s, p);
    }
    else if (names)
    {
        MatchNames(r, c, s, p);
    }
    if (code > 0 && (!p->hasCode || s->at - initial <= code))
    {
        s->at = initial;
        Take(s, p, code, c->code);
    }
}

void mlocMatchCurrency(const Parser* r, Segment* s, Parsed* p)
{
    const ParseCurrency* c = r->currency;
    if (c == nullptr || p->hasCode)
    {
        return;
    }
    size_t initial = s->at;
    // The spacing is a weak match: it does not move the end. When one
    // UTF-16 unit follows it, ICU's maybeMore short-circuits its currency
    // match (overlap == segment.length() || matchCurrency(...)), which is
    // kept: "706 $" ends at 3 in strict mode.
    size_t spacing = SeenNumber(p) ? mlocParsePrefix(r->sets, s, s_nbsp, sizeof(s_nbsp)) : 0;
    s->at += spacing;
    // Text that ends with the spacing has no currency to read.
    if (s->at == s->length || (spacing > 0 && UnitsOf(s->text + s->at, s->length - s->at) == 1))
    {
        s->at = initial;
        return;
    }
    MatchText(r, c, s, p);
    if (!p->hasCode)
    {
        s->at = initial;
        return;
    }
    if (!SeenNumber(p))
    {
        s->at += mlocParsePrefix(r->sets, s, s_nbsp, sizeof(s_nbsp));
    }
}
