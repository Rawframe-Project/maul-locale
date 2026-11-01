// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// PO exchange: the PO reader over gettext's forms (escapes, strings over
// lines, comments, flags, obsolete entries) and its refusals, the writer
// read back, an export's exact text, and an import's values, records,
// skips and refusals, with export then import giving the translation back.
// Run with a directory to write into.

#include "exchange_po.h"
#include "file.h"
#include "po.h"
#include "print.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_directory;

static bool Reads(const char* text, PoFile* po)
{
    PoError error;
    return PoRead(text, strlen(text), po, &error);
}

static const char* Refusal(const char* text)
{
    PoFile po;
    PoError error = {0, nullptr};
    if (PoRead(text, strlen(text), &po, &error))
    {
        PoFree(&po);
        return nullptr;
    }
    return error.reason;
}

// What the writer gives for a file, NUL-terminated; null when it fails.
static char* Written(const PoFile* po)
{
    FILE* f = tmpfile();
    if (f == nullptr)
    {
        return nullptr;
    }
    PoWrite(po, f);
    size_t size = (size_t)ftell(f);
    rewind(f);
    char* written = calloc(size + 1, 1);
    if (written != nullptr && fread(written, 1, size, f) != size)
    {
        written[0] = '\0';
    }
    fclose(f);
    return written;
}

static void TestPrint(void)
{
    char out[8] = "xxxxxxx";
    size_t size = 2;
    memcpy(out, "ab", 2);
    CHECK(TextPrint(out, sizeof(out), &size, "%s%d", "cd", 345) && size == 7 &&
              strcmp(out, "abcd345") == 0,
          "TextPrint appends a text that leaves capacity - 1 bytes in use, NUL-ended");
    size = 2;
    CHECK(!TextPrint(out, sizeof(out), &size, "%s%d", "cd", 3456) && size == 2 &&
              strcmp(out, "ab") == 0,
          "and refuses one that would fill the capacity, keeping the text before it");
    size = sizeof(out);
    CHECK(!TextPrint(out, sizeof(out), &size, "%s", ""), "and refuses any text at a full capacity");
}

static void TestReader(void)
{
    const char* text =
        "# translator\n#: src/a.c:1\nmsgid \"\"\nmsgstr \"Language: tr\\n\"\n"
        "\"X: y\\n\"\n\n"
        "#. first\n#.\n#. third\n#, c-format, fuzzy,mf2-format\n#| msgid \"old\"\n"
        "msgctxt \"k\"\nmsgid "
        "\"a\\tb\\\"c\\\\d\\101\\x42\\x39\\x6f\\x4F\\x40\\77\\107\\377\\n\"\n\"next\"\n"
        "msgstr \"\"\n\"x\" \t\r\n\n#~ msgid \"gone\"\n#~ msgstr \"x\"\n"
        "msgid \"bare\"\nmsgstr \"\"\n";
    PoFile po = {0};
    bool read = Reads(text, &po);
    CHECK(read && po.header != nullptr && strcmp(po.header, "Language: tr\nX: y\n") == 0 &&
              po.count == 2,
          "the header and two entries; the obsolete read past");
    if (read && po.count == 2)
    {
        const PoEntry* e = &po.entries[0];
        CHECK(strcmp(e->context, "k") == 0 && strcmp(e->id, "a\tb\"c\\dAB9oO@?G\377\nnext") == 0 &&
                  strcmp(e->string, "x") == 0 && strcmp(e->comments, "first\n\nthird") == 0 &&
                  e->fuzzy && e->mf2 && e->line == 13,
              "escapes, strings over lines, extracted comments, flags, the msgid's line");
        CHECK(po.entries[1].context == nullptr && strcmp(po.entries[1].id, "bare") == 0 &&
                  po.entries[1].comments == nullptr && !po.entries[1].fuzzy && !po.entries[1].mf2,
              "an entry without context, comments or flags");
    }
    char* written = read ? Written(&po) : nullptr;
    PoFile back = {0};
    CHECK(written != nullptr && Reads(written, &back) && back.count == po.count &&
              strcmp(back.header, po.header) == 0 &&
              strcmp(back.entries[0].id, po.entries[0].id) == 0 &&
              strcmp(back.entries[0].comments, po.entries[0].comments) == 0 &&
              back.entries[0].fuzzy && back.entries[0].mf2 &&
              strstr(written, "msgid \"\"\n\"a\\tb\\\"c\\\\dAB9oO@?G\xFF\\n\"\n\"next\"\n") !=
                  nullptr &&
              strstr(written, "#. first\n#.\n#. third\n") != nullptr,
          "the writer read back, a string split after its LF, an empty comment line bare");
    free(written);
    PoFree(&back);
    PoFree(&po);
}

