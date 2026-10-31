// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A whole locale's name (maul-locale/names.h), by UTS #35's Locale Display
// Name Algorithm as CLDR's NameGetter runs it (the generator of CLDR's
// localeDisplayName.txt): the language's name by the longest match
// (dialect names) or alone (standard names), then the script, region and
// variants it left out, the u keys, the t extension and its fields, and
// the other extensions, joined by the locale's separator and pattern, each
// name's brackets nested.

#include "data_view.h"
#include "invariant.h"
#include "kinds.h"
#include "names_data.h"
#include "names_text.h"

#include "maul-locale/currency.h"
#include "maul-locale/names.h"

#include <string.h>

enum
{
    // The most items of one kind a tail of 112 bytes holds: each takes
    // two bytes or more and a separator.
    maxItems = 40,
    // The longest code looked up.
    maxCode = 160,
};

typedef struct Span
{
    const char* text;
    size_t length;
} Span;

// A pattern split at its placeholders, {0} before {1}.
typedef struct Pattern
{
    Span prefix;
    Span middle;
    Span suffix;
} Pattern;

typedef enum ItemKind
{
    // A name of a script, region or variant (or the subtag itself).
    itemName,
    // A u attribute list, u key or t field with its values.
    itemKeyword,
    // The t extension's language.
    itemTransform,
    // Another extension or private use: a singleton and its subtags.
    itemOther,
} ItemKind;

typedef struct Item
{
    ItemKind kind;
    char key[3];
    // The values, or the subtags, as the tail joins them.
    Span values;
    uint32_t count;
    NameText name;
} Item;

// What a name is made of and how it is written.
typedef struct Composer
{
    const mlocDataSet* data;
    const mlocLocale* display;
    // The display locale's chain, resolved once for the call's lookups.
    const NameChain* chain;
    bool dialect;
    NameField field;
    Pattern separator;
    Pattern keyType;
    NameWriter* w;
    // The h0 field's values, which name the t extension's key.
    Span hybrid;
} Composer;

// A language id's parts.
typedef struct LanguageParts
{
    Span language;
    Span script;
    Span region;
    Span variants;
} LanguageParts;

static Span SpanOf(const char* text)
{
    return (Span){text, strlen(text)};
}

// A locale field, which fills its array without a NUL when it is full.
static Span FieldOf(const char* field, size_t size)
{
    size_t length = 0;
    while (length < size && field[length] != '\0')
    {
        length++;
    }
    return (Span){field, length};
}

static void Append(Composer* c, Span s, bool brackets)
{
    mlocNameAppend(c->w, (const uint8_t*)s.text, s.length, brackets);
}

static void AppendName(Composer* c, NameText n, bool brackets)
{
    mlocNameAppend(c->w, n.text, n.length, brackets);
}

// Splits a pattern at "{0}" and "{1}", or false when it lacks them in
// that order.
static bool Split(NameText text, Pattern* out)
{
    const char* t = (const char*)text.text;
    size_t zero = text.length;
    size_t one = text.length;
    for (size_t i = 0; i + 3 <= text.length; i++)
    {
        if (t[i] == '{' && t[i + 2] == '}' && t[i + 1] == '0' && zero == text.length)
        {
            zero = i;
        }
        else if (t[i] == '{' && t[i + 2] == '}' && t[i + 1] == '1' && one == text.length)
        {
            one = i;
        }
    }
    if (zero == text.length || one == text.length || one < zero + 3)
    {
        return false;
    }
    out->prefix = (Span){t, zero};
    out->middle = (Span){t + zero + 3, one - zero - 3};
    out->suffix = (Span){t + one + 3, text.length - one - 3};
    return true;
}

