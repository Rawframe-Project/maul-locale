// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The matchers of number parsing (parse_match.h), each as ICU4C 79's of
// the same name reads: symbols, ignorables, code points, the decimal
// digits with their separators, the exponent, and the affixes as series.

#include "parse_match.h"

#include "bytes.h"
#include "number_tables.h"
#include "parse_tables.h"

#include <string.h>

uint32_t mlocParseCodePoint(const uint8_t* text, size_t length, size_t at, size_t* sizeOut)
{
    uint8_t lead = text[at];
    size_t n = lead < 0x80   ? 1
               : lead < 0xC2 ? 0
               : lead < 0xE0 ? 2
               : lead < 0xF0 ? 3
               : lead < 0xF5 ? 4
                             : 0;
    *sizeOut = 1;
    if (n == 0 || at + n > length)
    {
        return parseNoCodePoint;
    }
    static const uint8_t s_masks[5] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t cp = lead & s_masks[n];
    for (size_t i = 1; i < n; i++)
    {
        if ((text[at + i] & 0xC0) != 0x80)
        {
            return parseNoCodePoint;
        }
        cp = cp << 6 | (text[at + i] & 0x3F);
    }
    static const uint32_t s_least[5] = {0, 0, 0x80, 0x800, 0x10000};
    if (cp < s_least[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
    {
        return parseNoCodePoint;
    }
    *sizeOut = n;
    return cp;
}

static uint32_t First(const Segment* s, size_t* sizeOut)
{
    return mlocParseCodePoint(s->text, s->length, s->at, sizeOut);
}

// The parse sets' folds, after the ranges.
static const uint8_t* Folds(const uint8_t* sets)
{
    return sets + parseHeaderSize + (size_t)parseClassCount * parseClassSize +
           (size_t)mlocLoad16(sets + 2) * parseRangeSize;
}

// What a code point folds to by the parse sets' folds, or itself.
static uint32_t Fold(const uint8_t* sets, uint32_t cp)
{
    const uint8_t* folds = Folds(sets);
    uint32_t low = 0;
    uint32_t high = mlocLoad16(sets + 8);
    // The last run that starts at or before the code point.
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (mlocLoad32(folds + (size_t)middle * parseFoldSize) <= cp)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    if (low == 0)
    {
        return cp;
    }
    const uint8_t* run = folds + (size_t)(low - 1) * parseFoldSize;
    uint32_t offset = cp - mlocLoad32(run);
    uint32_t stride = run[6];
    bool in = offset % stride == 0 && offset / stride < mlocLoad16(run + 4);
    return in ? (uint32_t)((int64_t)cp + (int32_t)mlocLoad32(run + 8)) : cp;
}

// Whether two code points are equal or fold alike (ICU's
// codePointsEqual); with units set, only those of the BMP fold, as ICU's
// prefix comparison folds UTF-16 units.
static bool SameFolded(const uint8_t* sets, uint32_t a, uint32_t b, bool units)
{
    if (a == b)
    {
        return true;
    }
    return (!units || (a <= 0xFFFF && b <= 0xFFFF)) && Fold(sets, a) == Fold(sets, b);
}

// The bytes of the segment that begin with a text, compared by code
// point as ICU's getCommonPrefixLength compares; 0 when it does not.
static size_t PrefixFolded(const uint8_t* sets, const Segment* s, const uint8_t* text,
                           size_t length)
{
    size_t at = s->at;
    for (size_t i = 0; i < length;)
    {
        size_t a = 0;
        size_t b = 0;
        if (at >= s->length)
        {
            return 0;
        }
        uint32_t x = mlocParseCodePoint(s->text, s->length, at, &a);
        uint32_t y = mlocParseCodePoint(text, length, i, &b);
        if (x == parseNoCodePoint || y == parseNoCodePoint || !SameFolded(sets, x, y, true))
        {
            return 0;
        }
        at += a;
        i += b;
    }
    return length == 0 ? 0 : at - s->at;
}

bool mlocParseHas(const uint8_t* sets, uint32_t set, uint32_t cp)
{
    const uint8_t* entry = sets + parseHeaderSize + (size_t)set * parseClassSize;
    const uint8_t* ranges =
        sets + parseHeaderSize + parseClassCount * parseClassSize + (size_t)mlocLoad16(entry) * 8;
    uint32_t low = 0;
    uint32_t high = mlocLoad16(entry + 2);
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* range = ranges + (size_t)middle * parseRangeSize;
        if (cp < mlocLoad32(range))
        {
            high = middle;
        }
        else if (cp > mlocLoad32(range + 4))
        {
            low = middle + 1;
        }
        else
        {
            return true;
        }
    }
    return false;
}

// Whether a class holds a symbol: its one code point, or one of the
// class's strings (ICU's UnicodeSet::contains of a string).
static bool HoldsSymbol(const uint8_t* sets, uint32_t set, const uint8_t* text, size_t length)
{
    size_t size = 0;
    uint32_t cp = length > 0 ? mlocParseCodePoint(text, length, 0, &size) : parseNoCodePoint;
    if (size == length && cp != parseNoCodePoint)
    {
        return mlocParseHas(sets, set, cp);
    }
    const uint8_t* entry = sets + parseHeaderSize + (size_t)set * parseClassSize;
    const uint8_t* offsets = Folds(sets) + (size_t)mlocLoad16(sets + 8) * parseFoldSize;
    const uint8_t* stringText = offsets + 2 * ((size_t)mlocLoad16(sets + 4) + 1);
    for (uint32_t i = 0; i < mlocLoad16(entry + 6); i++)
    {
        uint32_t s = mlocLoad16(entry + 4) + i;
        uint32_t start = mlocLoad16(offsets + 2 * (size_t)s);
        uint32_t end = mlocLoad16(offsets + 2 * (size_t)s + 2);
        if (end - start == length && memcmp(stringText + start, text, length) == 0)
        {
            return true;
        }
    }
    return false;
}

static const uint8_t* Symbol(const Formatter* f, NumberSymbol s, size_t* lengthOut)
{
    *lengthOut = f->core.symbolLength[s];
    return f->core.symbolText + f->core.symbolStart[s];
}

static bool SeenNumber(const Parsed* p)
{
    return p->quantity.present || (p->flags & (parsedNaN | parsedInfinity)) != 0;
}

// The class a symbol matcher reads besides its symbol, and its symbol.
static uint32_t SymbolClass(MatcherKind kind, NumberSymbol* symbolOut)
{
    static const struct
    {
        uint8_t set;
        uint8_t symbol;
    } s_symbols[] = {
        [matchMinus] = {parseMinus, numberMinus},
        [matchPlus] = {parsePlus, numberPlus},
        [matchApproximately] = {parseApproximately, numberApproximately},
        [matchPercent] = {parsePercent, numberPercent},
        [matchPerMille] = {parsePerMille, numberPerMille},
        [matchNaN] = {parseClassCount, numberNaN},
        [matchInfinity] = {parseInfinity, numberInfinity},
    };
    *symbolOut = (NumberSymbol)s_symbols[kind].symbol;
    return s_symbols[kind].set;
}

// Whether a symbol matcher is off for what was parsed.
static bool SymbolDisabled(const Matcher* m, const Parsed* p)
{
    switch (m->kind)
    {
    case matchMinus:
    case matchPlus:
    case matchApproximately:
        return !m->trailing && SeenNumber(p);
    case matchPercent:
        return (p->flags & parsedPercent) != 0;
    case matchPerMille:
        return (p->flags & parsedPerMille) != 0;
    case matchNaN:
        return SeenNumber(p);
    default:
        return (p->flags & parsedInfinity) != 0;
    }
}

// ICU's SymbolMatcher: the symbol's text where its class does not hold
// it, else a code point of the class.
static void MatchSymbol(const Parser* r, const Matcher* m, Segment* s, Parsed* p)
{
    if (SymbolDisabled(m, p))
    {
        return;
    }
    NumberSymbol symbol;
    uint32_t set = SymbolClass((MatcherKind)m->kind, &symbol);
    size_t length = 0;
    const uint8_t* text = Symbol(r->f, symbol, &length);
    bool own = set == parseClassCount || !HoldsSymbol(r->sets, set, text, length);
    size_t size = 0;
    uint32_t cp = First(s, &size);
    size_t prefix = own ? PrefixFolded(r->sets, s, text, length) : 0;
    if (prefix > 0)
    {
        s->at += prefix;
    }
    else if (set != parseClassCount && mlocParseHas(r->sets, set, cp))
    {
        s->at += size;
    }
    else
    {
        return;
    }
    static const uint32_t s_flags[] = {
        [matchMinus] = parsedNegative,    [matchPercent] = parsedPercent,
        [matchPerMille] = parsedPerMille, [matchNaN] = parsedNaN,
        [matchInfinity] = parsedInfinity,
    };
    p->flags |= m->kind < sizeof(s_flags) / sizeof(s_flags[0]) ? s_flags[m->kind] : 0;
    p->charEnd = s->at;
}

// A code point of the ignorables, or of the bidi controls alone.
static void MatchIgnorables(const Parser* r, Segment* s, bool strict)
{
    size_t size = 0;
    uint32_t cp = First(s, &size);
    uint32_t set = strict ? parseStrictIgnorables : parseIgnorables;
    if (cp != parseNoCodePoint && mlocParseHas(r->sets, set, cp))
    {
        s->at += size;
    }
}

static void MatchCodePoint(const Parser* r, const Matcher* m, Segment* s, Parsed* p)
{
    size_t size = 0;
    uint32_t cp = First(s, &size);
    if (cp != parseNoCodePoint && SameFolded(r->sets, cp, m->arg, false))
    {
        s->at += size;
        p->charEnd = s->at;
    }
}

// A decimal matcher's settings (ICU's parse flags it reads).
typedef struct DecimalConfig
{
    bool integerOnly;
    bool groupingDisabled;
    bool strictGrouping;
    bool strictSeparators;
} DecimalConfig;

int mlocParseDigit(const uint8_t* sets, uint32_t cp)
{
    const uint8_t* entry = sets + parseHeaderSize + (size_t)parseDigits * parseClassSize;
    const uint8_t* ranges = sets + parseHeaderSize + (size_t)parseClassCount * parseClassSize +
                            (size_t)mlocLoad16(entry) * parseRangeSize;
    uint32_t low = 0;
    uint32_t high = mlocLoad16(entry + 2);
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* range = ranges + (size_t)middle * parseRangeSize;
        if (cp < mlocLoad32(range))
        {
            high = middle;
        }
        else if (cp > mlocLoad32(range + 4))
        {
            low = middle + 1;
        }
        else
        {
            return (int)((cp - mlocLoad32(range)) % 10);
        }
    }
    return -1;
}

