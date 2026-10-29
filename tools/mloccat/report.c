// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog set's report, built (report.h).

#include "report.h"

#include "stale.h"

#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/message.h"

#include <stdlib.h>
#include <string.h>

enum
{
    // The most @mloc-example arguments read.
    exampleRoom = 32,
};

static void Add(LocaleReport* r, ReportKind kind, const char* key, uint32_t length, uint32_t most)
{
    r->findings = CatalogGrow(r->findings, r->count, sizeof(Finding));
    r->findings[r->count++] = (Finding){kind, key, length, most};
}

static const ResText* MetaValue(const CatalogEntry* e, const char* key)
{
    const ResMeta* m = CatalogFindMeta(e->item->meta, e->item->metaCount, key);
    return m != nullptr ? &m->value : nullptr;
}

// The @mloc-example arguments: "$name=value" parts, each value running to
// the next " $".
static size_t Examples(const ResText* text, mlocMessageArgument* out)
{
    size_t count = 0;
    size_t at = 0;
    while (text != nullptr && at < text->length && text->bytes[at] == '$' && count < exampleRoom)
    {
        size_t name = at + 1;
        size_t equals = name;
        while (equals < text->length && text->bytes[equals] != '=' && text->bytes[equals] != ' ')
        {
            equals++;
        }
        size_t end = equals;
        while (end < text->length &&
               !(text->bytes[end] == ' ' && end + 1 < text->length && text->bytes[end + 1] == '$'))
        {
            end++;
        }
        size_t value = equals < end ? equals + 1 : end;
        out[count++] = (mlocMessageArgument){
            text->bytes + name,
            equals - name,
            {mloc_messageValueString, 0, {.string = {text->bytes + value, end - value}}}};
        at = end + 1;
    }
    return count;
}

static uint32_t CodePoints(const char* text, size_t length)
{
    uint32_t count = 0;
    for (size_t i = 0; i < length; i++)
    {
        count += ((unsigned char)text[i] & 0xC0) != 0x80;
    }
    return count;
}

// The length of a message formatted with the examples, in code points;
// UINT32_MAX when it does not compile, bind or format.
static uint32_t Measure(const mlocMessageFormatter* f, const ResText* message,
                        const mlocMessageArgument* arguments, size_t count)
{
    size_t compiledSize = 0;
    size_t boundSize = 0;
    (void)mlocCompileMessage(message->bytes, message->length, nullptr, 0, &compiledSize, nullptr);
    uint8_t* compiled = malloc(compiledSize + 1);
    uint8_t* bound = nullptr;
    char* text = nullptr;
    uint32_t length = UINT32_MAX;
    if (compiled != nullptr && mlocCompileMessage(message->bytes, message->length, compiled,
                                                  compiledSize, &compiledSize, nullptr)
                                       .status == mloc_success)
    {
        (void)mlocMessageFormatter_Bind(f, compiled, compiledSize, nullptr, 0, &boundSize);
        bound = malloc(boundSize + 1);
    }
    if (bound != nullptr && mlocMessageFormatter_Bind(f, compiled, compiledSize, bound, boundSize,
                                                      &boundSize) == mloc_success)
    {
        mlocOutput out = {nullptr, 0, 0, nullptr, nullptr};
        (void)mlocFormatMessage(bound, arguments, count, &out, nullptr);
        text = malloc(out.length + 1);
        out = (mlocOutput){text, out.length, 0, nullptr, nullptr};
        mlocResult r = text != nullptr ? mlocFormatMessage(bound, arguments, count, &out, nullptr)
                                       : mloc_errorInvalid;
        length = r == mloc_success || r == mloc_withFallbacks ? CodePoints(text, out.length)
                                                              : UINT32_MAX;
    }
    free(text);
    free(bound);
    free(compiled);
    return length;
}

static int MakeFormatter(const CatalogInput* in, mlocMessageFormatter* out)
{
    static mlocData s_builtin;
    static mlocDataSet s_data;
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    mlocLocale locale;
    // No isolation marks: they are not text a reader counts.
    mlocMessageOptions options = {false, mloc_messageIsolationNone, nullptr, 0, nullptr};
    return mlocParseLocale(in->locale, strlen(in->locale), &locale).status == mloc_success &&
           mlocInitMessageFormatter(&s_data, &locale, &options, out) == mloc_success;
}

