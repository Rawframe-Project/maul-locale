// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog tool's resource reader and checks: the W3C draft's syntax
// read into its data model, attachment of comments and metadata, escapes,
// and every refusal with its line.

#include "resource.h"
#include "resource_check.h"
#include "resource_write.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

static int Read(const char* text, Resource* r, ResError* e)
{
    return ResourceRead(text, strlen(text), r, e);
}

static int Is(const ResText* t, const char* s)
{
    return t->length == strlen(s) && memcmp(t->bytes, s, t->length) == 0;
}

// The example's entries, sections and their metadata.
static void CheckTheExamplesParts(const Resource* rr)
{
    const Resource r = *rr;
    const ResItem* two = &r.sections[0].items[1];
    CHECK(two->metaCount == 1 &&
              Is(&two->meta[0].value, "$placeholder - A comment on the placeholder value."),
          "metadata attaches to the next entry");
    const ResItem* three = &r.sections[0].items[2];
    CHECK(Is(&three->meta[0].value, "$foobar - An input parameter\nwith a multiline description") &&
              Is(&three->value, "Some {$foobar} message\nwith a multiline value\n  and a third "
                                "line that starts with two spaces."),
          "multiline values: indents off, an escaped space kept");
    const ResItem* four = &r.sections[1].items[0];
    CHECK(Is(&four->value, ".input {$count :integer}\n.match $count\n0   {{You have no "
                           "messages.}}\none {{You have {$count} message.}}\n*   {{You have "
                           "{$count} messages.}}"),
          "a value that starts on the next line");
    CHECK(r.sections[2].id.count == 2 && Is(&r.sections[2].id.parts[1], "more") &&
              r.sections[2].metaCount == 1 && Is(&r.sections[2].meta[0].key, "do-not-translate"),
          "a section's full path and its metadata");
    CHECK(Is(&r.sections[2].items[0].comment, "This message (section.more.five) should not be "
                                              "modified."),
          "a comment attaches to the next entry");
}

static void TestTheDraftsExample(void)
{
    static const char s_text[] = "# The resource-level locale is the only required property.\n"
                                 "@locale en-US\n"
                                 "---\n"
                                 "\n"
                                 "one = A plain, simple message.\n"
                                 "\n"
                                 "@param $placeholder - A comment on the placeholder value.\n"
                                 "two = Another message with a {$placeholder}.\n"
                                 "\n"
                                 "@param $foobar - An input parameter\n"
                                 "                 with a multiline description\n"
                                 "three = Some {$foobar} message\n"
                                 "  with a multiline value\n"
                                 "  \\  and a third line that starts with two spaces.\n"
                                 "\n"
                                 "[section]\n"
                                 "\n"
                                 "four =\n"
                                 "  .input {$count :integer}\n"
                                 "  .match $count\n"
                                 "  0   {{You have no messages.}}\n"
                                 "  one {{You have {$count} message.}}\n"
                                 "  *   {{You have {$count} messages.}}\n"
                                 "\n"
                                 "@do-not-translate\n"
                                 "[section.more]\n"
                                 "\n"
                                 "# This message (section.more.five) should not be modified.\n"
                                 "five = Foo\n";
    Resource r;
    ResError e;
    CHECK(Read(s_text, &r, &e), "the draft's README example reads");
    CHECK(r.hasFrontmatter && r.metaCount == 1 && Is(&r.meta[0].key, "locale") &&
              Is(&r.meta[0].value, "en-US") &&
              Is(&r.comment, "The resource-level locale is the only required property."),
          "the frontmatter's comment and @locale are the resource's");
    CHECK(r.sectionCount == 3 && r.sections[0].itemCount == 3 && r.sections[1].itemCount == 1 &&
              r.sections[2].itemCount == 1,
          "the anonymous section, [section] and [section.more]");
    if (r.sectionCount == 3 && r.sections[0].itemCount == 3 && r.sections[1].itemCount == 1 &&
        r.sections[2].itemCount == 1)
    {
        CheckTheExamplesParts(&r);
    }
    CHECK(ResourceCheck(&r, "example", stderr), "the example checks as a catalog source");
    ResourceFree(&r);
}

static void TestEscapes(void)
{
    Resource r;
    ResError e;
    CHECK(Read("@locale tr\n---\n"
               "a\\.b = x\\ny\\t\\x41\\u00E7\\U01F600 \\{\\|\\}\\\\ \\ \n"
               "long = one \\\n   two\n",
               &r, &e),
          "escapes read");
    const ResItem* a = &r.sections[0].items[0];
    CHECK(a->id.count == 1 && Is(&a->id.parts[0], "a.b"), "an escaped dot is in its part");
    CHECK(Is(&a->value, "x\ny\tA\xC3\xA7\xF0\x9F\x98\x80 \\{\\|\\}\\\\  "),
          "value escapes: MF2's kept, the others processed, an escaped trailing space kept");
    CHECK(Is(&r.sections[0].items[1].value, "one two"), "an escaped newline joins lines");
    char key[64];
    CHECK(!ResourceKey(&r.sections[0].id, &a->id, key, sizeof(key)),
          "a key part holding a dot is refused as a catalog key");
    ResourceFree(&r);
}