static void TestEmptyLastComment(void)
{
    // An extracted comment whose last line is empty, alone or after another
    // line, is written with that line, so it reads back the same.
    static const char* const texts[] = {"#.\nmsgid \"t\"\nmsgstr \"\"\n",
                                        "#. a\n#.\nmsgid \"t\"\nmsgstr \"\"\n"};
    static const char* const comments[] = {"", "a\n"};
    for (size_t i = 0; i < 2; i++)
    {
        PoFile po = {0};
        PoFile back = {0};
        char* written = Reads(texts[i], &po) ? Written(&po) : nullptr;
        CHECK(po.count == 1 && po.entries[0].comments != nullptr &&
                  strcmp(po.entries[0].comments, comments[i]) == 0 && written != nullptr &&
                  Reads(written, &back) && back.count == 1 && back.entries[0].comments != nullptr &&
                  strcmp(back.entries[0].comments, comments[i]) == 0,
              "an extracted comment ending in an empty line written and read back the same");
        free(written);
        PoFree(&back);
        PoFree(&po);
    }
}

static void TestCommentCarriageReturns(void)
{
    // Every CR ending a comment line is the line's end, not the comment's:
    // a comment keeps none at its end, so it reads back the same once
    // written. A CR inside a line stays.
    static const char* const texts[] = {"#. a\r\r\nmsgid \"t\"\nmsgstr \"\"\n",
                                        "#. a\rb\r\n#.\r\r\nmsgid \"t\"\nmsgstr \"\"\n"};
    static const char* const comments[] = {"a", "a\rb\n"};
    for (size_t i = 0; i < 2; i++)
    {
        PoFile po = {0};
        PoFile back = {0};
        char* written = Reads(texts[i], &po) ? Written(&po) : nullptr;
        CHECK(po.count == 1 && po.entries[0].comments != nullptr &&
                  strcmp(po.entries[0].comments, comments[i]) == 0 && written != nullptr &&
                  Reads(written, &back) && back.count == 1 && back.entries[0].comments != nullptr &&
                  strcmp(back.entries[0].comments, comments[i]) == 0,
              "a comment line's trailing CRs read as its end, a CR inside it kept");
        free(written);
        PoFree(&back);
        PoFree(&po);
    }
}

