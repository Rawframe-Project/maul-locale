// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a failed call of message.h and catalog.h leaves in its outputs:
// each output filled with a pattern, each call made to fail in the ways it
// can, and every byte of the pattern checked after. A failure leaves its
// outputs as they were (base.h), but for the length or size
// mloc_errorCapacity gives and what a call names as its own: the
// compile's error, a format's faults, the buffer a refused bind leaves
// without a bound message. Run with a directory to write the catalogs
// into.

#include "compile.h"
#include "file.h"
#include "test_harness.h"

#include "maul-locale/catalog.h"
#include "maul-locale/message.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    pattern = 0xA5,
};

static const char* s_directory;
static mlocData s_builtin;
static mlocDataSet s_data;
static mlocLocale s_en;
static mlocMessageFormatter s_formatter;
static mlocMessageFormatter s_unmade;
static uint8_t s_compiled[4096];
static uint8_t s_bound[1 << 16];
static uint8_t s_garbage[256];

static void Fill(void* output, size_t size)
{
    memset(output, pattern, size);
}

static bool Kept(const void* output, size_t size)
{
    const unsigned char* bytes = output;
    for (size_t i = 0; i < size; i++)
    {
        if (bytes[i] != pattern)
        {
            return false;
        }
    }
    return true;
}

// An output over a patterned buffer, its length a pattern too.
typedef struct Text
{
    char buffer[64];
    mlocOutput output;
} Text;

static void FillText(Text* text, size_t capacity)
{
    Fill(text->buffer, sizeof(text->buffer));
    text->output = (mlocOutput){.buffer = text->buffer, .capacity = capacity};
    Fill(&text->output.length, sizeof(text->output.length));
}

static bool KeptText(const Text* text)
{
    return Kept(text->buffer, sizeof(text->buffer)) &&
           Kept(&text->output.length, sizeof(text->output.length));
}

// A capacity failure: the buffer as it was, the length the one needed.
static bool Measured(const Text* text, size_t needed)
{
    return Kept(text->buffer, sizeof(text->buffer)) && text->output.length == needed;
}

// Compiles a message into s_compiled and binds it into s_bound.
static bool Bind(const mlocMessageFormatter* formatter, const char* text)
{
    size_t size = 0;
    size_t bound = 0;
    return mlocCompileMessage(text, strlen(text), s_compiled, sizeof(s_compiled), &size, nullptr)
                   .status == mloc_success &&
           mlocMessageFormatter_Bind(formatter, s_compiled, size, s_bound, sizeof(s_bound),
                                     &bound) == mloc_success;
}

static void TestFormatter(void)
{
    mlocMessageFormatter formatter;
    mlocMessageOptions bad = {.isolation = (mlocMessageIsolation)-1};
    mlocMessageFunction nameless = {.name = "x", .nameLength = 1};
    mlocMessageOptions function = {.functions = &nameless, .functionCount = 1};
    Fill(&formatter, sizeof(formatter));
    CHECK(mlocInitMessageFormatter(nullptr, &s_en, nullptr, &formatter) == mloc_errorInvalid &&
              mlocInitMessageFormatter(&s_data, &s_en, &bad, &formatter) == mloc_errorInvalid &&
              mlocInitMessageFormatter(&s_data, &s_en, &function, &formatter) ==
                  mloc_errorInvalid &&
              Kept(&formatter, sizeof(formatter)),
          "a formatter refused leaves its output");
}