// The digit at the segment and its value: a decimal digit by code point,
// or one of the numbering system's digit strings; -1 for none.
static int DigitAt(const Parser* r, Segment* s)
{
    size_t size = 0;
    uint32_t cp = First(s, &size);
    int digit = mlocParseDigit(r->sets, cp);
    if (digit >= 0)
    {
        s->at += size;
        return digit;
    }
    for (int d = 0; r->digitStrings && d < 10; d++)
    {
        size_t n = PrefixFolded(r->sets, s, r->f->core.digits + d * numberDigitSize,
                                r->f->core.digitLength);
        if (n > 0)
        {
            s->at += n;
            return d;
        }
    }
    return -1;
}

// A separator seen: its bytes in the text.
typedef struct Seen
{
    const uint8_t* text;
    size_t length;
} Seen;

static bool SeenIs(const Seen* seen)
{
    return seen->text != nullptr;
}

static bool InClass(const uint8_t* sets, uint32_t set, uint32_t cp)
{
    if (set == setAll)
    {
        return mlocParseHas(sets, parseComma, cp) || mlocParseHas(sets, parsePeriod, cp) ||
               mlocParseHas(sets, parseOtherGrouping, cp);
    }
    return mlocParseHas(sets, set, cp);
}

uint32_t mlocParseChooseSet(const uint8_t* sets, const uint8_t* text, size_t length, uint32_t first,
                            uint32_t second)
{
    size_t size = 0;
    uint32_t cp = length > 0 ? mlocParseCodePoint(text, length, 0, &size) : parseNoCodePoint;
    if (cp == parseNoCodePoint || size != length)
    {
        return setOwn;
    }
    return InClass(sets, first, cp) ? first : InClass(sets, second, cp) ? second : setOwn;
}

