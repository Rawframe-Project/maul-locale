// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number parsing (number_parse.h), ICU4C 79's NumberParserImpl as
// DecimalFormat::parse runs it: a list of matchers (parse_match.h) built
// from the formatter's pattern and symbols, run greedily (the first that
// reads text starts the list again), then each matcher's post-processing.
// The list is a small array on the stack, and the text is read once,
// without recursion or allocation.

#include "number_parse.h"

#include "bytes.h"
#include "decimal.h"
#include "number_tables.h"
#include "parse_currency.h"
#include "parse_match.h"
#include "parse_tables.h"

#include <string.h>

// ICU's sign types of affix patterns: positive, positive with a plus
// sign, negative.
typedef enum SignType
{
    signPositive,
    signPositiveSign,
    signNegative,
} SignType;

// A token of an affix: a placeholder byte for a symbol, else a code
// point; its length into sizeOut.
static uint32_t TokenAt(const uint8_t* text, size_t length, size_t at, bool* symbolOut,
                        size_t* sizeOut)
{
    *symbolOut = text[at] < 0x20;
    if (*symbolOut)
    {
        *sizeOut = 1;
        return text[at];
    }
    return mlocParseCodePoint(text, length, at, sizeOut);
}

static bool AddTo(Series* series, Matcher m)
{
    if (series->count == parseMaxSeries)
    {
        return false;
    }
    series->matchers[series->count++] = m;
    return true;
}

// The matcher of an affix's symbol: a sign in an affix may follow the
// number; the percent placeholder of the per mille style is the per
// mille sign; the currency is the currency matcher, which a number
// formatter does not have.
static Matcher SymbolMatcher(uint32_t placeholder, bool perMille, bool currency)
{
    switch (placeholder)
    {
    case numberAffixMinus:
        return (Matcher){.kind = matchMinus, .trailing = true};
    case numberAffixPlus:
        return (Matcher){.kind = matchPlus, .trailing = true};
    case numberAffixPercent:
        return (Matcher){.kind = perMille ? matchPerMille : matchPercent};
    case numberAffixPerMille:
        return (Matcher){.kind = matchPerMille};
    default:
        return currency ? (Matcher){.kind = matchCurrency}
                        : (Matcher){.kind = matchCodePoint, .arg = parseNoCodePoint};
    }
}

// A token's length in an ICU affix pattern string, in UTF-16 units: a
// symbol's characters (¤¤ for the code), a code point's units.
static uint16_t TokenUnits(bool symbol, uint32_t token)
{
    static const uint8_t s_units[] = {
        [numberAffixCurrencyCode] = 2,
        [numberAffixCurrencyName] = 3,
        [numberAffixCurrencyNarrow] = 5,
    };
    if (symbol)
    {
        return token < sizeof(s_units) && s_units[token] != 0 ? s_units[token] : 1;
    }
    return token >= 0x10000 && token != parseNoCodePoint ? 2 : 1;
}

// Builds an affix's series (ICU's AffixPatternMatcherBuilder): an
// ignorables matcher between tokens but after an ignorable literal, the
// symbols' matchers, the other literals' code points; in exact mode no
// ignorables, every literal its code point. False when it does not fit.
static bool BuildSeries(const Parser* r, const uint8_t* text, size_t length, Series* out)
{
    bool exact = (r->flags & modeExactAffix) != 0;
    bool perMille = r->f->style == mloc_numberPerMille;
    memset(out, 0, sizeof(*out));
    if (length > sizeof(out->text))
    {
        return false;
    }
    memcpy(out->text, text, length);
    out->length = (uint16_t)length;
    bool lastSymbol = false;
    uint32_t last = parseNoCodePoint;
    bool ok = true;
    for (size_t at = 0; at < length;)
    {
        bool symbol = false;
        size_t size = 0;
        uint32_t token = TokenAt(text, length, at, &symbol, &size);
        at += size;
        out->units = (uint16_t)(out->units + TokenUnits(symbol, token));
        if (!exact && out->count > 0 &&
            (lastSymbol || !mlocParseHas(r->sets, parseIgnorables, last)))
        {
            ok = ok && AddTo(out, (Matcher){.kind = matchIgnorables});
        }
        bool ignorable = !symbol && !exact && mlocParseHas(r->sets, parseIgnorables, token);
        if (symbol || !ignorable)
        {
            ok = ok && AddTo(out, symbol ? SymbolMatcher(token, perMille, r->currency != nullptr)
                                         : (Matcher){.kind = matchCodePoint, .arg = token});
        }
        lastSymbol = symbol;
        last = token;
    }
    return ok;
}