static void TestCompile(void)
{
    static uint8_t buffer[256];
    size_t size;
    mlocMessageError error;
    Fill(buffer, sizeof(buffer));
    Fill(&size, sizeof(size));
    static const char s_missing[] = ".input {$n :number} .match $n 1 {{a}}";
    CHECK(mlocCompileMessage("{{a}", 4, buffer, sizeof(buffer), &size, &error).status ==
                  mloc_errorSyntax &&
              mlocCompileMessage(s_missing, sizeof(s_missing) - 1, buffer, sizeof(buffer), &size,
                                 &error)
                      .status == mloc_errorSyntax &&
              mlocCompileMessage(nullptr, 1, buffer, sizeof(buffer), &size, &error).status ==
                  mloc_errorInvalid &&
              mlocCompileMessage("a", MLOC_MAX_MESSAGE_TEXT + (size_t)1, buffer, sizeof(buffer),
                                 &size, &error)
                      .status == mloc_errorLimit &&
              Kept(buffer, sizeof(buffer)) && Kept(&size, sizeof(size)),
          "a compile refused while measuring leaves its outputs");
    size_t needed = 0;
    CHECK(mlocCompileMessage("{{abc}}", 7, nullptr, 0, &needed, nullptr).status ==
                  mloc_errorCapacity &&
              mlocCompileMessage("{{abc}}", 7, buffer, needed - 1, &size, nullptr).status ==
                  mloc_errorCapacity &&
              Kept(buffer, sizeof(buffer)) && size == needed,
          "a compile too large leaves the buffer and gives the capacity");
}

static void TestWriteText(void)
{
    size_t size = 0;
    Text text;
    FillText(&text, sizeof(text.buffer));
    CHECK(mlocCompileMessage("{{abc}}", 7, s_compiled, sizeof(s_compiled), &size, nullptr).status ==
                  mloc_success &&
              mlocWriteMessageText(s_garbage, sizeof(s_garbage), &text.output) ==
                  mloc_errorFormat &&
              mlocWriteMessageText(nullptr, size, &text.output) == mloc_errorInvalid &&
              KeptText(&text),
          "a text refused leaves its output");
    FillText(&text, 2);
    CHECK(mlocWriteMessageText(s_compiled, size, &text.output) == mloc_errorCapacity &&
              Measured(&text, 3),
          "a text too long leaves the buffer and gives the length");
}

static void TestBind(void)
{
    size_t size = 0;
    size_t bound;
    static uint8_t before[sizeof(s_compiled)];
    Fill(&bound, sizeof(bound));
    CHECK(mlocCompileMessage("{{abc}}", 7, s_compiled, sizeof(s_compiled), &size, nullptr).status ==
                  mloc_success &&
              mlocMessageFormatter_Bind(&s_unmade, s_compiled, size, s_bound, sizeof(s_bound),
                                        &bound) == mloc_errorInvalid &&
              mlocMessageFormatter_Bind(&s_formatter, s_garbage, sizeof(s_garbage), s_bound,
                                        sizeof(s_bound), &bound) == mloc_errorFormat &&
              Kept(&bound, sizeof(bound)),
          "a bind refused leaves its size");
    memcpy(before, s_compiled, sizeof(s_compiled));
    CHECK(mlocMessageFormatter_Bind(&s_formatter, s_compiled, size, s_compiled, sizeof(s_compiled),
                                    &bound) == mloc_errorInvalid &&
              memcmp(before, s_compiled, sizeof(s_compiled)) == 0 && Kept(&bound, sizeof(bound)),
          "a bind over its own message leaves both");
    size_t needed = 0;
    CHECK(mlocMessageFormatter_Bind(&s_formatter, s_compiled, size, nullptr, 0, &needed) ==
                  mloc_errorCapacity &&
              mlocMessageFormatter_Bind(&s_formatter, s_compiled, size, s_bound, needed - 1,
                                        &bound) == mloc_errorCapacity &&
              bound == needed,
          "a bind too large gives the capacity");
}

