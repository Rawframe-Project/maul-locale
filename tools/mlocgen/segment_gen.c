// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmentation data file: each file of CLDR's common/segments but
// root's, whose rules are Maul Unicode's, as a unit of its locale key. A
// segmentation's variables and rules are not compiled into rules of their
// own: the generator recognizes the forms CLDR writes and turns each into
// what Maul Unicode takes, refusing any other form with the file and the
// reason, so that a new CLDR release shows what to add.
//
// - A class's variable with characters added, "[[$STerm] [;
//   ;]]", gives them that class; with characters taken out,
//   "[[$MidNumLet]-[.]]", requires that another variable adds them. A
//   segmentation's overrides are a set of the code table
//   src/generated/segment_overrides.c, which its unit names.
// - The line variables "$ID = [[\p{Line_Break=Ideographic}] [$CJ]]" and
//   "$NS = \p{Line_Break=Nonstarter}" are CJ as an ideograph.
// - Japanese's word variables and rules 13.3 and 13.4 are the Japanese
//   word rules.
// - A sentence segmentation's standard suppressions become the two tables
//   ICU4C's FilteredBreakIterator builder makes of them
//   (segment_tables.h).

#include "segment_gen.h"

#include "keys.h"
#include "kinds.h"
#include "segment_tables.h"
#include "table_gen.h"
#include "xml.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxText = 256,
    maxOverrides = segmentGenMaxSetOverrides,
};

typedef struct Override
{
    uint32_t codePoint;
    uint32_t value;
} Override;

// What one segmentation gives.
typedef struct Overrides
{
    Override added[maxOverrides];
    int addedCount;
    uint32_t removed[maxOverrides];
    int removedCount;
} Overrides;

// A suppression table's entry.
typedef struct Entry
{
    char* text;
    uint32_t value;
} Entry;

typedef struct Segments
{
    uint32_t given;
    uint32_t flags;
    Overrides word;
    Overrides sentence;
    Entry* backward;
    int backwardCount;
    Entry* forward;
    int forwardCount;
} Segments;

// Each class's variable and Maul Unicode's name of the class.
static const char* const s_wordClasses[][2] = {
    {"$CR", "muni_wbCr"},
    {"$LF", "muni_wbLf"},
    {"$Newline", "muni_wbNewline"},
    {"$Extend", "muni_wbExtend"},
    {"$ZWJ", "muni_wbZwj"},
    {"$RI", "muni_wbRegionalIndicator"},
    {"$Format", "muni_wbFormat"},
    {"$Katakana", "muni_wbKatakana"},
    {"$Hebrew_Letter", "muni_wbHebrewLetter"},
    {"$ALetter", "muni_wbALetter"},
    {"$Single_Quote", "muni_wbSingleQuote"},
    {"$Double_Quote", "muni_wbDoubleQuote"},
    {"$MidNumLet", "muni_wbMidNumLet"},
    {"$MidLetter", "muni_wbMidLetter"},
    {"$MidNum", "muni_wbMidNum"},
    {"$Numeric", "muni_wbNumeric"},
    {"$ExtendNumLet", "muni_wbExtendNumLet"},
    {"$WSegSpace", "muni_wbWSegSpace"},
    {"$XX", "muni_wbOther"},
};

static const char* const s_sentenceClasses[][2] = {
    {"$CR", "muni_sbCr"},           {"$LF", "muni_sbLf"},
    {"$Extend", "muni_sbExtend"},   {"$Sep", "muni_sbSep"},
    {"$Format", "muni_sbFormat"},   {"$Sp", "muni_sbSp"},
    {"$Lower", "muni_sbLower"},     {"$Upper", "muni_sbUpper"},
    {"$OLetter", "muni_sbOLetter"}, {"$Numeric", "muni_sbNumeric"},
    {"$ATerm", "muni_sbATerm"},     {"$SContinue", "muni_sbSContinue"},
    {"$STerm", "muni_sbSTerm"},     {"$Close", "muni_sbClose"},
    {"$XX", "muni_sbOther"},
};