// Whether a separator's set holds a code point: its class, all the
// separators, or its own first code point.
static bool InSet(const Parser* r, uint32_t set, uint32_t own, uint32_t cp)
{
    if (cp == parseNoCodePoint)
    {
        return false;
    }
    return set == setOwn ? cp == own : InClass(r->sets, set, cp);
}

// ICU's DecimalMatcher::smokeTest: a digit, or a code point of the lead
// set. With both separators' sets static ICU takes the static lead set,
// whose separators are the strict ones in lenient mode and all of them in
// strict mode, as ICU 79 has it.
static bool MayLead(const Parser* r, const DecimalConfig* c, const Segment* s)
{
    size_t size = 0;
    uint32_t cp = First(s, &size);
    if (cp == parseNoCodePoint)
    {
        return r->digitStrings;
    }
    if (mlocParseDigit(r->sets, cp) >= 0)
    {
        return true;
    }
    if (!r->digitStrings && r->groupSet != setOwn && r->decimalSet != setOwn)
    {
        return c->strictSeparators ? InClass(r->sets, setAll, cp)
                                   : mlocParseHas(r->sets, parseStrictComma, cp) ||
                                         mlocParseHas(r->sets, parseStrictPeriod, cp) ||
                                         mlocParseHas(r->sets, parseOtherGrouping, cp);
    }
    // A numbering system's digit strings are tried by the digit reader.
    return r->digitStrings || InSet(r, r->groupSet, r->groupCodePoint, cp) ||
           InSet(r, r->decimalSet, r->decimalCodePoint, cp);
}