static void TestFormat(void)
{
    mlocMessageFault faults[4];
    mlocMessageFaults reported = {faults, 4, 0};
    Fill(faults, sizeof(faults));
    Fill(&reported.count, sizeof(reported.count));
    mlocMessageArgument unnamed = {nullptr, 1, {mloc_messageValueInteger, 0, {.integer = 1}}};
    Text text;
    FillText(&text, sizeof(text.buffer));
    CHECK(
        Bind(&s_formatter, "{$n} {$missing}") &&
            mlocFormatMessage(s_bound, &unnamed, 1, &text.output, &reported) == mloc_errorInvalid &&
            mlocFormatMessage(s_garbage, nullptr, 0, &text.output, &reported) ==
                mloc_errorInvalid &&
            mlocFormatMessage(s_bound, nullptr, 1, &text.output, &reported) == mloc_errorInvalid &&
            KeptText(&text) && Kept(faults, sizeof(faults)) &&
            Kept(&reported.count, sizeof(reported.count)),
        "a format refused leaves its outputs");
    mlocMessageFormatter strict;
    mlocMessageOptions options = {.strict = true};
    FillText(&text, sizeof(text.buffer));
    CHECK(mlocInitMessageFormatter(&s_data, &s_en, &options, &strict) == mloc_success &&
              Bind(&strict, "a {$missing}") &&
              mlocFormatMessage(s_bound, nullptr, 0, &text.output, &reported) ==
                  mloc_errorFallback &&
              KeptText(&text) && reported.count == 1,
          "a strict format with a fault leaves the text and reports the fault");
    FillText(&text, 2);
    CHECK(Bind(&s_formatter, "{{abc}}") &&
              mlocFormatMessage(s_bound, nullptr, 0, &text.output, nullptr) == mloc_errorCapacity &&
              Measured(&text, 3),
          "a format too long leaves the buffer and gives the length");
}

// x:probe's resolve entry: each accessor of its call refused, its
// outputs checked.
static mlocMessageFaultKind ProbeResolve(void* context, const mlocMessageCall* call,
                                         const mlocMessageOperand* operand,
                                         mlocMessageResolved* valueOut)
{
    (void)operand;
    bool* kept = context;
    const char* name;
    size_t length;
    mlocMessageOperand value;
    mlocMessageResolved storage;
    mlocLocale locale;
    Fill((void*)&name, sizeof(name));
    Fill(&length, sizeof(length));
    Fill(&value, sizeof(value));
    Fill(&storage, sizeof(storage));
    Fill(&locale, sizeof(locale));
    *kept =
        mlocMessageCall_GetOption(call, 1, &name, &length, &value, &storage) == mloc_errorInvalid &&
        mlocMessageCall_GetOption(nullptr, 0, &name, &length, &value, &storage) ==
            mloc_errorInvalid &&
        mlocMessageCall_FindOption(call, "zz", 2, &value, &storage) == mloc_errorNotFound &&
        mlocMessageCall_FindOption(call, nullptr, 0, &value, &storage) == mloc_errorInvalid &&
        mlocMessageCall_GetLocale(nullptr, &locale) == mloc_errorInvalid &&
        Kept((const void*)&name, sizeof(name)) && Kept(&length, sizeof(length)) &&
        Kept(&value, sizeof(value)) && Kept(&storage, sizeof(storage)) &&
        Kept(&locale, sizeof(locale));
    memset(valueOut, 0, sizeof(*valueOut));
    return mloc_messageFaultNone;
}

static void TestCall(void)
{
    bool kept = false;
    mlocMessageFunction probe = {"x:probe", 7,       &kept,   ProbeResolve,
                                 nullptr,   nullptr, nullptr, nullptr};
    mlocMessageOptions options = {.functions = &probe, .functionCount = 1};
    mlocMessageFormatter formatter;
    char buffer[64];
    mlocOutput output = {buffer, sizeof(buffer), 0, nullptr, nullptr};
    CHECK(mlocInitMessageFormatter(&s_data, &s_en, &options, &formatter) == mloc_success &&
              Bind(&formatter, "{|a| :x:probe k=1}") &&
              mlocFormatMessage(s_bound, nullptr, 0, &output, nullptr) >= 0 && kept,
          "a call's option or locale refused leaves its outputs");
}

static void TestParts(void)
{
    mlocMessagePartInfo info;
    mlocMessageParameter parameter;
    mlocPart placeholder = mloc_messagePartPlaceholder;
    Fill(&info, sizeof(info));
    Fill(&parameter, sizeof(parameter));
    CHECK(Bind(&s_formatter, "{$n :number k=1}") &&
              mlocGetMessagePartInfo(s_garbage, placeholder, &info) == mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_bound, mloc_messagePartText, &info) == mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_bound, 9 << 8 | placeholder, &info) == mloc_errorInvalid &&
              Kept(&info, sizeof(info)),
          "a part's info refused leaves its output");
    CHECK(mlocGetMessagePartParameter(s_bound, placeholder, 1, &parameter) == mloc_errorInvalid &&
              mlocGetMessagePartParameter(s_garbage, placeholder, 0, &parameter) ==
                  mloc_errorInvalid &&
              mlocFindMessagePartOption(s_bound, placeholder, "zz", 2, &parameter) ==
                  mloc_errorNotFound &&
              mlocFindMessagePartOption(s_bound, placeholder, nullptr, 1, &parameter) ==
                  mloc_errorInvalid &&
              Kept(&parameter, sizeof(parameter)),
          "a part's parameter refused leaves its output");
}