// A display pattern of the locale, root's when its own cannot be read.
static Pattern FindPattern(const Composer* c, const char* code, const char* fallback)
{
    NameText text;
    Pattern p;
    if (mlocFindChainName(c->chain, code, strlen(code), nameFieldName, &text) && Split(text, &p))
    {
        return p;
    }
    // Every caller passes a literal with "{0}" before "{1}".
    bool split = Split((NameText){(const uint8_t*)fallback, (uint16_t)strlen(fallback)}, &p);
    MLOC_ASSERT(split);
    (void)split;
    return p;
}

// Looks a code up: a namespace and the parts given, joined by '-'.
static bool Find(const Composer* c, char space, const Span* parts, size_t count, NameField field,
                 NameText* out)
{
    char code[2 + maxCode];
    size_t at = 2;
    code[0] = space;
    code[1] = ':';
    for (size_t i = 0; i < count; i++)
    {
        if (parts[i].length == 0)
        {
            continue;
        }
        if (at + 1 + parts[i].length > sizeof(code))
        {
            return false;
        }
        if (at > 2)
        {
            code[at++] = '-';
        }
        memcpy(code + at, parts[i].text, parts[i].length);
        at += parts[i].length;
    }
    return mlocFindChainName(c->chain, code, at, field, out);
}

// Whether a name holds '_' or '-', which sends the whole id's match on
// to the parts (NameGetter's check).
static bool Joined(NameText n)
{
    return memchr(n.text, '_', n.length) != nullptr || memchr(n.text, '-', n.length) != nullptr;
}

// The language's name and which of the script and region it covers; a
// whole match (dialect names, an id with no extensions) covers it all.
typedef struct LanguageName
{
    NameText name;
    bool found;
    bool whole;
    bool script;
    bool region;
} LanguageName;

static LanguageName NameLanguage(const Composer* c, const LanguageParts* p, bool extensions)
{
    LanguageName r = {0};
    if (c->dialect)
    {
        Span all[] = {p->language, p->script, p->region, p->variants};
        Span lsr[] = {p->language, p->script, p->region};
        Span ls[] = {p->language, p->script};
        Span lr[] = {p->language, p->region};
        if (!extensions && Find(c, 'l', all, 4, c->field, &r.name) && !Joined(r.name))
        {
            r.found = r.whole = true;
        }
        else if (Find(c, 'l', lsr, 3, c->field, &r.name))
        {
            r.found = r.script = r.region = true;
        }
        else if (Find(c, 'l', ls, 2, c->field, &r.name))
        {
            r.found = r.script = true;
        }
        else if (Find(c, 'l', lr, 2, c->field, &r.name))
        {
            r.found = r.region = true;
        }
        if (r.found)
        {
            return r;
        }
    }
    r.found = Find(c, 'l', &p->language, 1, c->field, &r.name);
    return r;
}

static size_t SubtagLength(const char* at)
{
    return strcspn(at, "-");
}

// The length of the subtag at at, which ends by end at the latest.
static size_t SubtagIn(const char* at, const char* end)
{
    size_t n = 0;
    while (at + n < end && at[n] != '-')
    {
        n++;
    }
    return n;
}

// The span from one subtag's start to another's end.
static Span Between(const char* start, const char* end)
{
    return (Span){start, (size_t)(end - start)};
}

// Reads the subtags after a key, up to the next key (as isKey says) or
// singleton: its values.
static const char* Values(const char* at, bool (*isKey)(const char*, size_t), Item* item)
{
    const char* first = at;
    const char* last = at;
    item->count = 0;
    while (*at != '\0')
    {
        size_t n = SubtagLength(at);
        if (n == 1 || isKey(at, n))
        {
            break;
        }
        item->count++;
        last = at + n;
        at += n + (at[n] == '-');
    }
    item->values = item->count == 0 ? (Span){first, 0} : Between(first, last);
    return at;
}

static bool UKey(const char* at, size_t n)
{
    (void)at;
    return n == 2;
}

static bool TKey(const char* at, size_t n)
{
    return n == 2 && at[1] >= '0' && at[1] <= '9';
}

static bool NoKey(const char* at, size_t n)
{
    (void)at;
    (void)n;
    return false;
}