typedef struct Group
{
    size_t offset;
    int type;
    int count;
} Group;

// ICU's DecimalMatcher::validateGroup: the strict sizes, or in lenient
// mode no middle group of one digit.
static bool ValidGroup(const Parser* r, const DecimalConfig* c, const Group* g, bool primary)
{
    if (!c->strictGrouping)
    {
        return g->type != 1 || g->count != 1;
    }
    int grouping1 = r->f->core.primary;
    int grouping2 = r->f->core.secondary;
    switch (g->type)
    {
    case 0:
        return primary || (g->count != 0 && g->count <= grouping2);
    case 1:
        return g->count == (primary ? grouping1 : grouping2);
    default:
        return true;
    }
}

// The separators seen.
typedef struct Separators
{
    Seen grouping;
    Seen decimal;
} Separators;

// A separator at the segment (ICU's steps 1 to 4): the decimal one's
// text, the grouping one already seen, the grouping one's text, then a
// code point of the decimal and the grouping sets.
static void MatchSeparator(const Parser* r, const DecimalConfig* c, const Segment* s,
                           Separators* seen, bool* isDecimal, bool* isGrouping)
{
    size_t n =
        SeenIs(&seen->decimal) ? 0 : PrefixFolded(r->sets, s, r->decimalText, r->decimalLength);
    if (n > 0)
    {
        *isDecimal = true;
        seen->decimal = (Seen){s->text + s->at, n};
    }
    if (SeenIs(&seen->grouping) &&
        PrefixFolded(r->sets, s, seen->grouping.text, seen->grouping.length) > 0)
    {
        *isGrouping = true;
    }
    bool fresh = !c->groupingDisabled && !SeenIs(&seen->grouping) && !SeenIs(&seen->decimal);
    n = fresh ? PrefixFolded(r->sets, s, r->groupText, r->groupLength) : 0;
    if (n > 0)
    {
        *isGrouping = true;
        seen->grouping = (Seen){s->text + s->at, n};
    }
    size_t size = 0;
    uint32_t cp = First(s, &size);
    if (!*isGrouping && !SeenIs(&seen->decimal) && InSet(r, r->decimalSet, r->decimalCodePoint, cp))
    {
        *isDecimal = true;
        seen->decimal = (Seen){s->text + s->at, size};
    }
    fresh = !c->groupingDisabled && !SeenIs(&seen->grouping) && !SeenIs(&seen->decimal);
    if (fresh && InSet(r, r->groupSet, r->groupCodePoint, cp))
    {
        *isGrouping = true;
        seen->grouping = (Seen){s->text + s->at, size};
    }
}