static void TestComments(void)
{
    Resource r;
    ResError e;
    CHECK(Read("@locale tr\n---\n# alone\n\n# one\n#two\n@param $x\nk = {$x}\n# last\n", &r, &e),
          "comments read");
    const ResSection* s = &r.sections[0];
    CHECK(s->itemCount == 3 && s->items[0].kind == resComment && Is(&s->items[0].comment, "alone"),
          "a comment an empty line ends stands alone");
    CHECK(s->items[1].kind == resEntry && Is(&s->items[1].comment, "one\ntwo") &&
              s->items[1].metaCount == 1,
          "adjacent comment lines join, metadata between them and the entry");
    CHECK(s->items[2].kind == resComment && Is(&s->items[2].comment, "last"),
          "a comment at the end stands alone");
    ResourceFree(&r);
}

static void TestRefusals(void)
{
    static const struct
    {
        const char* text;
        size_t line;
        const char* reason;
    } s_cases[] = {
        {"@a b\n\nk = v\n", 2, "an empty line between metadata"},
        {"@a b\n# c\nk = v\n", 2, "a comment between metadata"},
        {"k = v\n---\n", 2, "after a section or an entry"},
        {"---\n---\n", 2, "a second frontmatter"},
        {"  k = v\n", 1, "continues nothing"},
        {"k v\n", 1, "not followed by ="},
        {"[a\n", 1, "without its ]"},
        {"k = a\x01\n", 1, "control character"},
        {"k = a\rb\n", 1, "control character"},
        {"k = \\q\n", 1, "does not have"},
        {"k = \\uD800\n", 1, "no Unicode scalar"},
        {"---x = v\n", 1, "three hyphens"},
        {"@a\n", 2, "nothing after it"},
        {"k = v\xFF\n", 1, "not UTF-8"},
        {"k = v \\\nnext = w\n", 2, "not followed by an indent"},
        {"[a] b\n", 1, "more on a line"},
        {"\\\n = v\n", 2, "an identifier is expected"},
        {"a\\qb = v\n", 1, "does not have"},
        {"k = a\\.b\n", 1, "does not have"},
    };
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        Resource r;
        ResError e;
        int ok = Read(s_cases[i].text, &r, &e);
        CHECK(!ok && e.line == s_cases[i].line && e.column >= 1 &&
                  strstr(e.reason, s_cases[i].reason) != nullptr,
              s_cases[i].reason);
        ResourceFree(&r);
    }
}

static void TestChecks(void)
{
    static const struct
    {
        const char* text;
        int ok;
    } s_cases[] = {
        {"k = v\n", 0},
        {"@locale xx_!!\n---\nk = v\n", 0},
        {"@locale tr\n---\nk = v\nk = w\n", 0},
        {"@locale tr\n---\na.b = w\n[a]\nb = v\n", 0},
        {"@locale tr\n---\nk =\n", 0},
        {"@locale tr\n---\n@allow-empty\nk =\n", 1},
        {"@locale tr\n---\nk = {$x\n", 0},
        {"@locale tr\n---\n@obsolete\nk = {$x\n", 1},
        {"@locale tr\n---\n@mloc-max-length 0\nk = v\n", 0},
        {"@locale tr\n---\n@mloc-max-length 40\nk = v\n", 1},
        {"@locale tr\n---\n@mloc-source 0123456789abcdef\nk = v\n", 1},
        {"@locale tr\n---\n@mloc-source 0123\nk = v\n", 0},
        {"@locale tr\n---\n@translate maybe\nk = v\n", 0},
        {"@locale tr\n---\n@param count\nk = v\n", 0},
        {"@locale tr\n---\n@locale tr\nk = v\n", 0},
        {"@locale tr\n---\n@x-own anything\nk = v\n", 1},
        {"@locale tr\n---\n@allow-empty yes\nk = v\n", 0},
        {"@locale tr\n---\n@obsolete now\nk = v\n", 0},
        {"@locale tr\n---\ne\xCC\x81 = v\n", 0},
        {"@locale tr\n---\n\xC3\xA9 = v\n", 1},
    };
    FILE* sink = tmpfile();
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        Resource r;
        ResError e;
        int read = Read(s_cases[i].text, &r, &e);
        CHECK(read && ResourceCheck(&r, "t", sink != nullptr ? sink : stderr) == s_cases[i].ok,
              s_cases[i].text);
        ResourceFree(&r);
    }
    if (sink != nullptr)
    {
        fclose(sink);
    }
}