// The series of an affix pattern, one per distinct text; affixNone for
// the empty pattern (ICU makes no matcher of it), and -3 when the
// pattern does not fit.
static int AddSeries(Parser* r, const uint8_t* text, size_t length)
{
    if (length == 0)
    {
        return affixNone;
    }
    for (uint32_t i = 0; i < r->seriesCount; i++)
    {
        if (r->series[i].length == length && memcmp(r->series[i].text, text, length) == 0)
        {
            return (int)i;
        }
    }
    if (r->seriesCount == parseMaxAffixes ||
        !BuildSeries(r, text, length, &r->series[r->seriesCount]))
    {
        return -3;
    }
    return (int)r->seriesCount++;
}

static const uint8_t* AffixText(const Parser* r, int affix, size_t* lengthOut)
{
    *lengthOut = r->affixLength[affix];
    return r->affixText[affix];
}

static bool HasPlaceholder(const Parser* r, int affix, uint8_t placeholder)
{
    size_t length = 0;
    const uint8_t* text = AffixText(r, affix, &length);
    return memchr(text, placeholder, length) != nullptr;
}

static bool Append(uint8_t* buffer, size_t* at, const uint8_t* text, size_t length)
{
    if (length > parseMaxAffixText - *at)
    {
        return false;
    }
    memcpy(buffer + *at, text, length);
    *at += length;
    return true;
}

// The affix patterns the parser reads: the formatter's parse affixes,
// within one of the currency's outer patterns when it has them (a prefix
// after the text before the number, a suffix before the text after it).
// False when one does not fit.
static bool FillAffixes(Parser* r, uint32_t variant)
{
    const NumberCore* c = &r->f->core;
    const ParseCurrency* outer =
        r->currency != nullptr && r->currency->outer ? r->currency : nullptr;
    bool ok = true;
    for (int affix = 0; affix < affixCount; affix++)
    {
        bool prefix = affix == affixPositivePrefix || affix == affixNegativePrefix;
        size_t at = 0;
        uint8_t* out = r->affixText[affix];
        if (outer != nullptr && prefix)
        {
            ok = ok && Append(out, &at, outer->before[variant], outer->beforeLength[variant]);
        }
        ok = ok && Append(out, &at, c->affixText + c->parseStart[affix], c->parseLength[affix]);
        if (outer != nullptr && !prefix)
        {
            ok = ok && Append(out, &at, outer->after[variant], outer->afterLength[variant]);
        }
        r->affixLength[affix] = (uint8_t)at;
    }
    return ok;
}

// An affix pattern of a sign type (ICU's patternInfoToStringBuilder):
// the positive or negative affix, or for a plus sign the negative one
// with the plus sign in the minus sign's place, or, when the negative
// affixes have no minus sign, the positive one after a plus sign.
static size_t PatternOf(const Parser* r, SignType type, bool prefix, uint8_t* buffer,
                        size_t capacity)
{
    int positive = prefix ? affixPositivePrefix : affixPositiveSuffix;
    int negative = prefix ? affixNegativePrefix : affixNegativeSuffix;
    size_t length = 0;
    if (type != signPositiveSign)
    {
        const uint8_t* text = AffixText(r, type == signNegative ? negative : positive, &length);
        length = length < capacity ? length : capacity;
        memcpy(buffer, text, length);
        return length;
    }
    bool minus = HasPlaceholder(r, affixNegativePrefix, numberAffixMinus) ||
                 HasPlaceholder(r, affixNegativeSuffix, numberAffixMinus);
    const uint8_t* text = AffixText(r, minus ? negative : positive, &length);
    size_t at = 0;
    if (!minus && prefix && capacity > 0)
    {
        buffer[at++] = numberAffixPlus;
    }
    for (size_t i = 0; i < length && at < capacity; i++)
    {
        buffer[at++] = text[i] == numberAffixMinus ? (uint8_t)numberAffixPlus : text[i];
    }
    return at;
}