// Appends a digit: a leading zero is dropped, a trailing one counted, and
// a significant digit past the decimal's capacity is an overflow.
static void AppendDigit(Quantity* q, int digit, bool* overflow)
{
    q->present = true;
    if (digit == 0)
    {
        q->zeros += q->count > 0 ? 1 : 0;
        return;
    }
    if (q->zeros >= mlocDecimalDigits - q->count)
    {
        *overflow = true;
        return;
    }
    memset(q->digits + q->count, 0, q->zeros);
    q->count += q->zeros;
    q->zeros = 0;
    q->digits[q->count++] = (uint8_t)digit;
}

// Gives back the last digits typed (ICU's adjustMagnitude and truncate
// of integer digits).
static void RemoveDigits(Quantity* q, uint32_t n)
{
    if (n <= q->zeros)
    {
        q->zeros -= n;
        return;
    }
    n -= q->zeros;
    q->zeros = 0;
    q->count = n < q->count ? q->count - n : 0;
    while (q->count > 0 && q->digits[q->count - 1] == 0)
    {
        q->count--;
        q->zeros++;
    }
}

// Whether a separator read ends the digits (ICU's checks): a decimal one
// of an integer-only matcher, a grouping one after the decimal one,
// group sizes that fail (in strict mode clearing the digits, but for a
// trailing grouping separator, which the back-off handles), or in strict
// mode two grouping separators in a row.
static bool SeparatorEnds(const Parser* r, const DecimalConfig* c, bool isDecimal, bool isGrouping,
                          const Group* curr, const Group* prev, Quantity* q)
{
    if ((isDecimal && c->integerOnly) || (curr->type == 2 && isGrouping))
    {
        return true;
    }
    if (!ValidGroup(r, c, prev, false) || (isDecimal && !ValidGroup(r, c, curr, true)))
    {
        if (!(isGrouping && curr->count == 0) && c->strictGrouping)
        {
            q->present = false;
        }
        return true;
    }
    return c->strictGrouping && curr->count == 0 && curr->type == 1;
}

// The digits and separators' loop of ICU's DecimalMatcher::match.
static void ReadDigits(const Parser* r, const DecimalConfig* c, Segment* s, Quantity* q,
                       int64_t* afterPoint, Group* curr, Group* prev, bool* overflow)
{
    Separators seen = {0};
    while (s->at < s->length)
    {
        int digit = DigitAt(r, s);
        if (digit >= 0)
        {
            AppendDigit(q, digit, overflow);
            curr->count++;
            *afterPoint += SeenIs(&seen.decimal) ? 1 : 0;
            continue;
        }
        bool isDecimal = false;
        bool isGrouping = false;
        MatchSeparator(r, c, s, &seen, &isDecimal, &isGrouping);
        if ((!isDecimal && !isGrouping) ||
            SeparatorEnds(r, c, isDecimal, isGrouping, curr, prev, q))
        {
            break;
        }
        prev->offset = curr->offset;
        prev->count = curr->count;
        prev->type = isDecimal ? -1 : curr->type;
        curr->offset = curr->count != 0 ? s->at : curr->offset;
        curr->type = isGrouping ? 1 : 2;
        curr->count = 0;
        s->at += isGrouping ? seen.grouping.length : seen.decimal.length;
    }
}

