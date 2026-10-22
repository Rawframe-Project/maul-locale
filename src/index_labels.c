// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "index_labels.h"

#include "bytes.h"
#include "code_point_trie.h"
#include "collation_index.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"

static const uint8_t s_ascii[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// A code point's CE32 and the unit that gives it, as ICU4C's
// internalAddContractions looks it up: the tailoring's, else the root's.
static uint32_t Ce32Of(const Collator* c, uint32_t codePoint, const CollationRoot** unitOut)
{
    if (c->tailored != 0)
    {
        uint32_t ce32 = mlocTrieValue(&c->tailoring.trie, codePoint);
        if (ce32 != collationFallbackCe32)
        {
            *unitOut = &c->tailoring;
            return ce32;
        }
    }
    *unitOut = &c->root;
    return mlocTrieValue(&c->root.trie, codePoint);
}

static bool IsContext(uint32_t ce32, uint32_t tag)
{
    return mlocCe32IsSpecial(ce32) && ce32 != collationImplicitCe32 && mlocCe32Tag(ce32) == tag;
}

typedef bool (*ContractionVisitor)(void* context, uint32_t first, const uint8_t* words,
                                   uint32_t count);

// Each contraction a code point starts, as ICU4C's
// ContractionsAndExpansions finds them without prefixes: a prefix list's
// default followed, a contraction list's entries given. False when the
// visitor refuses one.
static bool Contractions(const Collator* c, uint32_t codePoint, ContractionVisitor visit,
                         void* context)
{
    const CollationRoot* unit = nullptr;
    uint32_t ce32 = Ce32Of(c, codePoint, &unit);
    if (IsContext(ce32, collationTagPrefix))
    {
        ce32 = mlocCollationContext(unit, mlocCollationListStart(unit, mlocCe32Index(ce32)));
    }
    if (!IsContext(ce32, collationTagContraction))
    {
        return true;
    }
    uint32_t at = mlocCollationListStart(unit, mlocCe32Index(ce32));
    uint32_t count = mlocCollationContext(unit, at + 1);
    at += 2;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t length = mlocCollationContext(unit, at);
        if (!visit(context, codePoint, unit->contexts + (size_t)(at + 1) * 4, length))
        {
            return false;
        }
        at += length + 2;
    }
    return true;
}

static bool AddLabel(void* context, uint32_t first, const uint8_t* words, uint32_t count)
{
    IndexCandidates* out = context;
    if (out->labelCount == indexMaxLabels)
    {
        return false;
    }
    out->labels[out->labelCount++] = (IndexText){words, count, first};
    return true;
}

// ICU4C's firstStringsInScript: a boundary whose sample is a letter or
// unassigned stands for a script; the others for the special groups. (A
// Hangul sample is in the data as its jamo, the first a letter too.)
static bool AddScript(void* context, uint32_t first, const uint8_t* words, uint32_t count)
{
    IndexCandidates* out = context;
    uint32_t sample = mlocLoad32(words);
    muniGeneralCategory gc = muniGetGeneralCategory(sample);
    if (gc != muni_gcCn && (gc < muni_gcLu || gc > muni_gcLo))
    {
        return true;
    }
    if (out->scriptCount == indexMaxScripts)
    {
        return false;
    }
    out->scripts[out->scriptCount++] = (IndexText){words, count, first};
    return true;
}

// Whether a contraction label ends in a letter A to Z, after which
// ICU4C adds the ASCII labels for the pinyin ones to show as.
static bool EndsInAscii(const IndexText* t)
{
    uint32_t last = t->length > 0 ? mlocLoad32(t->bytes + (size_t)(t->length - 1) * 4) : t->first;
    return last >= 'A' && last <= 'Z';
}

static mlocResult AddDataLabels(const mlocDataSet* data, const mlocLocale* locale,
                                IndexCandidates* out)
{
    IndexLabels labels;
    if (!mlocFindIndexLabels(data, locale, &labels))
    {
        return mloc_success;
    }
    uint32_t at = 4;
    const uint8_t* text = nullptr;
    uint32_t length = 0;
    while (mlocNextIndexLabel(&labels, &at, &text, &length))
    {
        if (out->labelCount == indexMaxLabels)
        {
            return mloc_errorData;
        }
        out->labels[out->labelCount++] = (IndexText){text, length, 0};
    }
    return mloc_success;
}

mlocResult mlocIndexGather(const Collator* collator, const mlocDataSet* data,
                           const mlocLocale* locale, IndexCandidates* out)
{
    out->labelCount = 0;
    out->scriptCount = 0;
    if (!Contractions(collator, 0xFDD1, AddScript, out) ||
        !Contractions(collator, 0xFDD0, AddLabel, out))
    {
        return mloc_errorData;
    }
    if (out->labelCount == 0)
    {
        return AddDataLabels(data, locale, out);
    }
    bool ascii = false;
    for (uint32_t i = 0; !ascii && i < out->labelCount; i++)
    {
        ascii = EndsInAscii(&out->labels[i]);
    }
    for (uint32_t i = 0; ascii && i < 26; i++)
    {
        if (out->labelCount == indexMaxLabels)
        {
            return mloc_errorData;
        }
        out->labels[out->labelCount++] = (IndexText){s_ascii + i, 1, 0};
    }
    return mloc_success;
}

const uint8_t* mlocIndexTextBytes(const IndexText* text, uint8_t* scratch, uint32_t* lengthOut)
{
    if (text->first == 0)
    {
        *lengthOut = text->length;
        return text->bytes;
    }
    size_t length = 0;
    size_t size = 0;
    (void)muniEncodeUtf8(text->first, (char*)scratch, &size);
    length += size;
    for (uint32_t i = 0; i < text->length; i++)
    {
        (void)muniEncodeUtf8(mlocLoad32(text->bytes + (size_t)i * 4), (char*)scratch + length,
                             &size);
        length += size;
    }
    *lengthOut = (uint32_t)length;
    return scratch;
}