static void Write(const char* name, const char* text)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", s_directory, name);
    (void)FileWrite(path, text, strlen(text));
}

// Opens a catalog the tool wrote; the bytes are the caller's to free.
static uint8_t* Open(const char* locale, mlocCatalog* catalog, mlocMessageFormatter* formatter)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.mlct", s_directory, locale);
    size_t size = 0;
    uint8_t* bytes = (uint8_t*)FileRead(path, &size);
    mlocLocale parsed;
    if (bytes == nullptr || mlocOpenCatalog(bytes, size, catalog) != mloc_success ||
        mlocParseLocale(locale, strlen(locale), &parsed).status != mloc_success ||
        mlocInitMessageFormatter(&s_data, &parsed, nullptr, formatter) != mloc_success)
    {
        free(bytes);
        return nullptr;
    }
    return bytes;
}

// The set of the source en and fr, "bye" untranslated in fr.
static bool MakeCatalogs(void)
{
    Write("fr.mf2", "@locale fr\n---\nhello = Bonjour\n");
    Write("en.mf2", "@locale en\n---\nhello = Hello\nbye = Goodbye\n");
    char paths[2][1024];
    snprintf(paths[0], sizeof(paths[0]), "%s/en.mf2", s_directory);
    snprintf(paths[1], sizeof(paths[1]), "%s/fr.mf2", s_directory);
    char* set[] = {paths[0], paths[1]};
    FILE* report = tmpfile();
    bool made = CatalogCompile(s_directory, set, 2, report != nullptr ? report : stderr);
    if (report != nullptr)
    {
        fclose(report);
    }
    return made;
}