// Whether an affix is only symbols and ignorables (ICU's
// containsOnlySymbolsAndIgnorables).
static bool OnlySymbols(const Parser* r, int affix)
{
    size_t length = 0;
    const uint8_t* text = AffixText(r, affix, &length);
    for (size_t at = 0; at < length;)
    {
        bool symbol = false;
        size_t size = 0;
        uint32_t token = TokenAt(text, length, at, &symbol, &size);
        at += size;
        if (!symbol && !mlocParseHas(r->sets, parseIgnorables, token))
        {
            return false;
        }
    }
    return true;
}

// ICU's isInteresting: affix matchers are made in strict mode, or when an
// affix has a literal ignorables do not cover, or a suffix a sign.
static bool Interesting(const Parser* r)
{
    bool plain = true;
    for (int affix = 0; affix < affixCount; affix++)
    {
        plain = plain && OnlySymbols(r, affix);
    }
    for (int affix = affixPositiveSuffix; affix < affixCount; affix += 2)
    {
        plain = plain && !HasPlaceholder(r, affix, numberAffixMinus) &&
                !HasPlaceholder(r, affix, numberAffixPlus);
    }
    return (r->flags & modeFullAffixes) != 0 || !plain;
}

static int AffixUnits(const Parser* r, int8_t affix)
{
    return affix < 0 ? 0 : r->series[affix].units;
}

// ICU's AffixMatcher::compareTo: the longer prefix, then the longer
// suffix, first.
static bool AffixAfter(const Parser* r, const AffixMatcher* a, const AffixMatcher* b)
{
    if (AffixUnits(r, a->prefix) != AffixUnits(r, b->prefix))
    {
        return AffixUnits(r, a->prefix) < AffixUnits(r, b->prefix);
    }
    return AffixUnits(r, a->suffix) < AffixUnits(r, b->suffix);
}

// Adds an affix matcher; false when the matchers are full.
static bool AddAffixMatcher(Parser* r, int prefix, int suffix, uint32_t flags)
{
    if (r->affixCount == parseMaxAffixMatchers)
    {
        return false;
    }
    r->affixes[r->affixCount++] =
        (AffixMatcher){.prefix = (int8_t)prefix, .suffix = (int8_t)suffix, .flags = flags};
    return true;
}

// The positive pair.
typedef struct Pair
{
    int prefix;
    int suffix;
} Pair;

// A sign type's affix matchers: its pair, unless a later type's equals
// the positive one, and in lenient mode each affix of it alone, unless
// a later type's equals the positive one's. False when an affix does not
// fit.
static bool AddSignType(Parser* r, SignType type, Pair* positive)
{
    uint8_t buffer[parseMaxAffixText + 1];
    size_t length = PatternOf(r, type, true, buffer, sizeof(buffer));
    int prefix = AddSeries(r, buffer, length);
    length = PatternOf(r, type, false, buffer, sizeof(buffer));
    int suffix = AddSeries(r, buffer, length);
    if (prefix < affixNone || suffix < affixNone)
    {
        return false;
    }
    // As ICU, the positive pair is the positive type's alone: with a plus
    // sign shown, every type is compared with no affixes.
    bool first = type == signPositive;
    if (first)
    {
        *positive = (Pair){prefix, suffix};
    }
    else if (prefix == positive->prefix && suffix == positive->suffix)
    {
        return true;
    }
    uint32_t flags = type == signNegative ? parsedNegative : 0;
    bool ok = AddAffixMatcher(r, prefix, suffix, flags);
    if ((r->flags & modeUnpairedAffixes) != 0 && prefix >= 0 && suffix >= 0)
    {
        if (first || prefix != positive->prefix)
        {
            ok = ok && AddAffixMatcher(r, prefix, affixNone, flags);
        }
        if (first || suffix != positive->suffix)
        {
            ok = ok && AddAffixMatcher(r, affixNone, suffix, flags);
        }
    }
    return ok;
}