// The tail's items in NameGetter's order: the u extension's attributes
// and keys, the t extension's language and fields (h0 kept apart), then
// the other extensions and private use by their singletons.
typedef struct Tail
{
    Span variants;
    Item u[maxItems];
    Item t[maxItems];
    Item other[maxItems];
    size_t uCount;
    size_t tCount;
    size_t otherCount;
    Span hybrid;
} Tail;

// Reads a u or t extension's keys from at, into list.
static const char* Keys(const char* at, bool (*isKey)(const char*, size_t), Item* list,
                        size_t* count, Span* hybrid)
{
    while (*at != '\0' && SubtagLength(at) != 1)
    {
        Item item = {.kind = itemKeyword};
        size_t n = SubtagLength(at);
        memcpy(item.key, at, n < 2 ? n : 2);
        at = Values(at + n + (at[n] == '-'), isKey, &item);
        if (hybrid != nullptr && strcmp(item.key, "h0") == 0)
        {
            *hybrid = item.values;
        }
        else if (*count < maxItems)
        {
            list[(*count)++] = item;
        }
    }
    return at;
}

// Reads one extension, its singleton at at; returns past it.
static const char* Extension(const char* at, Tail* t)
{
    char singleton = *at;
    at += 1 + (at[1] == '-');
    Item item = {.kind = itemOther, .key = {singleton}};
    if (singleton == 'u' || singleton == 't')
    {
        bool u = singleton == 'u';
        Item* list = u ? t->u : t->t;
        size_t* count = u ? &t->uCount : &t->tCount;
        at = Values(at, u ? UKey : TKey, &item);
        if (item.count > 0 && *count < maxItems)
        {
            item.kind = u ? itemKeyword : itemTransform;
            list[(*count)++] = item;
        }
        return Keys(at, u ? UKey : TKey, list, count, u ? nullptr : &t->hybrid);
    }
    if (singleton == 'x')
    {
        item.values = SpanOf(at);
        item.count = 1;
        at += item.values.length;
    }
    else
    {
        at = Values(at, NoKey, &item);
    }
    // Private use goes among the singletons by its letter.
    if (t->otherCount == maxItems)
    {
        return at;
    }
    size_t place = t->otherCount;
    while (place > 0 && t->other[place - 1].key[0] > singleton)
    {
        t->other[place] = t->other[place - 1];
        place--;
    }
    t->other[place] = item;
    t->otherCount++;
    return at;
}

static void ReadTail(const char* tail, Tail* t)
{
    const char* at = tail;
    const char* last = tail;
    while (*at != '\0' && SubtagLength(at) != 1)
    {
        size_t n = SubtagLength(at);
        last = at + n;
        at += n + (at[n] == '-');
    }
    t->variants = Between(tail, last);
    while (*at != '\0')
    {
        at = Extension(at, t);
    }
    // Keys and Extension keep each list within maxItems, which
    // WriteLocale's order is sized for.
    MLOC_ASSERT(t->uCount <= maxItems && t->tCount <= maxItems && t->otherCount <= maxItems);
}

// Writes a key and value by the locale's key-type pattern, every bracket
// nested; the value is written by the caller between the two halves.
static void KeyTypeStart(Composer* c, NameText keyName, Span keyText)
{
    Append(c, c->keyType.prefix, true);
    if (keyName.text != nullptr)
    {
        AppendName(c, keyName, true);
    }
    else
    {
        Append(c, keyText, true);
    }
    Append(c, c->keyType.middle, true);
}

// Writes the items of a list joined by the separator, each by write.
static void JoinSpans(Composer* c, Span values, void (*write)(Composer*, Span))
{
    const char* end = values.text + values.length;
    size_t count = 0;
    for (const char* at = values.text; at < end; count++)
    {
        at += SubtagIn(at, end) + 1;
    }
    for (size_t i = 1; i < count; i++)
    {
        Append(c, c->separator.prefix, true);
    }
    size_t index = 0;
    for (const char* at = values.text; at < end; index++)
    {
        size_t n = SubtagIn(at, end);
        if (index > 0)
        {
            Append(c, c->separator.middle, true);
        }
        write(c, (Span){at, n});
        if (index > 0)
        {
            Append(c, c->separator.suffix, true);
        }
        at += n + 1;
    }
}