static void TestCatalog(void)
{
    mlocCatalog catalogs[2];
    mlocMessageFormatter formatters[2];
    uint8_t* fr = nullptr;
    uint8_t* en = nullptr;
    bool made = MakeCatalogs() && (fr = Open("fr", &catalogs[0], &formatters[0])) != nullptr &&
                (en = Open("en", &catalogs[1], &formatters[1])) != nullptr;
    CHECK(made, "the set compiles and opens");
    if (!made)
    {
        free(fr);
        free(en);
        return;
    }
    mlocCatalog zero;
    memset(&zero, 0, sizeof(zero));
    mlocCatalog catalog;
    mlocCatalogInfo info;
    uint32_t index;
    const void* message;
    size_t size;
    Fill(&catalog, sizeof(catalog));
    Fill(&info, sizeof(info));
    Fill(&index, sizeof(index));
    Fill((void*)&message, sizeof(message));
    Fill(&size, sizeof(size));
    uint32_t bye = 0;
    CHECK(mlocOpenCatalog(s_garbage, sizeof(s_garbage), &catalog) == mloc_errorFormat &&
              mlocOpenCatalog(nullptr, 0, &catalog) == mloc_errorInvalid &&
              mlocCatalog_GetInfo(&zero, &info) == mloc_errorInvalid &&
              mlocCatalog_FindKey(&catalogs[0], "nope", 4, &index) == mloc_errorNotFound &&
              mlocCatalog_FindKey(&zero, "hello", 5, &index) == mloc_errorInvalid &&
              mlocCatalog_FindKey(&catalogs[1], "bye", 3, &bye) == mloc_success &&
              mlocCatalog_GetMessage(&catalogs[0], bye, &message, &size) == mloc_errorNotFound &&
              mlocCatalog_GetMessage(&catalogs[0], 99, &message, &size) == mloc_errorInvalid &&
              mlocCatalog_GetMessage(&zero, 0, &message, &size) == mloc_errorInvalid &&
              Kept(&catalog, sizeof(catalog)) && Kept(&info, sizeof(info)) &&
              Kept(&index, sizeof(index)) && Kept((const void*)&message, sizeof(message)) &&
              Kept(&size, sizeof(size)),
          "a catalog, its info, a key or a message refused leaves its outputs");
    mlocCatalogChain chain;
    uint8_t small[8];
    Fill(&chain, sizeof(chain));
    CHECK(mlocMeasureCatalogChain(catalogs, formatters, 0, &size) == mloc_errorInvalid &&
              mlocMeasureCatalogChain(catalogs, formatters, MLOC_MAX_CATALOG_CHAIN + 1, &size) ==
                  mloc_errorLimit &&
              mlocInitCatalogChain(catalogs, formatters, 2, small, sizeof(small), &chain) ==
                  mloc_errorCapacity &&
              mlocInitCatalogChain(catalogs, formatters, MLOC_MAX_CATALOG_CHAIN + 1, small,
                                   sizeof(small), &chain) == mloc_errorLimit &&
              mlocInitCatalogChain(&zero, formatters, 1, small, sizeof(small), &chain) ==
                  mloc_errorInvalid &&
              Kept(&size, sizeof(size)) && Kept(&chain, sizeof(chain)),
          "a chain measured or made refused leaves its outputs");
    size_t needed = 0;
    uint8_t* cache = nullptr;
    made = mlocMeasureCatalogChain(catalogs, formatters, 2, &needed) == mloc_success &&
           (cache = malloc(needed)) != nullptr &&
           mlocInitCatalogChain(catalogs, formatters, 2, cache, needed, &chain) == mloc_success;
    mlocMessageFault faults[4];
    mlocMessageFaults reported = {faults, 4, 0};
    mlocCatalogResult result;
    Text text;
    Fill(faults, sizeof(faults));
    Fill(&reported.count, sizeof(reported.count));
    Fill(&result, sizeof(result));
    FillText(&text, sizeof(text.buffer));
    mlocCatalogChain unmade;
    memset(&unmade, 0, sizeof(unmade));
    CHECK(made &&
              mlocCatalogChain_FormatMessage(&chain, 99, nullptr, 0, &text.output, &reported,
                                             &result) == mloc_errorInvalid &&
              mlocCatalogChain_FormatMessage(&unmade, 0, nullptr, 0, &text.output, &reported,
                                             &result) == mloc_errorInvalid &&
              mlocCatalogChain_FormatKey(&chain, nullptr, 1, nullptr, 0, &text.output, &reported,
                                         &result) == mloc_errorInvalid &&
              KeptText(&text) && Kept(faults, sizeof(faults)) &&
              Kept(&reported.count, sizeof(reported.count)) && Kept(&result, sizeof(result)),
          "a chain's format refused leaves its outputs");
    FillText(&text, 2);
    mlocResult key =
        mlocCatalogChain_FormatKey(&chain, "nope", 4, nullptr, 0, &text.output, nullptr, &result);
    bool keyMeasured = Measured(&text, 4);
    FillText(&text, 2);
    CHECK(key == mloc_errorCapacity && keyMeasured &&
              mlocCatalogChain_FormatKey(&chain, "hello", 5, nullptr, 0, &text.output, nullptr,
                                         &result) == mloc_errorCapacity &&
              Measured(&text, 7) && Kept(&result, sizeof(result)),
          "a key or a message too long leaves the buffer and the result and gives the length");
    free(cache);
    free(fr);
    free(en);
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    memset(&s_unmade, 0, sizeof(s_unmade));
    Fill(s_garbage, sizeof(s_garbage));
    CHECK(mlocParseLocale("en", 2, &s_en).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &s_en, nullptr, &s_formatter) == mloc_success,
          "a formatter to work with");
    TestFormatter();
    TestCompile();
    TestWriteText();
    TestBind();
    TestFormat();
    TestCall();
    TestParts();
    TestCatalog();
    return s_failures == 0 ? 0 : 1;
}