// ICU's back-off after the loop: a trailing grouping separator is not
// read, and in lenient mode a group of one digit gives its digits back;
// false when the grouping fails.
static bool CheckGroups(const Parser* r, const DecimalConfig* c, Segment* s, Quantity* q,
                        Group* curr, Group* prev)
{
    if (curr->type != 2 && curr->count == 0)
    {
        s->at = curr->offset;
        *curr = *prev;
        *prev = (Group){.offset = 0, .type = 0, .count = 1};
    }
    bool prevValid = ValidGroup(r, c, prev, false);
    bool currValid = ValidGroup(r, c, curr, true);
    if (!c->strictGrouping)
    {
        uint32_t remove = 0;
        if (!prevValid)
        {
            s->at = prev->offset;
            remove = (uint32_t)(prev->count + curr->count);
        }
        else if (!currValid && (prev->type != 0 || prev->count != 0))
        {
            s->at = curr->offset;
            remove = (uint32_t)curr->count;
        }
        RemoveDigits(q, remove);
        return true;
    }
    return curr->type == 2 || (prevValid && currValid);
}

// An exponent's digits as a number, false past 999,999,999.
static bool ExponentOf(const Quantity* q, int64_t* out)
{
    int64_t value = 0;
    for (uint32_t i = 0; i < q->count + q->zeros; i++)
    {
        value = value * 10 + (i < q->count ? q->digits[i] : 0);
        if (value > 999999999)
        {
            return false;
        }
    }
    *out = value;
    return true;
}

// ICU's DecimalMatcher: digits with grouping and decimal separators; with
// an exponent sign, an exponent's digits moving the number read before.
static void MatchDecimal(const Parser* r, const DecimalConfig* c, Segment* s, Parsed* p,
                         int exponentSign)
{
    if ((SeenNumber(p) && exponentSign == 0) || (exponentSign == 0 && !MayLead(r, c, s)))
    {
        return;
    }
    size_t initial = s->at;
    Quantity q = {0};
    int64_t afterPoint = 0;
    Group curr = {.offset = 0, .type = 0, .count = 0};
    Group prev = {.offset = 0, .type = -1, .count = -1};
    bool overflow = false;
    ReadDigits(r, c, s, &q, &afterPoint, &curr, &prev, &overflow);
    if (!CheckGroups(r, c, s, &q, &curr, &prev))
    {
        q.present = false;
    }
    if (!q.present)
    {
        s->at = initial;
        return;
    }
    if (exponentSign == 0)
    {
        p->quantity = q;
        p->afterPoint = afterPoint;
        p->overflow = p->overflow || overflow;
    }
    else if (s->at != initial)
    {
        int64_t exponent = 0;
        bool fits = !overflow && ExponentOf(&q, &exponent);
        p->overflow = p->overflow || !fits;
        p->exponent += exponentSign * exponent;
    }
    p->charEnd = s->at;
}

// A sign at the segment for an exponent: a code point of the sign sets,
// else the locale's own sign; 1, -1, or 0 with nothing read.
static int ExponentSign(const Parser* r, Segment* s, bool* partialOut)
{
    size_t size = 0;
    uint32_t cp = First(s, &size);
    if (cp != parseNoCodePoint &&
        (mlocParseHas(r->sets, parseMinus, cp) || mlocParseHas(r->sets, parsePlus, cp)))
    {
        s->at += size;
        return mlocParseHas(r->sets, parseMinus, cp) ? -1 : 1;
    }
    for (int i = 0; i < 2; i++)
    {
        size_t length = 0;
        const uint8_t* text = Symbol(r->f, i == 0 ? numberMinus : numberPlus, &length);
        if (HoldsSymbol(r->sets, i == 0 ? parseMinus : parsePlus, text, length) || length == 0 ||
            cp == parseNoCodePoint ||
            !SameFolded(r->sets, mlocParseCodePoint(text, length, 0, &size), cp, false))
        {
            continue;
        }
        size_t n = PrefixFolded(r->sets, s, text, length);
        *partialOut = n == 0;
        s->at += n;
        return n == 0 ? 0 : (i == 0 ? -1 : 1);
    }
    return 0;
}