// ICU's createAffixMatchers: the positive sign type (with a plus sign when
// the formatter shows one) and the negative one, then the matchers sorted
// (a stable bubble sort, as ICU's) into the list. The except-zero sign
// display, which ICU's DecimalFormat lacks, writes zero without the plus
// sign: strict mode reads the positive type too. With the ISO append,
// which ICU lacks, lenient mode reads it too whenever a plus sign is
// shown: NaN (and zero, except zero) is written without one, and the
// appended code after the number would otherwise be left unread ("£ NaN
// GBP" where the plus sign is a suffix). Strict mode reads each outer
// pattern of a currency's categories, as the formatter writes them.
static bool AddAffixMatchers(Parser* r)
{
    if (!Interesting(r))
    {
        return true;
    }
    bool strict = (r->flags & modeFullAffixes) != 0;
    bool plus = (r->flags & modePlusAllowed) != 0;
    bool appended = r->currency != nullptr && r->currency->appended;
    bool unsignedToo = plus && (strict ? r->f->signDisplay == mloc_signExceptZero : appended);
    uint32_t variants =
        strict && r->currency != nullptr && r->currency->outer ? r->currency->outerCount : 1;
    for (uint32_t v = 0; v < variants; v++)
    {
        Pair positive = {affixNone, affixNone};
        if (!FillAffixes(r, v) ||
            !AddSignType(r, plus ? signPositiveSign : signPositive, &positive) ||
            (unsignedToo && !AddSignType(r, signPositive, &positive)) ||
            !AddSignType(r, signNegative, &positive))
        {
            return false;
        }
    }
    // The rest of the parser reads the first pattern's affixes.
    (void)FillAffixes(r, 0);
    for (bool changed = true; changed;)
    {
        changed = false;
        for (uint32_t i = 1; i < r->affixCount; i++)
        {
            if (AffixAfter(r, &r->affixes[i - 1], &r->affixes[i]))
            {
                AffixMatcher swap = r->affixes[i - 1];
                r->affixes[i - 1] = r->affixes[i];
                r->affixes[i] = swap;
                changed = true;
            }
        }
    }
    for (uint32_t i = 0; i < r->affixCount; i++)
    {
        r->list[r->count++] = (Matcher){.kind = matchAffix, .arg = i};
    }
    return true;
}

static void Add(Parser* r, MatcherKind kind, uint32_t arg)
{
    r->list[r->count++] = (Matcher){.kind = (uint8_t)kind, .arg = arg};
}

static bool PatternHas(const Parser* r, uint8_t placeholder)
{
    for (int affix = 0; affix < affixCount; affix++)
    {
        if (HasPlaceholder(r, affix, placeholder))
        {
            return true;
        }
    }
    return false;
}

static bool HasCurrency(const Parser* r)
{
    for (uint32_t placeholder = numberAffixCurrency; placeholder <= numberAffixCurrencyNarrow;
         placeholder++)
    {
        if (PatternHas(r, (uint8_t)placeholder))
        {
            return true;
        }
    }
    return false;
}

// Whether the numbering system's digits are not matched by code point
// (ICU's getCodePointZero is not a decimal digit 0): a digit not one code
// point, or not the zero's code point plus its value.
static bool NeedsDigitStrings(const NumberCore* c, const uint8_t* sets)
{
    size_t size = 0;
    uint32_t zero = mlocParseCodePoint(c->digits, c->digitLength, 0, &size);
    if (zero == parseNoCodePoint || size != c->digitLength || mlocParseDigit(sets, zero) != 0)
    {
        return true;
    }
    for (uint32_t d = 1; d < 10; d++)
    {
        const uint8_t* digit = c->digits + (size_t)d * numberDigitSize;
        if (mlocParseCodePoint(digit, c->digitLength, 0, &size) != zero + d ||
            size != c->digitLength)
        {
            return true;
        }
    }
    return false;
}

// The separators and their sets (ICU's DecimalMatcher constructor).
static void SetSeparators(Parser* r)
{
    const NumberCore* c = &r->f->core;
    bool strict = (r->flags & modeStrictSeparators) != 0;
    r->decimalText = c->decimalText;
    r->decimalLength = (uint8_t)c->symbolLength[numberDecimal];
    r->groupText = c->groupText;
    r->groupLength = (uint8_t)c->symbolLength[numberGroup];
    size_t size = 0;
    r->decimalCodePoint = r->decimalLength == 0
                              ? parseNoCodePoint
                              : mlocParseCodePoint(r->decimalText, r->decimalLength, 0, &size);
    r->groupCodePoint = r->groupLength == 0
                            ? parseNoCodePoint
                            : mlocParseCodePoint(r->groupText, r->groupLength, 0, &size);
    uint32_t group =
        mlocParseChooseSet(r->sets, r->groupText, r->groupLength,
                           strict ? parseStrictComma : setAll, strict ? parseStrictPeriod : setAll);
    r->groupSet = group != setOwn ? group
                                  : mlocParseChooseSet(r->sets, r->groupText, r->groupLength,
                                                       parseOtherGrouping, parseOtherGrouping);
    r->decimalSet = mlocParseChooseSet(r->sets, r->decimalText, r->decimalLength,
                                       strict ? parseStrictComma : parseComma,
                                       strict ? parseStrictPeriod : parsePeriod);
    r->digitStrings = NeedsDigitStrings(c, r->sets);
}

