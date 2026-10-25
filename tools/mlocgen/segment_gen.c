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
//   "[[$MidNumLet]-[.]]", requires that another variable adds them.
// - The line variables "$ID = [[\p{Line_Break=Ideographic}] [$CJ]]" and
//   "$NS = \p{Line_Break=Nonstarter}" are CJ as an ideograph.
// - Japanese's word variables and rules 13.3 and 13.4 are the Japanese
//   word rules.
// - A sentence segmentation's standard suppressions are kept as written.

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
    maxOverrides = 64,
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

typedef struct Segments
{
    uint32_t given;
    uint32_t flags;
    Overrides word;
    Overrides sentence;
    char** suppressions;
    int suppressionCount;
} Segments;

static const char* const s_wordClasses[segmentWordClassCount] = {
    "$CR",        "$LF",        "$Newline",       "$Extend",  "$ZWJ",          "$RI",
    "$Format",    "$Katakana",  "$Hebrew_Letter", "$ALetter", "$Single_Quote", "$Double_Quote",
    "$MidNumLet", "$MidLetter", "$MidNum",        "$Numeric", "$ExtendNumLet", "$WSegSpace",
    "$XX",
};

static const char* const s_sentenceClasses[segmentSentenceClassCount] = {
    "$CR",      "$LF",      "$Extend", "$Sep",       "$Format", "$Sp",    "$Lower", "$Upper",
    "$OLetter", "$Numeric", "$ATerm",  "$SContinue", "$STerm",  "$Close", "$XX",
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

static int ClassOf(const char* const* names, int count, const char* id)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(names[i], id) == 0)
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
    return overrides->addedCount > segmentMaxOverrides ? "more overrides than a segmenter holds"
                                                       : nullptr;
}

// A word or sentence segmentation's variables and rules.
static const char* ReadClasses(const XmlNode* segmentation, bool word, Segments* out)
{
    Overrides* overrides = word ? &out->word : &out->sentence;
    const char* const* names = word ? s_wordClasses : s_sentenceClasses;
    int classCount = word ? (int)segmentWordClassCount : (int)segmentSentenceClassCount;
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
    const char* x = *(const char* const*)a;
    const char* y = *(const char* const*)b;
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

// A sentence segmentation's standard suppressions, sorted from the end
// and each kept once.
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
    out->suppressions = GenAllocate(sizeof(char*) * (size_t)(list->childCount + 1));
    for (int i = 0; i < list->childCount; i++)
    {
        const XmlNode* item = list->children[i];
        size_t length = strlen(item->text);
        if (strcmp(item->name, "suppression") != 0 || length == 0 || length > segmentMaxSuppression)
        {
            return "a suppression empty or longer than a segmenter looks back";
        }
        out->suppressions[out->suppressionCount] = GenAllocate(length + 1);
        memcpy(out->suppressions[out->suppressionCount++], item->text, length + 1);
    }
    if (out->suppressionCount > 0)
    {
        qsort((void*)out->suppressions, (size_t)out->suppressionCount, sizeof(char*),
              CompareBackwards);
    }
    int kept = 0;
    for (int i = 0; i < out->suppressionCount; i++)
    {
        if (kept > 0 && strcmp(out->suppressions[kept - 1], out->suppressions[i]) == 0)
        {
            free(out->suppressions[i]);
            continue;
        }
        out->suppressions[kept++] = out->suppressions[i];
    }
    out->suppressionCount = kept;
    return kept > segmentMaxSuppressions ? "more suppressions than the format holds" : nullptr;
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

static void PutOverrides(uint8_t* at, const Overrides* overrides)
{
    for (int i = 0; i < overrides->addedCount; i++)
    {
        GenPut(at + segmentOverrideSize * (size_t)i, overrides->added[i].codePoint, 4);
        GenPut(at + segmentOverrideSize * (size_t)i + 4, overrides->added[i].value, 4);
    }
}

static uint8_t* Unit(const Segments* s, size_t* sizeOut)
{
    size_t text = 0;
    for (int i = 0; i < s->suppressionCount; i++)
    {
        text += strlen(s->suppressions[i]);
    }
    size_t overrides = (size_t)s->word.addedCount + (size_t)s->sentence.addedCount;
    size_t size = segmentHeaderSize + segmentOverrideSize * overrides +
                  4 * ((size_t)s->suppressionCount + 1) + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, s->given, 4);
    GenPut(unit + 4, s->flags, 4);
    GenPut(unit + 8, (uint64_t)s->word.addedCount, 4);
    GenPut(unit + 12, (uint64_t)s->sentence.addedCount, 4);
    GenPut(unit + 16, (uint64_t)s->suppressionCount, 4);
    GenPut(unit + 20, text, 4);
    uint8_t* at = unit + segmentHeaderSize;
    PutOverrides(at, &s->word);
    at += segmentOverrideSize * (size_t)s->word.addedCount;
    PutOverrides(at, &s->sentence);
    at += segmentOverrideSize * (size_t)s->sentence.addedCount;
    uint8_t* textAt = at + 4 * ((size_t)s->suppressionCount + 1);
    size_t offset = 0;
    for (int i = 0; i < s->suppressionCount; i++)
    {
        size_t length = strlen(s->suppressions[i]);
        GenPut(at + 4 * (size_t)i, offset, 4);
        memcpy(textAt + offset, s->suppressions[i], length);
        offset += length;
    }
    GenPut(at + 4 * (size_t)s->suppressionCount, offset, 4);
    *sizeOut = size;
    return unit;
}

const char* SegmentsUnit(const XmlNode* root, uint8_t** unitOut, size_t* sizeOut)
{
    Segments s = {0};
    const char* problem = ReadFile(root, &s);
    *unitOut = nullptr;
    *sizeOut = 0;
    if (problem == nullptr && s.given != 0)
    {
        *unitOut = Unit(&s, sizeOut);
    }
    for (int k = 0; k < s.suppressionCount; k++)
    {
        free(s.suppressions[k]);
    }
    free((void*)s.suppressions);
    return problem;
}

int GenerateSegments(const char* commonDirectory, DataFile* file, char* errorOut,
                     size_t errorCapacity)
{
    Keys keys;
    if (!KeysOpen(&keys, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
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
        const char* problem = SegmentsUnit(root, &unit, &size);
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
    return ok;
}