static void TestRefusals(void)
{
    const char* plural = Refusal("msgid \"a\"\nmsgid_plural \"b\"\nmsgstr[0] \"\"\n");
    CHECK(plural != nullptr && strstr(plural, "plural") != nullptr, "a plural entry is refused");
    const char* twice = Refusal("msgid \"a\"\nmsgid \"b\"\nmsgstr \"\"\n");
    const char* unknown = Refusal("msgfoo \"a\"\n");
    CHECK(twice != nullptr && strstr(twice, "twice") != nullptr && unknown != nullptr &&
              strstr(unknown, "does not have") != nullptr &&
              Refusal("msgid \"\\400\"\nmsgstr \"\"\n") != nullptr,
          "each refusal its reason; an octal escape past a byte");
    CHECK(Refusal("msgid \"a\nmsgstr \"\"\n") != nullptr &&
              Refusal("msgid \"a\\q\"\nmsgstr \"\"\n") != nullptr &&
              Refusal("\"a\"\nmsgid \"a\"\nmsgstr \"\"\n") != nullptr &&
              Refusal("msgid \"a\"\nmsgid \"b\"\nmsgstr \"\"\n") != nullptr &&
              Refusal("msgid \"a\"\n") != nullptr && Refusal("msgstr \"a\"\n") != nullptr &&
              Refusal("msgfoo \"a\"\n") != nullptr &&
              Refusal("msgid \"a\" x\nmsgstr \"\"\n") != nullptr,
          "an open string, an unknown escape, a string with no keyword, a keyword twice, "
          "an entry without msgstr or msgid, an unknown keyword, text after a string");
    PoFile po;
    CHECK(Reads("", &po) && po.count == 0 && po.header == nullptr, "an empty file");
    PoFree(&po);
    // An entry refused when it ends names the line where it began.
    PoError noId = {0, nullptr};
    PoError noString = {0, nullptr};
    static const char withoutId[] = "msgid \"a\"\nmsgstr \"\"\n\n#. c\nmsgctxt \"k\"\nmsgstr \"\"";
    static const char withoutString[] = "\nmsgctxt \"k\"\nmsgid \"a\"\n";
    CHECK(!PoRead(withoutId, strlen(withoutId), &po, &noId) && noId.line == 5 &&
              !PoRead(withoutString, strlen(withoutString), &po, &noString) && noString.line == 3,
          "an entry without msgid at its first keyword's line, without msgstr at its msgid's");
    // The tool keeps strings as C strings: a NUL byte in one, as written or
    // escaped, is refused instead of cutting the message short.
    static const char raw[] = "msgid \"a\"\nmsgstr \"\"\n\nmsgid \"\0\"\nmsgstr \"x\"\n";
    PoError nul = {0, nullptr};
    const char* escaped = Refusal("msgid \"a\\0b\"\nmsgstr \"\"\n");
    bool rawRead = PoRead(raw, sizeof(raw) - 1, &po, &nul);
    if (rawRead)
    {
        PoFree(&po);
    }
    CHECK(!rawRead && nul.line == 4 && nul.reason != nullptr &&
              strstr(nul.reason, "NUL") != nullptr && escaped != nullptr &&
              strstr(escaped, "NUL") != nullptr,
          "a NUL byte in a string, as written or escaped, refused at its line");
    static const char comment[] = "#. one\n#. t\0wo\nmsgid \"a\"\nmsgstr \"\"\n";
    PoError inComment = {0, nullptr};
    bool commentRead = PoRead(comment, sizeof(comment) - 1, &po, &inComment);
    if (commentRead)
    {
        PoFree(&po);
    }
    CHECK(!commentRead && inComment.line == 2 && inComment.reason != nullptr &&
              strstr(inComment.reason, "NUL") != nullptr,
          "a NUL byte in an extracted comment, refused at its line");
}

static void Path(char* out, size_t capacity, const char* name)
{
    snprintf(out, capacity, "%s/%s", s_directory, name);
}

static void Write(const char* name, const char* text)
{
    char path[1024];
    Path(path, sizeof(path), name);
    (void)FileWrite(path, text, strlen(text));
}

static char* Read(const char* name)
{
    char path[1024];
    Path(path, sizeof(path), name);
    size_t size = 0;
    char* text = FileRead(path, &size);
    char* string = text != nullptr ? realloc(text, size + 1) : nullptr;
    if (string != nullptr)
    {
        string[size] = '\0';
    }
    return string != nullptr ? string : text;
}

static const char s_source[] = "@locale en\n---\n# A greeting.\n@param $name The user\n"
                               "hello = Hello {$name}\nbye = Bye\n@translate no\nbrand = Maul\n"
                               "[menu]\nopen = Open\n";

static const char s_exported[] =
    "msgid \"\"\nmsgstr \"\"\n\"Project-Id-Version: en.mf2\\n\"\n\"PO-Revision-Date: \\n\"\n"
    "\"Last-Translator: \\n\"\n\"Language-Team: \\n\"\n\"Language: tr\\n\"\n"
    "\"MIME-Version: 1.0\\n\"\n\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
    "\"Content-Transfer-Encoding: 8bit\\n\"\n\"X-Generator: mloccat\\n\"\n\n"
    "#. A greeting.\n#. @param $name The user\n#, fuzzy, mf2-format\nmsgctxt \"hello\"\n"
    "msgid \"Hello {$name}\"\nmsgstr \"Merhaba {$name}\"\n\n"
    "#, mf2-format\nmsgctxt \"bye\"\nmsgid \"Bye\"\nmsgstr \"\"\n\n"
    "#, mf2-format\nmsgctxt \"menu.open\"\nmsgid \"Open\"\nmsgstr \"A\xC3\xA7\"\n";