// ICU's ScientificMatcher: after the number, the exponent symbol, an
// optional sign and the exponent's digits, bidi controls allowed around
// the sign.
static void MatchScientific(const Parser* r, Segment* s, Parsed* p)
{
    size_t length = 0;
    const uint8_t* symbol = Symbol(r->f, numberExponential, &length);
    size_t n = SeenNumber(p) && (p->flags & parsedHasExponent) == 0
                   ? PrefixFolded(r->sets, s, symbol, length)
                   : 0;
    if (n == 0 || s->at + n == s->length)
    {
        return;
    }
    size_t initial = s->at;
    s->at += n;
    MatchIgnorables(r, s, true);
    bool partial = false;
    int sign = s->at < s->length ? ExponentSign(r, s, &partial) : 0;
    sign = sign == 0 && !partial ? 1 : sign;
    if (s->at < s->length)
    {
        MatchIgnorables(r, s, true);
    }
    if (partial || s->at == s->length)
    {
        s->at = initial;
        return;
    }
    DecimalConfig c = {.integerOnly = true, .groupingDisabled = true};
    size_t digits = s->at;
    MatchDecimal(r, &c, s, p, sign);
    if (s->at != digits)
    {
        p->flags |= parsedHasExponent;
    }
    else
    {
        s->at = initial;
    }
}

// ICU's SeriesMatcher: each matcher in turn, an ignorables matcher
// flexible (it may match many times or none); the whole series or
// nothing.
static void MatchSeries(const Parser* r, const Series* series, Segment* s, Parsed* p)
{
    Parsed backup = *p;
    size_t initial = s->at;
    for (uint32_t i = 0; i < series->count;)
    {
        const Matcher* m = &series->matchers[i];
        size_t offset = s->at;
        if (s->at < s->length)
        {
            mlocParseMatch(r, m, s, p);
        }
        bool success = s->at != offset;
        bool flexible = m->kind == matchIgnorables;
        if (success && flexible)
        {
            continue;
        }
        if (success)
        {
            i++;
            // Trailing weak characters are left for the next matcher.
            if (i < series->count && s->at != p->charEnd && p->charEnd > offset)
            {
                s->at = p->charEnd;
            }
            continue;
        }
        if (flexible)
        {
            i++;
            continue;
        }
        s->at = initial;
        *p = backup;
        return;
    }
}

bool mlocParseMatched(const Parser* r, int8_t affix, int8_t seen)
{
    if (affix < 0 || seen < 0)
    {
        return affix < 0 && seen == affixNone;
    }
    const Series* a = &r->series[affix];
    const Series* b = &r->series[seen];
    return a->length == b->length && memcmp(a->text, b->text, a->length) == 0;
}

// ICU's AffixMatcher: its prefix before the number, its suffix after it
// when the prefix seen is its own.
static void MatchAffix(const Parser* r, const AffixMatcher* a, Segment* s, Parsed* p)
{
    size_t initial = s->at;
    if (!SeenNumber(p))
    {
        if (p->prefix != affixNone || a->prefix < 0)
        {
            return;
        }
        MatchSeries(r, &r->series[a->prefix], s, p);
        p->prefix = s->at != initial ? a->prefix : p->prefix;
        return;
    }
    if (p->suffix != affixNone || a->suffix < 0 || !mlocParseMatched(r, a->prefix, p->prefix))
    {
        return;
    }
    MatchSeries(r, &r->series[a->suffix], s, p);
    p->suffix = s->at != initial ? a->suffix : p->suffix;
}

void mlocParseMatch(const Parser* r, const Matcher* m, Segment* s, Parsed* p)
{
    DecimalConfig c = {
        .integerOnly = (r->flags & modeIntegerOnly) != 0,
        .groupingDisabled = (r->flags & modeGroupingDisabled) != 0,
        .strictGrouping = (r->flags & modeStrictGrouping) != 0,
        .strictSeparators = (r->flags & modeStrictSeparators) != 0,
    };
    switch (m->kind)
    {
    case matchIgnorables:
        MatchIgnorables(r, s, (r->flags & modeStrictIgnorables) != 0);
        break;
    case matchDecimal:
        MatchDecimal(r, &c, s, p, 0);
        break;
    case matchScientific:
        MatchScientific(r, s, p);
        break;
    case matchCodePoint:
        MatchCodePoint(r, m, s, p);
        break;
    case matchAffix:
        MatchAffix(r, &r->affixes[m->arg], s, p);
        break;
    case validateNumber:
    case validateAffix:
    case validateScale:
        break;
    default:
        MatchSymbol(r, m, s, p);
        break;
    }
}