// Japanese's word variables and rules, whitespace collapsed.
static const char* const s_japaneseVariables[][2] = {
    {"$Hiragana", "\\p{Hiragana}"},
    {"$Hiragana", "($Hiragana $FE*)"},
    {"$Ideographic", "[[\\p{Ideographic}] [\\u3005 \\u3007 \\u303B]]"},
    {"$Ideographic", "($Ideographic $FE*)"},
};
static const char* const s_japaneseRules[][2] = {
    {"13.3", "$Hiragana × $Hiragana"},
    {"13.4", "$Ideographic × $Ideographic"},
};

// The line variables of CJ as an ideograph.
static const char* const s_cjVariables[][2] = {
    {"$ID", "[[\\p{Line_Break=Ideographic}] [$CJ]]"},
    {"$NS", "\\p{Line_Break=Nonstarter}"},
};

static int Fail(char* errorOut, size_t errorCapacity, const char* path, const char* reason,
                const char* detail)
{
    snprintf(errorOut, errorCapacity, "%s: %s: %s", path, reason, detail);
    return 0;
}

// A text with its whitespace runs made one space and its ends trimmed;
// false when it does not fit.
static bool Collapse(const char* text, char* out, size_t capacity)
{
    size_t n = 0;
    bool space = false;
    for (const char* p = text; *p != '\0'; p++)
    {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        {
            space = n > 0;
            continue;
        }
        if (n + (space ? 2 : 1) >= capacity)
        {
            return false;
        }
        if (space)
        {
            out[n++] = ' ';
            space = false;
        }
        out[n++] = *p;
    }
    out[n] = '\0';
    return true;
}

static int ClassOf(const char* const (*names)[2], int count, const char* id)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(names[i][0], id) == 0)
        {
            return i;
        }
    }
    return -1;
}

// One code point written as a character or \uXXXX, \UXXXXXXXX or \x{X},
// the whole token; false for anything else and for a surrogate.
static bool ReadCodePoint(const char* token, size_t length, uint32_t* out)
{
    char digits[16];
    const char* hex = nullptr;
    size_t hexLength = 0;
    if (length == 6 && token[0] == '\\' && token[1] == 'u')
    {
        hex = token + 2;
        hexLength = 4;
    }
    else if (length == 10 && token[0] == '\\' && token[1] == 'U')
    {
        hex = token + 2;
        hexLength = 8;
    }
    else if (length > 4 && length < 12 && memcmp(token, "\\x{", 3) == 0 && token[length - 1] == '}')
    {
        hex = token + 3;
        hexLength = length - 4;
    }
    if (hex != nullptr)
    {
        memcpy(digits, hex, hexLength);
        digits[hexLength] = '\0';
        char* end = nullptr;
        unsigned long value = strtoul(digits, &end, 16);
        *out = (uint32_t)value;
        return *end == '\0' && value <= 0x10FFFF && (value < 0xD800 || value > 0xDFFF);
    }
    const unsigned char* t = (const unsigned char*)token;
    int n = token[0] == '\\' ? 0 : GenCharLength(t);
    if (n <= 0 || (size_t)n != length)
    {
        return false;
    }
    static const uint8_t s_lead[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t c = t[0] & s_lead[n];
    for (int i = 1; i < n; i++)
    {
        c = (c << 6) | (t[i] & 0x3Fu);
    }
    *out = c;
    return c < 0xD800 || c > 0xDFFF;
}

// The code points of a set of characters "[a é \x{1F600}]".
static bool ReadCharacters(const char* set, uint32_t* out, int* countOut)
{
    size_t length = strlen(set);
    if (length < 2 || set[0] != '[' || set[length - 1] != ']')
    {
        return false;
    }
    int count = 0;
    for (size_t at = 1; at + 1 < length;)
    {
        if (set[at] == ' ')
        {
            at++;
            continue;
        }
        size_t end = at;
        while (end + 1 < length && set[end] != ' ')
        {
            end++;
        }
        if (count == maxOverrides || !ReadCodePoint(set + at, end - at, &out[count]))
        {
            return false;
        }
        count++;
        at = end;
    }
    *countOut = count;
    return count > 0;
}

// A class's variable: "[[$X] [chars]]" adds to it, "[[$X]-[chars]]"
// takes out of it.
static bool ReadClassVariable(const char* id, const char* text, uint32_t value,
                              Overrides* overrides)
{
    char added[maxText];
    char taken[maxText];
    snprintf(added, sizeof(added), "[[%s] ", id);
    snprintf(taken, sizeof(taken), "[[%s]-", id);
    size_t length = strlen(text);
    bool add = strncmp(text, added, strlen(added)) == 0;
    bool take = strncmp(text, taken, strlen(taken)) == 0;
    if ((!add && !take) || length < 2 || text[length - 1] != ']')
    {
        return false;
    }
    char set[maxText];
    size_t start = strlen(add ? added : taken);
    snprintf(set, sizeof(set), "%.*s", (int)(length - 1 - start), text + start);
    uint32_t codePoints[maxOverrides];
    int count = 0;
    if (!ReadCharacters(set, codePoints, &count))
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        if (add && overrides->addedCount < maxOverrides)
        {
            overrides->added[overrides->addedCount++] = (Override){codePoints[i], value};
        }
        else if (take && overrides->removedCount < maxOverrides)
        {
            overrides->removed[overrides->removedCount++] = codePoints[i];
        }
        else
        {
            return false;
        }
    }
    return true;
}