static void TestColumnsAndCharacters(void)
{
    Resource r;
    ResError e;
    CHECK(!Read("k = ab\x01\n", &r, &e) && e.line == 1 && e.column == 7,
          "a refusal's column counts from 1");
    ResourceFree(&r);
    CHECK(Read("@locale tr\r\n---\r\n"
               "\xC3\xA7\xEF\xBF\xBD = [a]\xC3\xA7\xF0\x9F\x98\x80 \\x39\\x66\\x6f\\u00FF\r\n",
               &r, &e) &&
              r.hasFrontmatter && r.sections[0].itemCount == 1,
          "CRLF lines, a frontmatter among them");
    const ResItem* item = &r.sections[0].items[0];
    CHECK(Is(&item->id.parts[0], "\xC3\xA7\xEF\xBF\xBD") &&
              Is(&item->value, "[a]\xC3\xA7\xF0\x9F\x98\x80 9fo\xC3\xBF"),
          "characters of two to four bytes, [ in a value, U+FFFD in an id, hex digits 9 and f");
    ResourceFree(&r);
}

// Writes a resource and reads it back.
static int RoundTrip(const char* text, char* written, size_t room)
{
    Resource first;
    Resource second;
    ResError e;
    if (!Read(text, &first, &e))
    {
        ResourceFree(&first);
        return 0;
    }
    FILE* f = tmpfile();
    if (f == nullptr)
    {
        ResourceFree(&first);
        return 0;
    }
    ResourceWrite(&first, f);
    rewind(f);
    size_t n = fread(written, 1, room - 1, f);
    written[n] = '\0';
    fclose(f);
    int ok = ResourceRead(written, n, &second, &e);
    if (ok)
    {
        FILE* a = tmpfile();
        FILE* b = tmpfile();
        ResourceWrite(&first, a);
        ResourceWrite(&second, b);
        ok = a != nullptr && b != nullptr && ftell(a) == ftell(b);
        if (a != nullptr)
        {
            fclose(a);
        }
        if (b != nullptr)
        {
            fclose(b);
        }
    }
    ResourceFree(&first);
    ResourceFree(&second);
    return ok;
}

static void TestWriter(void)
{
    char written[2048];
    CHECK(RoundTrip("# file\n@locale tr\n---\n# alone\n\n# about k\n@param $x\nk = a {$x}\n"
                    "  b\n\n[s.t]\n#\n\n@mloc-max-length 5\nv = \\{x\\}\n",
                    written, sizeof(written)) &&
              strcmp(written, "# file\n@locale tr\n---\n\n# alone\n\n\n# about k\n@param $x\n"
                              "k = a {$x}\n  b\n\n[s.t]\n\n#\n\n\n@mloc-max-length 5\n"
                              "v = \\{x\\}\n") == 0,
          "the written form: comments, metadata, an indented line, a section, a lone #");
    CHECK(RoundTrip("---x = \\ a\\ \\n\\nb\\n\\x00\\x7F\\r\\x5Cq\\u2028\n", written,
                    sizeof(written)) == 0,
          "three hyphens refused before writing");
    CHECK(RoundTrip("-\\-\\- = \\ a\\ \\n\\nb\\n\\x00\\x7F\\r\\x5Cq\\u2028\\t\n"
                    "\\x2e\\u0020\\U01F600 = \\x20\n",
                    written, sizeof(written)) &&
              strstr(written, "--\\x2D = \\ a\\ \n  \n  b\n  \\x00\\x7F\\r\\x5Cq\\u2028\\t\n") !=
                  nullptr &&
              strstr(written, "\\x2E\\x20\xF0\x9F\x98\x80 = \\ \n") != nullptr,
          "escapes written: inner newlines as lines, edge whitespace, controls, a lone "
          "backslash, a third hyphen, id symbols");
}

static void TestEdgesKeysAndReport(void)
{
    char written[256];
    CHECK(RoundTrip("k = \\n\\na\\n\\n\n", written, sizeof(written)) &&
              strcmp(written, "\nk = \\n\\na\\n\\n\n") == 0,
          "newlines at a value's ends written as escapes, which reading would drop");
    Resource r;
    ResError e;
    CHECK(Read("[a]\nb.c = v\n", &r, &e), "a dotted key in a section");
    char key[8];
    CHECK(ResourceKey(&r.sections[1].id, &r.sections[1].items[0].id, key, 6) &&
              strcmp(key, "a.b.c") == 0 &&
              !ResourceKey(&r.sections[1].id, &r.sections[1].items[0].id, key, 5),
          "a key's parts joined by dots, refused past its room");
    ResourceFree(&r);
    FILE* report = tmpfile();
    CHECK(report != nullptr && Read("@locale tr\n---\nk = v\nk = w\nk = x\n", &r, &e) &&
              !ResourceCheck(&r, "t", report),
          "keys given twice");
    char text[256] = "";
    if (report != nullptr)
    {
        rewind(report);
        size_t n = fread(text, 1, sizeof(text) - 1, report);
        text[n] = '\0';
        fclose(report);
    }
    CHECK(strcmp(text, "t:4: a key given twice: k, first on line 3\n"
                       "t:5: a key given twice: k, first on line 4\n") == 0,
          "each later one reported with the line before it");
    ResourceFree(&r);
}

int main(void)
{
    TestTheDraftsExample();
    TestEscapes();
    TestComments();
    TestRefusals();
    TestChecks();
    TestColumnsAndCharacters();
    TestWriter();
    TestEdgesKeysAndReport();
    return s_failures == 0 ? 0 : 1;
}