// The modes' flags (ICU's createParserFromProperties): grouping is off
// when the formatter does not group, or in scientific notation, whose
// pattern ICU writes without grouping.
static uint32_t ModeFlags(const Formatter* f, mlocParseMode mode)
{
    uint32_t flags = mode == mloc_parseStrict
                         ? modeStrictGrouping | modeStrictSeparators | modeFullAffixes |
                               modeExactAffix | modeStrictIgnorables
                         : modeUnpairedAffixes;
    if (f->grouping == mloc_groupingOff || f->core.primary == 0 ||
        f->notation != mloc_notationStandard)
    {
        flags |= modeGroupingDisabled;
    }
    if (f->signDisplay == mloc_signAlways || f->signDisplay == mloc_signExceptZero)
    {
        flags |= modePlusAllowed;
    }
    return flags;
}

// Builds the list in ICU's order: the affix matchers, in lenient mode the
// percent or per mille sign the pattern has and the signs, NaN, infinity,
// the ignorables, the digits, the exponent, then the validators.
static bool BuildParser(Parser* r, const Formatter* f, const ParseCurrency* currency,
                        mlocParseMode mode)
{
    memset(r, 0, sizeof(*r));
    r->f = f;
    r->currency = currency;
    r->matchCurrency = currency != nullptr ? mlocMatchCurrency : nullptr;
    r->sets = f->core.parseSets;
    r->flags = ModeFlags(f, mode);
    SetSeparators(r);
    if (!FillAffixes(r, 0) || !AddAffixMatchers(r))
    {
        return false;
    }
    bool strict = mode == mloc_parseStrict;
    bool perMille = f->style == mloc_numberPerMille;
    // ICU adds the currency matcher for parseCurrency or a pattern with
    // the currency (a long name's too).
    bool any = currency != nullptr && currency->chain != nullptr;
    if (currency != nullptr && (any || HasCurrency(r)))
    {
        Add(r, matchCurrency, 0);
    }
    if (!strict && PatternHas(r, numberAffixPercent))
    {
        Add(r, perMille ? matchPerMille : matchPercent, 0);
    }
    if (!strict && PatternHas(r, numberAffixPerMille) && !perMille)
    {
        Add(r, matchPerMille, 0);
    }
    if (!strict)
    {
        Add(r, matchPlus, 0);
        Add(r, matchMinus, 0);
        Add(r, matchApproximately, 0);
    }
    Add(r, matchNaN, 0);
    Add(r, matchInfinity, 0);
    Add(r, matchIgnorables, 0);
    Add(r, matchDecimal, 0);
    Add(r, matchScientific, 0);
    Add(r, validateNumber, 0);
    if (strict)
    {
        Add(r, validateAffix, 0);
    }
    if (any)
    {
        Add(r, validateCurrency, 0);
    }
    if (f->style != mloc_numberDecimal && f->unscaled == 0)
    {
        Add(r, validateScale, perMille ? 3 : 2);
    }
    return true;
}

// ICU's parseGreedy: each matcher in turn at the offset, the list started
// again after one reads text, until the text ends or none reads.
static void RunGreedy(const Parser* r, Segment* s, Parsed* p)
{
    for (uint32_t i = 0; i < r->count && s->at < s->length;)
    {
        const Matcher* m = &r->list[i];
        size_t initial = s->at;
        if (m->kind < validateNumber)
        {
            mlocParseMatch(r, m, s, p);
        }
        i = s->at != initial ? 0 : i + 1;
    }
}