// The bit of the pair an id and text are, or 0.
static uint32_t MatchOf(const char* const (*pairs)[2], size_t count, const char* id,
                        const char* text)
{
    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(pairs[i][0], id) == 0 && strcmp(pairs[i][1], text) == 0)
        {
            return 1u << i;
        }
    }
    return 0;
}

static int CompareOverrides(const void* a, const void* b)
{
    uint32_t x = ((const Override*)a)->codePoint;
    uint32_t y = ((const Override*)b)->codePoint;
    return (x > y) - (x < y);
}

// Sorts the added overrides, refusing a code point given twice or taken
// out of a class and given none.
static const char* Settle(Overrides* overrides)
{
    if (overrides->addedCount > 0)
    {
        qsort(overrides->added, (size_t)overrides->addedCount, sizeof(Override), CompareOverrides);
    }
    for (int i = 1; i < overrides->addedCount; i++)
    {
        if (overrides->added[i].codePoint == overrides->added[i - 1].codePoint)
        {
            return "a character given two classes";
        }
    }
    for (int r = 0; r < overrides->removedCount; r++)
    {
        bool given = false;
        for (int i = 0; i < overrides->addedCount; i++)
        {
            given = given || overrides->added[i].codePoint == overrides->removed[r];
        }
        if (!given)
        {
            return "a character taken out of a class and given no other";
        }
    }
    return nullptr;
}