static char Upper(char letter)
{
    return (char)(letter >= 'a' && letter <= 'z' ? letter - 32 : letter);
}

// A collation reorder code's name: a script's, else the kr value's, else
// the code.
static void WriteReorder(Composer* c, Span code)
{
    NameText name;
    char script[4];
    bool four = code.length == 4;
    if (four)
    {
        memcpy(script, code.text, 4);
        script[0] = Upper(script[0]);
    }
    Span parts[] = {SpanOf("kr"), code};
    Span scriptCode = {script, 4};
    if ((four && Find(c, 's', &scriptCode, 1, nameFieldName, &name)) ||
        Find(c, 't', parts, 2, nameFieldName, &name))
    {
        AppendName(c, name, true);
        return;
    }
    Append(c, code, true);
}

// A currency's symbol in the locale, else its code in capitals (CLDR's
// tools look the symbol up by the code in capitals, which is its own
// fallback, whatever its length).
static void WriteCurrency(Composer* c, Span code)
{
    char upper[3];
    char symbol[64];
    mlocOutput output = {.buffer = symbol, .capacity = sizeof(symbol)};
    for (size_t i = 0; code.length == 3 && i < 3; i++)
    {
        upper[i] = Upper(code.text[i]);
    }
    mlocResult r = code.length == 3 ? mlocGetCurrencySymbol(c->data, c->display, upper, 3,
                                                            mloc_currencySymbol, &output)
                                    : mloc_errorInvalid;
    if (r == mloc_success || r == mloc_fellBack)
    {
        Append(c, (Span){symbol, output.length}, true);
        return;
    }
    for (size_t i = 0; i < code.length; i++)
    {
        char letter = Upper(code.text[i]);
        Append(c, (Span){&letter, 1}, true);
    }
}

// A subdivision's name (subdivisions.mloc), else its code.
static void WriteSubdivision(Composer* c, Span code)
{
    NameText name;
    if (Find(c, 'd', &code, 1, nameFieldName, &name))
    {
        AppendName(c, name, true);
        return;
    }
    Append(c, code, true);
}

// A u key or t field: its value's name when the locale has one (the
// calendar's whatever its subtags, another key's for one subtag), else the
// key's name and the value by the key-type pattern: the currency's symbol
// for cu, the reorder codes' names for kr, the subdivision's for sd and
// rg, the subtags for the rest (time zones as written until their names
// are in the data).
static void WriteKeyword(Composer* c, const Item* item)
{
    NameText name;
    Span key = {item->key, strlen(item->key)};
    // A key without a value has the value "true" (UTS #35, Unicode
    // locale identifier), which canonical syntax leaves out.
    Item withTrue = *item;
    if (item->count == 0)
    {
        withTrue.values = SpanOf("true");
        withTrue.count = 1;
        item = &withTrue;
    }
    Span parts[] = {key, item->values};
    bool calendar = strcmp(item->key, "ca") == 0;
    if ((item->count == 1 || (calendar && item->count > 0)) &&
        Find(c, 't', parts, 2, nameFieldName, &name))
    {
        AppendName(c, name, true);
        return;
    }
    NameText keyName = {0};
    (void)Find(c, 'k', &key, 1, nameFieldName, &keyName);
    KeyTypeStart(c, keyName, key);
    if (strcmp(item->key, "cu") == 0)
    {
        WriteCurrency(c, item->values);
    }
    else if (strcmp(item->key, "kr") == 0)
    {
        JoinSpans(c, item->values, WriteReorder);
    }
    else if (strcmp(item->key, "sd") == 0 || strcmp(item->key, "rg") == 0)
    {
        WriteSubdivision(c, item->values);
    }
    else
    {
        Append(c, item->values, true);
    }
    Append(c, c->keyType.suffix, true);
}