// A message above its source's @mloc-max-length.
static void CheckLength(LocaleReport* r, const mlocMessageFormatter* f, const CatalogEntry* source,
                        const CatalogEntry* entry)
{
    const ResText* most = MetaValue(source, "mloc-max-length");
    if (most == nullptr)
    {
        return;
    }
    uint32_t limit = (uint32_t)strtoul(most->bytes, nullptr, 10);
    mlocMessageArgument arguments[exampleRoom];
    size_t count = Examples(MetaValue(source, "mloc-example"), arguments);
    uint32_t length = Measure(f, &entry->item->value, arguments, count);
    if (length > limit)
    {
        Add(r, reportLong, entry->key, length, limit);
    }
}

// A translation's findings against the source.
static void CheckTranslation(LocaleReport* r, const CatalogInput* source,
                             const mlocMessageFormatter* f)
{
    const CatalogInput* in = r->input;
    for (uint32_t i = 0; i < source->count; i++)
    {
        const CatalogEntry* s = &source->entries[i];
        const CatalogEntry* t = CatalogFindEntry(in, s->key);
        if (s->keep)
        {
            if (t != nullptr &&
                (t->item->value.length != s->item->value.length ||
                 memcmp(t->item->value.bytes, s->item->value.bytes, s->item->value.length) != 0))
            {
                Add(r, reportDiffers, s->key, 0, 0);
            }
            continue;
        }
        r->translatable++;
        if (t == nullptr)
        {
            Add(r, reportMissing, s->key, 0, 0);
            continue;
        }
        r->translated++;
        SourceState state = SourceStateOf(t->item->meta, t->item->metaCount, &s->item->value);
        if (state != sourceFresh)
        {
            Add(r, state == sourceStale ? reportStale : reportUnrecorded, s->key, 0, 0);
        }
        CheckLength(r, f, s, t);
    }
    for (uint32_t i = 0; i < in->count; i++)
    {
        if (CatalogFindEntry(source, in->entries[i].key) == nullptr)
        {
            Add(r, reportOrphaned, in->entries[i].key, 0, 0);
        }
    }
}

int ReportBuild(const CatalogSet* set, Report* out)
{
    *out = (Report){calloc(set->count + 1, sizeof(LocaleReport)), set->count};
    if (out->locales == nullptr)
    {
        exit(1);
    }
    const CatalogInput* source = &set->inputs[0];
    int ok = 1;
    for (size_t i = 0; i < set->count; i++)
    {
        LocaleReport* r = &out->locales[i];
        r->input = &set->inputs[i];
        mlocMessageFormatter f;
        if (!MakeFormatter(r->input, &f))
        {
            ok = 0;
            continue;
        }
        if (i > 0)
        {
            CheckTranslation(r, source, &f);
            continue;
        }
        for (uint32_t k = 0; k < source->count; k++)
        {
            r->translatable++;
            r->translated++;
            CheckLength(r, &f, &source->entries[k], &source->entries[k]);
        }
    }
    return ok;
}

void ReportFree(Report* report)
{
    for (size_t i = 0; i < report->count; i++)
    {
        free(report->locales[i].findings);
    }
    free(report->locales);
    *report = (Report){0};
}

unsigned ReportKindsFound(const Report* report)
{
    unsigned kinds = 0;
    for (size_t i = 0; i < report->count; i++)
    {
        for (size_t k = 0; k < report->locales[i].count; k++)
        {
            kinds |= 1U << report->locales[i].findings[k].kind;
        }
    }
    return kinds;
}

static const char* const s_kindNames[reportKindCount] = {
    "missing", "stale", "unrecorded", "orphaned", "differs", "long",
};

const char* ReportKindName(ReportKind kind)
{
    return kind < reportKindCount ? s_kindNames[kind] : "";
}

int ReportReadKinds(const char* list, unsigned* kindsOut)
{
    unsigned kinds = 0;
    const char* at = list;
    while (*at != '\0')
    {
        size_t length = strcspn(at, ",");
        int known = 0;
        for (int k = 0; k < reportKindCount; k++)
        {
            if (strlen(s_kindNames[k]) == length && memcmp(at, s_kindNames[k], length) == 0)
            {
                kinds |= 1U << k;
                known = 1;
            }
        }
        if (!known)
        {
            return 0;
        }
        at += length + (at[length] == ',');
    }
    *kindsOut = kinds;
    return 1;
}