static int Exchange(bool import, const char* translation, const char* po)
{
    char s[1024];
    char t[1024];
    char p[1024];
    Path(s, sizeof(s), "en.mf2");
    Path(t, sizeof(t), translation);
    Path(p, sizeof(p), po);
    FILE* report = tmpfile();
    FILE* to = report != nullptr ? report : stderr;
    int ok = import ? CatalogImportPo(s, t, p, to) : CatalogExportPo(s, t, p, to);
    if (report != nullptr)
    {
        fclose(report);
    }
    return ok;
}

static void TestExport(void)
{
    Write("en.mf2", s_source);
    Write("tr.mf2", "@locale tr\n---\n@mloc-source 0000000000000000\nhello = Merhaba {$name}\n"
                    "brand = Moul\n[menu]\nopen = A\xC3\xA7\n");
    char* po = Exchange(false, "tr.mf2", "tr.po") ? Read("tr.po") : nullptr;
    CHECK(po != nullptr && strcmp(po, s_exported) == 0,
          "the export: the header, comments and @param, fuzzy when stale, an empty msgstr, the "
          "kept key left out");
    free(po);
}

static void TestImport(void)
{
    Write("en.mf2", s_source);
    Write("empty.mf2", "@locale tr\n---\n");
    Write("in.po",
          "#, mf2-format\nmsgctxt \"bye\"\nmsgid \"Bye\"\nmsgstr \"Ho\xC5\x9F\xC3\xA7\x61 kal\"\n\n"
          "#, fuzzy, mf2-format\nmsgctxt \"hello\"\nmsgid \"Hello {$name}\"\n"
          "msgstr \"Merhaba {$name}\"\n\n"
          "msgctxt \"menu.open\"\nmsgid \"Open\"\nmsgstr \"\"\n\n"
          "msgctxt \"brand\"\nmsgid \"Maul\"\nmsgstr \"Moul\"\n\n"
          "msgctxt \"gone\"\nmsgid \"Gone\"\nmsgstr \"Yok\"\n");
    char* tr = Exchange(true, "empty.mf2", "in.po") ? Read("empty.mf2") : nullptr;
    CHECK(tr != nullptr &&
              strstr(tr, "@mloc-source f07932a934038771\nbye = Ho\xC5\x9F\xC3\xA7\x61 kal\n") !=
                  nullptr &&
              strstr(tr, "\nhello = Merhaba {$name}\n") != nullptr &&
              strstr(tr, "@mloc-source 6a037f79ece2314b") == nullptr &&
              strstr(tr, "open") == nullptr && strstr(tr, "Moul") == nullptr &&
              strstr(tr, "Yok") == nullptr,
          "the import: a translation recorded with its msgid's hash, a fuzzy one not, an empty "
          "msgstr, a kept key and an unknown key left out");
    free(tr);
    char* before = Read("empty.mf2");
    Write("bad.po", "msgctxt \"bye\"\nmsgid \"Bye\"\nmsgstr \"{$\"\n");
    Write("bare.po", "msgid \"Bye\"\nmsgstr \"x\"\n");
    Write("plural.po", "msgctxt \"bye\"\nmsgid \"Bye\"\nmsgid_plural \"Byes\"\nmsgstr[0] \"x\"\n");
    Write("twice.po", "msgctxt \"bye\"\nmsgid \"Bye\"\nmsgstr \".input {$x :string} .input "
                      "{$x :string} {{a}}\"\n");
    CHECK(!Exchange(true, "empty.mf2", "twice.po"),
          "a translation whose data model is wrong (a variable declared twice) refused");
    CHECK(!Exchange(true, "empty.mf2", "bad.po") && !Exchange(true, "empty.mf2", "bare.po") &&
              !Exchange(true, "empty.mf2", "plural.po") && !Exchange(true, "empty.mf2", "none.po"),
          "a translation that does not compile, an entry without msgctxt, a plural entry, a file "
          "that is not");
    char* after = Read("empty.mf2");
    CHECK(before != nullptr && after != nullptr && strcmp(before, after) == 0,
          "a refused import writes nothing");
    free(before);
    free(after);
}