static mlocResult WriteLocale(Composer* c, const mlocLocale* locale, const Tail* tail,
                              const Pattern* pattern, bool inner);

// The t extension: its language's name, its parts joined by the
// separator, after the transform's name (h0's value's name when it has
// one).
static void WriteTransform(Composer* c, const Item* item)
{
    NameText keyName = {0};
    Span key = SpanOf("t");
    Span hybrid[] = {SpanOf("h0"), c->hybrid};
    if (c->hybrid.length == 0 || !Find(c, 't', hybrid, 2, nameFieldName, &keyName))
    {
        (void)Find(c, 'k', &key, 1, nameFieldName, &keyName);
    }
    KeyTypeStart(c, keyName, key);
    mlocLocale language;
    // The language's variants are its tail; it has no extensions.
    static const Tail s_none;
    Tail variants = s_none;
    if (item->values.length < maxCode &&
        mlocParseLocale(item->values.text, item->values.length, &language).status == mloc_success)
    {
        variants.variants = FieldOf(language.tail, sizeof(language.tail));
        (void)WriteLocale(c, &language, &variants, &c->separator, true);
    }
    else
    {
        Append(c, item->values, true);
    }
    Append(c, c->keyType.suffix, true);
}

static void WriteItem(Composer* c, const Item* item)
{
    switch (item->kind)
    {
    case itemName:
        AppendName(c, item->name, true);
        break;
    case itemKeyword:
        WriteKeyword(c, item);
        break;
    case itemTransform:
        WriteTransform(c, item);
        break;
    default:
        Append(c, c->keyType.prefix, false);
        Append(c, (Span){item->key, 1}, false);
        Append(c, c->keyType.middle, false);
        Append(c, item->values, false);
        Append(c, c->keyType.suffix, false);
        break;
    }
}

// A script's, region's or variant's name item: its name, or the subtag.
static Item NameItem(const Composer* c, char space, Span subtag)
{
    Item item = {.kind = itemName};
    if (!Find(c, space, &subtag, 1, c->field, &item.name))
    {
        item.name = (NameText){(const uint8_t*)subtag.text, (uint16_t)subtag.length};
    }
    return item;
}

// Writes a locale's name: the language's, then by the pattern the names
// of the parts it left out and the extensions, joined by the separator.
// Inside a transform (inner) the patterns' brackets are nested too.
static mlocResult WriteLocale(Composer* c, const mlocLocale* locale, const Tail* tail,
                              const Pattern* pattern, bool inner)
{
    LanguageParts p = {FieldOf(locale->language, sizeof(locale->language)),
                       FieldOf(locale->script, sizeof(locale->script)),
                       FieldOf(locale->region, sizeof(locale->region)), tail->variants};
    bool extensions = tail->uCount + tail->tCount + tail->otherCount > 0 || tail->hybrid.length > 0;
    LanguageName language = NameLanguage(c, &p, extensions);
    Item names[2 + maxItems];
    const Item* order[2 + 4 * maxItems];
    size_t count = 0;
    if (!language.whole && !language.script && p.script.length > 0)
    {
        names[count] = NameItem(c, 's', p.script);
        order[count] = &names[count];
        count++;
    }
    if (!language.whole && !language.region && p.region.length > 0)
    {
        names[count] = NameItem(c, 'r', p.region);
        order[count] = &names[count];
        count++;
    }
    const char* end = p.variants.text + p.variants.length;
    for (const char* at = p.variants.text; !language.whole && count < 2 + maxItems && at < end;)
    {
        size_t n = SubtagIn(at, end);
        names[count] = NameItem(c, 'v', (Span){at, n});
        order[count] = &names[count];
        count++;
        at += n + 1;
    }
    const Item* lists[] = {tail->u, tail->t, tail->other};
    size_t sizes[] = {tail->uCount, tail->tCount, tail->otherCount};
    for (size_t l = 0; !language.whole && l < 3; l++)
    {
        for (size_t i = 0; i < sizes[l]; i++)
        {
            order[count++] = &lists[l][i];
        }
    }
    if (count > 0)
    {
        Append(c, pattern->prefix, inner);
    }
    if (language.found)
    {
        AppendName(c, language.name, true);
    }
    else
    {
        Append(c, p.language, true);
    }
    if (count == 0)
    {
        return language.found ? mloc_success : mloc_fellBack;
    }
    Append(c, pattern->middle, inner);
    for (size_t i = 1; i < count; i++)
    {
        Append(c, c->separator.prefix, inner);
    }
    for (size_t i = 0; i < count; i++)
    {
        if (i > 0)
        {
            Append(c, c->separator.middle, inner);
        }
        WriteItem(c, order[i]);
        if (i > 0)
        {
            Append(c, c->separator.suffix, inner);
        }
    }
    Append(c, pattern->suffix, inner);
    return language.found ? mloc_success : mloc_fellBack;
}