// ICU's AffixMatcher::postProcess: when its affixes are the ones matched,
// the missing one is marked empty and its flags are added.
static void AcceptAffix(const Parser* r, const AffixMatcher* a, Parsed* p)
{
    if (mlocParseMatched(r, a->prefix, p->prefix) && mlocParseMatched(r, a->suffix, p->suffix))
    {
        p->prefix = p->prefix == affixNone ? (int8_t)affixEmpty : p->prefix;
        p->suffix = p->suffix == affixNone ? (int8_t)affixEmpty : p->suffix;
        p->flags |= a->flags;
    }
}

// Each matcher's post-processing in list order: the affix matcher whose
// affixes were matched marks them and adds its flags; the validators
// fail the parse or scale it.
static void PostProcess(const Parser* r, Parsed* p, int64_t* scaleOut)
{
    for (uint32_t i = 0; i < r->count; i++)
    {
        const Matcher* m = &r->list[i];
        switch (m->kind)
        {
        case matchAffix:
            AcceptAffix(r, &r->affixes[m->arg], p);
            break;
        case validateNumber:
            p->flags |= p->quantity.present || (p->flags & (parsedNaN | parsedInfinity)) != 0
                            ? 0
                            : parsedFail;
            break;
        case validateCurrency:
            p->flags |= p->hasCode ? 0 : parsedFail;
            break;
        case validateAffix:
            p->flags |= p->prefix == affixNone || p->suffix == affixNone ? parsedFail : 0;
            break;
        case validateScale:
            *scaleOut = p->quantity.present ? m->arg : 0;
            break;
        default:
            break;
        }
    }
}

// The decimal a parse gives: its digits, the point moved by the exponent
// and the scale, the fraction digits typed moved with it; false past the
// decimal's limits.
static bool ToDecimal(const Parsed* p, int64_t scale, mlocDecimal* out)
{
    mlocDecimal d = {0};
    d.negative = (p->flags & parsedNegative) != 0 ? 1 : 0;
    if ((p->flags & parsedNaN) != 0)
    {
        d.kind = mloc_decimalNaN;
        d.negative = 0;
        *out = d;
        return true;
    }
    if ((p->flags & parsedInfinity) != 0)
    {
        d.kind = mloc_decimalInfinity;
        *out = d;
        return true;
    }
    const Quantity* q = &p->quantity;
    int64_t shift = p->exponent - scale;
    int64_t fraction = p->afterPoint - shift;
    int64_t exponent = (int64_t)q->zeros - p->afterPoint + shift;
    if (q->count == 0)
    {
        exponent = 0;
    }
    int64_t needed = exponent < 0 ? -exponent : 0;
    fraction = fraction > needed ? fraction : needed;
    int64_t top = exponent + (q->count == 0 ? 0 : q->count - 1);
    if (p->overflow || exponent < -mlocDecimalMaxExponent || top > mlocDecimalMaxExponent ||
        fraction > mlocDecimalMaxExponent)
    {
        return false;
    }
    memcpy(d.digits, q->digits, q->count);
    d.count = (uint8_t)q->count;
    d.exponent = (int32_t)exponent;
    d.fractionDigits = (int32_t)fraction;
    *out = d;
    return true;
}

mlocResult mlocParseWith(const Formatter* formatter, const ParseCurrency* currency,
                         const char* text, size_t length, mlocParseMode mode,
                         mlocDecimal* decimalOut, char* codeOut, size_t* endOut)
{
    if (formatter->core.parseSets == nullptr)
    {
        return mloc_errorNotFound;
    }
    Parser r;
    if (!BuildParser(&r, formatter, currency, mode))
    {
        return mloc_errorCapacity;
    }
    Segment s = {.text = (const uint8_t*)text, .length = length, .at = 0};
    Parsed p;
    memset(&p, 0, sizeof(p));
    p.prefix = affixNone;
    p.suffix = affixNone;
    RunGreedy(&r, &s, &p);
    int64_t scale = 0;
    PostProcess(&r, &p, &scale);
    *endOut = p.charEnd;
    if (p.charEnd == 0 || (p.flags & parsedFail) != 0)
    {
        return mloc_errorSyntax;
    }
    if (codeOut != nullptr && currency != nullptr)
    {
        memcpy(codeOut, p.hasCode ? p.code : currency->code, 3);
    }
    return ToDecimal(&p, scale, decimalOut) ? mloc_success : mloc_errorCapacity;
}