static void TestRoundTrip(void)
{
    Write("en.mf2", s_source);
    const char* tr = "@locale tr\n---\n@mloc-source 0000000000000000\nhello = Merhaba {$name}\n"
                     "[menu]\nopen = A\xC3\xA7\n";
    Write("trip.mf2", tr);
    Write("copy.mf2", tr);
    char* trip = nullptr;
    char* copy = nullptr;
    if (Exchange(false, "trip.mf2", "trip.po") && Exchange(true, "copy.mf2", "trip.po"))
    {
        trip = Read("trip.mf2");
        copy = Read("copy.mf2");
    }
    CHECK(trip != nullptr && copy != nullptr &&
              strstr(copy, "@mloc-source 0000000000000000\nhello = Merhaba {$name}") != nullptr &&
              strstr(copy, "@mloc-source 09da4906f82ff46c\nopen = A\xC3\xA7") != nullptr,
          "export then import: the values back, the stale record kept, the fresh recorded");
    free(trip);
    free(copy);
}

static size_t Count(const char* text, const char* needle)
{
    size_t count = 0;
    for (const char* at = text != nullptr ? strstr(text, needle) : nullptr; at != nullptr;
         at = strstr(at + 1, needle))
    {
        count++;
    }
    return count;
}

// Keys added where the source has them: a section made once and filled,
// entries replaced on a second import rather than added again.
static void TestSections(void)
{
    Write("en.mf2", "@locale en\n---\ntop = Top\n[menu]\nopen = Open\nclose = Close\n");
    Write("sec.mf2", "@locale tr\n---\ntop = \xC3\x9Cst\n");
    Write("sec.po", "msgctxt \"menu.open\"\nmsgid \"Open\"\nmsgstr \"A\xC3\xA7\"\n\n"
                    "msgctxt \"menu.close\"\nmsgid \"Close\"\nmsgstr \"Kapat\"\n\n"
                    "msgctxt \"top\"\nmsgid \"Top\"\nmsgstr \"Tepe\"\n");
    bool once = Exchange(true, "sec.mf2", "sec.po");
    bool twice = once && Exchange(true, "sec.mf2", "sec.po");
    char* text = Read("sec.mf2");
    CHECK(twice && Count(text, "[menu]") == 1 && Count(text, "open = ") == 1 &&
              Count(text, "close = Kapat") == 1 && Count(text, "top = ") == 1 &&
              Count(text, "top = Tepe") == 1 && Count(text, "\n\n[") == 1,
          "a section added once, its keys in it, an entry replaced not added, twice over");
    free(text);
    char s[1024];
    char t[1024];
    char missing[1024];
    Path(s, sizeof(s), "en.mf2");
    Path(t, sizeof(t), "sec.mf2");
    Path(missing, sizeof(missing), "none.po");
    FILE* report = tmpfile();
    char said[512] = {0};
    if (report != nullptr)
    {
        (void)CatalogImportPo(s, t, missing, report);
        rewind(report);
        size_t read = fread(said, 1, sizeof(said) - 1, report);
        said[read] = '\0';
        fclose(report);
    }
    CHECK(strstr(said, "none.po: cannot read\n") != nullptr, "a PO file that is not, reported");
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: test_po DIRECTORY\n");
        return 1;
    }
    s_directory = argv[1];
    TestPrint();
    TestReader();
    TestEmptyLastComment();
    TestCommentCarriageReturns();
    TestRefusals();
    TestExport();
    TestImport();
    TestRoundTrip();
    TestSections();
    return s_failures == 0 ? 0 : 1;
}