typedef struct LocaleNameWriting
{
    const Composer* c;
    const mlocLocale* locale;
    const Tail* tail;
    const Pattern* pattern;
    bool capitalize;
} LocaleNameWriting;

// Composes a locale's name into a writer, each pass from a copy of the
// composer.
static mlocResult WriteLocaleName(const void* context, NameWriter* w)
{
    const LocaleNameWriting* g = context;
    Composer c = *g->c;
    c.w = w;
    mlocResult r = WriteLocale(&c, g->locale, g->tail, g->pattern, false);
    if (g->capitalize)
    {
        mlocNameCapitalize(c.data, c.display, w);
    }
    return r;
}

mlocResult mlocGetLocaleName(const mlocDataSet* data, const mlocLocale* displayLocale,
                             const mlocLocale* locale, const mlocLocaleNameOptions* options,
                             mlocOutput* textOut)
{
    mlocLocaleNameOptions o = options != nullptr ? *options : (mlocLocaleNameOptions){0};
    if (data == nullptr || displayLocale == nullptr || locale == nullptr || textOut == nullptr ||
        textOut->part != nullptr || (textOut->buffer == nullptr && textOut->capacity != 0) ||
        (uint32_t)o.languageDisplay > mloc_languageDisplayStandard ||
        (uint32_t)o.form >= nameFieldCount ||
        (uint32_t)o.capitalization > mloc_capitalizationStandAlone)
    {
        return mloc_errorInvalid;
    }
    if (!mlocDataSetHasKind(data, kindNameIds))
    {
        return mloc_errorNotFound;
    }
    // A chain that cannot be walked past its first step keeps that step,
    // where mlocFindName would find names too.
    NameChain chain;
    (void)mlocOpenNameChain(data, displayLocale, &chain);
    Composer c = {
        .data = data,
        .display = displayLocale,
        .chain = &chain,
        .dialect = o.languageDisplay == mloc_languageDisplayDialect,
        .field = (NameField)o.form,
    };
    c.separator = FindPattern(&c, "p:localeSeparator", "{0}, {1}");
    c.keyType = FindPattern(&c, "p:localeKeyTypePattern", "{0}: {1}");
    Pattern pattern = FindPattern(&c, "p:localePattern", "{0} ({1})");
    // The tail is read from a copy that ends in a NUL whatever the
    // caller's value holds.
    char text[sizeof(locale->tail) + 1];
    Span field = FieldOf(locale->tail, sizeof(locale->tail));
    memcpy(text, field.text, field.length);
    text[field.length] = '\0';
    Tail tail = {0};
    ReadTail(text, &tail);
    c.hybrid = tail.hybrid;
    LocaleNameWriting g = {
        &c, locale, &tail, &pattern,
        mlocNameCapitalizes(data, displayLocale, contextLanguages, o.capitalization)};
    return mlocWriteName(WriteLocaleName, &g, textOut);
}