// A word or sentence segmentation's variables and rules.
static const char* ReadClasses(const XmlNode* segmentation, bool word, Segments* out)
{
    Overrides* overrides = word ? &out->word : &out->sentence;
    const char* const(*names)[2] = word ? s_wordClasses : s_sentenceClasses;
    int classCount = word ? (int)(sizeof(s_wordClasses) / sizeof(s_wordClasses[0]))
                          : (int)(sizeof(s_sentenceClasses) / sizeof(s_sentenceClasses[0]));
    // The Japanese variables' bits, then the rules'.
    uint32_t japanese = 0;
    for (int c = 0; c < segmentation->childCount; c++)
    {
        const XmlNode* part = segmentation->children[c];
        bool variables = strcmp(part->name, "variables") == 0;
        bool rules = strcmp(part->name, "segmentRules") == 0;
        if (!variables && !rules)
        {
            continue;
        }
        for (int i = 0; i < part->childCount; i++)
        {
            const XmlNode* item = part->children[i];
            const char* id = XmlGetAttribute(item, "id");
            char text[maxText];
            if (id == nullptr)
            {
                return "a variable or rule without an id";
            }
            if (!Collapse(item->text, text, sizeof(text)))
            {
                return "a variable or rule longer than the generator reads";
            }
            uint32_t match = 0;
            if (word)
            {
                match = rules ? MatchOf(s_japaneseRules, 2, id, text) << 4
                              : MatchOf(s_japaneseVariables, 4, id, text);
            }
            if (match != 0)
            {
                japanese |= match;
                continue;
            }
            int value = ClassOf(names, classCount, id);
            if (rules || value < 0 || !ReadClassVariable(id, text, (uint32_t)value, overrides))
            {
                return rules ? "a rule the generator does not take"
                             : "a variable the generator does not take";
            }
        }
    }
    if (japanese != 0 && japanese != 0x3F)
    {
        return "Japanese's word rules in part";
    }
    out->flags |= japanese != 0 ? segmentJapaneseWords : 0;
    return Settle(overrides);
}

// A line segmentation: CJ as an ideograph, or nothing at all.
static const char* ReadLine(const XmlNode* segmentation, Segments* out)
{
    uint32_t cj = 0;
    for (int c = 0; c < segmentation->childCount; c++)
    {
        const XmlNode* part = segmentation->children[c];
        if (strcmp(part->name, "segmentRules") == 0 && part->childCount > 0)
        {
            return "a line rule the generator does not take";
        }
        if (strcmp(part->name, "variables") != 0)
        {
            continue;
        }
        for (int i = 0; i < part->childCount; i++)
        {
            const char* id = XmlGetAttribute(part->children[i], "id");
            char text[maxText];
            if (!Collapse(part->children[i]->text, text, sizeof(text)))
            {
                return "a variable or rule longer than the generator reads";
            }
            uint32_t match = id == nullptr ? 0 : MatchOf(s_cjVariables, 2, id, text);
            if (match == 0)
            {
                return "a line variable the generator does not take";
            }
            cj |= match;
        }
    }
    if (cj != 0 && cj != 3)
    {
        return "CJ as an ideograph in part";
    }
    out->flags |= cj != 0 ? segmentCjIdeographic : 0;
    return nullptr;
}

// Whether a's bytes read from the end order before b's.
static int CompareBackwards(const void* a, const void* b)
{
    const char* x = ((const Entry*)a)->text;
    const char* y = ((const Entry*)b)->text;
    size_t xl = strlen(x);
    size_t yl = strlen(y);
    for (size_t i = 1; i <= xl && i <= yl; i++)
    {
        unsigned char p = (unsigned char)x[xl - i];
        unsigned char q = (unsigned char)y[yl - i];
        if (p != q)
        {
            return p < q ? -1 : 1;
        }
    }
    return (xl > yl) - (xl < yl);
}

static int CompareForwards(const void* a, const void* b)
{
    return strcmp(((const Entry*)a)->text, ((const Entry*)b)->text);
}