static bool ValidRanges(const uint8_t* ranges, uint32_t first, uint32_t count)
{
    uint32_t previous = 0;
    for (uint32_t i = first; i < first + count; i++)
    {
        uint32_t low = mlocLoad32(ranges + (size_t)i * parseRangeSize);
        uint32_t high = mlocLoad32(ranges + (size_t)i * parseRangeSize + 4);
        if (low > high || high > 0x10FFFF || (i > first && low <= previous))
        {
            return false;
        }
        previous = high;
    }
    return true;
}

// Runs of a mapping: 1 or more code points 1 or 2 apart, sorted and
// disjoint, each mapping within the code points.
static bool ValidRuns(const uint8_t* folds, uint32_t count)
{
    int64_t previous = -1;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* run = folds + (size_t)i * parseFoldSize;
        int64_t first = mlocLoad32(run);
        int64_t last = first + (int64_t)(mlocLoad16(run + 4) - 1) * run[6];
        int64_t delta = (int32_t)mlocLoad32(run + 8);
        if (mlocLoad16(run + 4) == 0 || run[6] < 1 || run[6] > 2 || run[7] != 0 ||
            first <= previous || last > 0x10FFFF || first + delta < 0 || last + delta > 0x10FFFF)
        {
            return false;
        }
        previous = last;
    }
    return true;
}

// The specials: sorted, each of two or three code points.
static bool ValidSpecials(const uint8_t* specials, uint32_t count)
{
    uint32_t previous = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* e = specials + (size_t)i * parseSpecialSize;
        uint32_t cp = mlocLoad32(e);
        bool ordered = i == 0 || cp > previous;
        if (!ordered || cp > 0x10FFFF || mlocLoad32(e + 4) - 1 > 0x10FFFE ||
            mlocLoad32(e + 8) - 1 > 0x10FFFE || mlocLoad32(e + 12) > 0x10FFFF)
        {
            return false;
        }
        previous = cp;
    }
    return true;
}

bool mlocValidParseSets(const DataView* view, DataSpan payload)
{
    (void)view;
    const uint8_t* b = payload.bytes;
    if (payload.size < parseHeaderSize || mlocLoad16(b) != parseClassCount)
    {
        return false;
    }
    uint32_t ranges = mlocLoad16(b + 2);
    uint32_t strings = mlocLoad16(b + 4);
    uint32_t textSize = mlocLoad16(b + 6);
    uint32_t folds = mlocLoad16(b + 8);
    uint32_t uppers = mlocLoad16(b + 10);
    uint32_t specials = mlocLoad16(b + 12);
    size_t rangeStart = parseHeaderSize + (size_t)parseClassCount * parseClassSize;
    size_t foldStart = rangeStart + (size_t)ranges * parseRangeSize;
    size_t upperStart = foldStart + (size_t)folds * parseFoldSize;
    size_t specialStart = upperStart + (size_t)uppers * parseFoldSize;
    size_t offsetStart = specialStart + (size_t)specials * parseSpecialSize;
    size_t textStart = offsetStart + 2 * ((size_t)strings + 1);
    if (payload.size != textStart + textSize || mlocLoad16(b + 14) != 0 ||
        mlocLoad16(b + offsetStart) != 0 || mlocLoad16(b + textStart - 2) != textSize ||
        !ValidRuns(b + foldStart, folds) || !ValidRuns(b + upperStart, uppers) ||
        !ValidSpecials(b + specialStart, specials))
    {
        return false;
    }
    for (uint32_t i = 0; i < strings; i++)
    {
        if (mlocLoad16(b + offsetStart + 2 * (size_t)i) >
            mlocLoad16(b + offsetStart + 2 * (size_t)i + 2))
        {
            return false;
        }
    }
    for (uint32_t c = 0; c < parseClassCount; c++)
    {
        const uint8_t* entry = b + parseHeaderSize + (size_t)c * parseClassSize;
        uint32_t first = mlocLoad16(entry);
        uint32_t count = mlocLoad16(entry + 2);
        if (first + count > ranges || mlocLoad16(entry + 4) + mlocLoad16(entry + 6) > strings ||
            !ValidRanges(b + rangeStart, first, count))
        {
            return false;
        }
    }
    return true;
}