static char* Copy(const char* text, size_t length)
{
    char* copy = GenAllocate(length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

// The length of a text through its first full stop, or 0 without one.
static size_t ThroughStop(const char* text)
{
    const char* stop = strchr(text, '.');
    return stop == nullptr ? 0 : (size_t)(stop - text) + 1;
}

// Whether a suppression is partial: a full stop before its end.
static bool Partial(const char* text)
{
    size_t through = ThroughStop(text);
    return through != 0 && text[through] != '\0';
}

// Sorts a table and keeps each entry once.
static int Settled(Entry* entries, int count, int (*compare)(const void*, const void*))
{
    if (count > 0)
    {
        qsort(entries, (size_t)count, sizeof(Entry), compare);
    }
    int kept = 0;
    for (int i = 0; i < count; i++)
    {
        if (kept > 0 && strcmp(entries[kept - 1].text, entries[i].text) == 0)
        {
            free(entries[i].text);
            continue;
        }
        entries[kept++] = entries[i];
    }
    return kept;
}

// A sentence segmentation's standard suppressions, as ICU4C's
// SimpleFilteredBreakIteratorBuilder::build makes its tables of them: a
// suppression is a forward entry when a partial suppression begins with
// its text through its first full stop, else a backward match; and each
// partial one's text through its first full stop is a backward partial.
static const char* ReadSuppressions(const XmlNode* segmentation, Segments* out)
{
    const XmlNode* list = XmlFindChild(segmentation, "suppressions");
    if (list == nullptr)
    {
        return nullptr;
    }
    const char* type = XmlGetAttribute(list, "type");
    if (type == nullptr || strcmp(type, "standard") != 0)
    {
        return "suppressions of a type other than standard";
    }
    int count = list->childCount;
    for (int i = 0; i < count; i++)
    {
        const XmlNode* item = list->children[i];
        size_t length = strlen(item->text);
        if (strcmp(item->name, "suppression") != 0 || length == 0 || length > segmentMaxSuppression)
        {
            return "a suppression empty or longer than a segmenter looks back";
        }
    }
    out->backward = GenAllocate(sizeof(Entry) * (size_t)(2 * count + 1));
    out->forward = GenAllocate(sizeof(Entry) * (size_t)(count + 1));
    for (int i = 0; i < count; i++)
    {
        const char* text = list->children[i]->text;
        size_t through = ThroughStop(text);
        bool family = false;
        for (int j = 0; through != 0 && j < count; j++)
        {
            const char* other = list->children[j]->text;
            family = family || (Partial(other) && ThroughStop(other) == through &&
                                memcmp(other, text, through) == 0);
        }
        if (family)
        {
            out->forward[out->forwardCount++] = (Entry){Copy(text, strlen(text)), 0};
        }
        else
        {
            out->backward[out->backwardCount++] =
                (Entry){Copy(text, strlen(text)), segmentSuppressionMatch};
        }
        if (Partial(text))
        {
            out->backward[out->backwardCount++] =
                (Entry){Copy(text, through), segmentSuppressionPartial};
        }
    }
    // A family's partial entries are one text, and a match is never one:
    // a suppression that is a family's text belongs to it.
    out->backwardCount = Settled(out->backward, out->backwardCount, CompareBackwards);
    out->forwardCount = Settled(out->forward, out->forwardCount, CompareForwards);
    return out->backwardCount > segmentMaxSuppressions || out->forwardCount > segmentMaxSuppressions
               ? "more suppressions than the format holds"
               : nullptr;
}

static const char* ReadFile(const XmlNode* root, Segments* out)
{
    const XmlNode* segmentations = XmlFindChild(root, "segmentations");
    for (int i = 0; segmentations != nullptr && i < segmentations->childCount; i++)
    {
        const XmlNode* segmentation = segmentations->children[i];
        const char* type = XmlGetAttribute(segmentation, "type");
        const char* problem = nullptr;
        if (strcmp(segmentation->name, "segmentation") != 0 || type == nullptr)
        {
            problem = "a segmentation without a type";
        }
        else if (strcmp(type, "WordBreak") == 0)
        {
            out->given |= segmentGivesWord;
            problem = ReadClasses(segmentation, true, out);
        }
        else if (strcmp(type, "SentenceBreak") == 0)
        {
            out->given |= segmentGivesSentence;
            problem = ReadClasses(segmentation, false, out);
            problem = problem != nullptr ? problem : ReadSuppressions(segmentation, out);
        }
        else if (strcmp(type, "LineBreak") == 0)
        {
            out->given |= segmentGivesLine;
            problem = ReadLine(segmentation, out);
        }
        else
        {
            problem = "a segmentation the generator does not take";
        }
        if (problem != nullptr)
        {
            return problem;
        }
    }
    return nullptr;
}

// A type's overrides as a set of the code table: 0 for none, else the
// set's number plus one; -1 when the table is full.
static int64_t SetOf(SegmentGenSets* sets, const Overrides* overrides, uint32_t given)
{
    if (overrides->addedCount == 0)
    {
        return 0;
    }
    bool word = given == segmentGivesWord;
    SegmentGenSet set = {.given = given, .count = overrides->addedCount};
    for (int i = 0; i < overrides->addedCount; i++)
    {
        set.codePoints[i] = overrides->added[i].codePoint;
        set.classes[i] = word ? s_wordClasses[overrides->added[i].value][1]
                              : s_sentenceClasses[overrides->added[i].value][1];
    }
    for (int k = 0; k < sets->count; k++)
    {
        const SegmentGenSet* old = &sets->sets[k];
        bool same = old->given == given && old->count == set.count;
        for (int i = 0; same && i < set.count; i++)
        {
            same = old->codePoints[i] == set.codePoints[i] && old->classes[i] == set.classes[i];
        }
        if (same)
        {
            return k + 1;
        }
    }
    if (sets->count == segmentGenMaxSets)
    {
        return -1;
    }
    sets->sets[sets->count++] = set;
    return sets->count;
}

static uint8_t* Unit(const Segments* s, uint32_t words, uint32_t sentences, size_t* sizeOut)
{
    size_t text = 0;
    for (int i = 0; i < s->backwardCount; i++)
    {
        text += strlen(s->backward[i].text);
    }
    for (int i = 0; i < s->forwardCount; i++)
    {
        text += strlen(s->forward[i].text);
    }
    size_t entries = (size_t)s->backwardCount + (size_t)s->forwardCount;
    size_t size = segmentHeaderSize + 4 * (size_t)s->backwardCount + 4 * (entries + 1) + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, s->given, 4);
    GenPut(unit + 4, s->flags, 4);
    GenPut(unit + 8, words, 4);
    GenPut(unit + 12, sentences, 4);
    GenPut(unit + 16, (uint64_t)s->backwardCount, 4);
    GenPut(unit + 20, (uint64_t)s->forwardCount, 4);
    GenPut(unit + 24, text, 4);
    uint8_t* at = unit + segmentHeaderSize;
    for (int i = 0; i < s->backwardCount; i++)
    {
        GenPut(at + 4 * (size_t)i, s->backward[i].value, 4);
    }
    at += 4 * (size_t)s->backwardCount;
    uint8_t* textAt = at + 4 * (entries + 1);
    size_t offset = 0;
    for (size_t i = 0; i < entries; i++)
    {
        const char* entry = i < (size_t)s->backwardCount
                                ? s->backward[i].text
                                : s->forward[i - (size_t)s->backwardCount].text;
        size_t length = strlen(entry);
        GenPut(at + 4 * i, offset, 4);
        memcpy(textAt + offset, entry, length);
        offset += length;
    }
    GenPut(at + 4 * entries, offset, 4);
    *sizeOut = size;
    return unit;
}

const char* SegmentsUnit(const XmlNode* root, SegmentGenSets* sets, uint8_t** unitOut,
                         size_t* sizeOut)
{
    Segments s = {0};
    const char* problem = ReadFile(root, &s);
    *unitOut = nullptr;
    *sizeOut = 0;
    int64_t words = problem == nullptr ? SetOf(sets, &s.word, segmentGivesWord) : 0;
    int64_t sentences = problem == nullptr ? SetOf(sets, &s.sentence, segmentGivesSentence) : 0;
    if (words < 0 || sentences < 0)
    {
        problem = "more override sets than the generator holds";
    }
    if (problem == nullptr && s.given != 0)
    {
        *unitOut = Unit(&s, (uint32_t)words, (uint32_t)sentences, sizeOut);
    }
    for (int k = 0; k < s.backwardCount; k++)
    {
        free(s.backward[k].text);
    }
    for (int k = 0; k < s.forwardCount; k++)
    {
        free(s.forward[k].text);
    }
    free(s.backward);
    free(s.forward);
    return problem;
}

void SegmentOverridesSource(const SegmentGenSets* sets, char** sourceOut, size_t* sourceSizeOut)
{
    size_t overrides = 0;
    for (int k = 0; k < sets->count; k++)
    {
        overrides += (size_t)sets->sets[k].count;
    }
    size_t capacity = 1024 + overrides * 64 + (size_t)sets->count * 64;
    char* source = GenAllocate(capacity);
    int size = snprintf(source, capacity,
                        "// SPDX-License-Identifier: MIT\n"
                        "// Copyright (c) 2026 Sirac Ozmen\n"
                        "//\n"
                        "// The class overrides of CLDR's common/segments, each set once,\n"
                        "// in the order the files first give them.\n"
                        "// Generated by tools/mlocgen; do not edit.\n"
                        "// clang-format off\n"
                        "\n"
                        "#include \"segment_overrides.h\"\n"
                        "\n"
                        "const muniBreakClassOverride mlocSegmentOverrides[] = {\n");
    for (int k = 0; k < sets->count; k++)
    {
        for (int i = 0; i < sets->sets[k].count; i++)
        {
            size += snprintf(source + size, capacity - (size_t)size, "    {0x%04X, %s},\n",
                             sets->sets[k].codePoints[i], sets->sets[k].classes[i]);
        }
    }
    size += snprintf(source + size, capacity - (size_t)size,
                     "};\n\nconst SegmentOverrideSet mlocSegmentOverrideSets[] = {\n");
    int first = 0;
    for (int k = 0; k < sets->count; k++)
    {
        size += snprintf(source + size, capacity - (size_t)size, "    {%d, %d, %s},\n", first,
                         sets->sets[k].count,
                         sets->sets[k].given == segmentGivesWord ? "mloc_segmentWord"
                                                                 : "mloc_segmentSentence");
        first += sets->sets[k].count;
    }
    size += snprintf(source + size, capacity - (size_t)size,
                     "};\n\nconst uint32_t mlocSegmentOverrideSetCount = %d;\n", sets->count);
    *sourceOut = source;
    *sourceSizeOut = (size_t)size;
}

int GenerateSegments(const char* commonDirectory, DataFile* file, char** sourceOut,
                     size_t* sourceSizeOut, char* errorOut, size_t errorCapacity)
{
    Keys keys;
    if (!KeysOpen(&keys, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    SegmentGenSets* sets = GenAllocate(sizeof(SegmentGenSets));
    int count = 0;
    char** names = GenListXml(commonDirectory, "segments", &count);
    int ok = 1;
    for (int i = 0; ok && i < count; i++)
    {
        if (strcmp(names[i], "root") == 0)
        {
            continue;
        }
        char path[4096];
        snprintf(path, sizeof(path), "%s/segments/%s.xml", commonDirectory, names[i]);
        char error[512] = "";
        XmlNode* root = XmlParseFile(path, error, sizeof(error));
        if (root == nullptr)
        {
            ok = Fail(errorOut, errorCapacity, path, "cannot read", error);
            break;
        }
        uint8_t* unit = nullptr;
        size_t size = 0;
        const char* problem = SegmentsUnit(root, sets, &unit, &size);
        if (problem != nullptr)
        {
            ok = Fail(errorOut, errorCapacity, path, problem, names[i]);
        }
        else if (unit != nullptr)
        {
            char key[maxKey];
            KeyOf(&keys, names[i], key, sizeof(key));
            DataFileAdd(file, kindSegments, key, unit, size);
        }
        free(unit);
        XmlFree(root);
    }
    GenFreeList(names, count);
    KeysClose(&keys);
    if (ok)
    {
        SegmentOverridesSource(sets, sourceOut, sourceSizeOut);
    }
    free(sets);
    return ok;
}
