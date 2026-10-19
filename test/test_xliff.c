// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// XLIFF 2.2 exchange (record mloc-0060): an export's exact text (notes,
// the size restriction, codes over original data, markup paired and
// isolated, syntax protected, escapes undone, states); an import of a
// CAT tool's file (its own prefix, <pc>, <mrk>, <cp>, segments split,
// a reordered target) and its records; the refusals; export then import
// giving the translation back. Run with a directory to write into.

#include "exchange_xliff.h"
#include "file.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_directory;

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

static int Exchange(bool import, const char* translation, const char* xliff)
{
    char s[1024];
    char t[1024];
    char x[1024];
    Path(s, sizeof(s), "en.mf2");
    Path(t, sizeof(t), translation);
    Path(x, sizeof(x), xliff);
    FILE* report = tmpfile();
    FILE* to = report != nullptr ? report : stderr;
    int ok = import ? CatalogImportXliff(s, t, x, to) : CatalogExportXliff(s, t, x, to);
    if (report != nullptr)
    {
        fclose(report);
    }
    return ok;
}

static const char s_source[] = "@locale en\n---\n# A greeting.\n@param $name The user\n"
                               "@mloc-max-length 30\nhello = Hi {$name} & {#b}you{/b} \\{x\\}\n"
                               "bye = Bye\n@translate no\nbrand = Maul\n"
                               "multi = .input {$n :integer}\n .match $n\n one {{One {#i}file}}\n"
                               " * {{{$n} files}}\n";

static const char s_exported[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<xliff xmlns=\"urn:oasis:names:tc:xliff:document:2.2\"\n"
    "       xmlns:slr=\"urn:oasis:names:tc:xliff:sizerestriction:2.0\"\n"
    "       version=\"2.2\" srcLang=\"en\" trgLang=\"tr\">\n"
    "  <file id=\"f1\" original=\"en.mf2\" canResegment=\"no\" xml:space=\"preserve\">\n"
    "    <slr:profiles generalProfile=\"xliff:codepoints\"/>\n"
    "    <unit id=\"u1\" name=\"hello\" slr:sizeRestriction=\"30\">\n"
    "      <notes>\n"
    "        <note category=\"comment\">A greeting.</note>\n"
    "        <note category=\"param\">$name The user</note>\n"
    "      </notes>\n"
    "      <originalData>\n"
    "        <data id=\"d1\">{$name}</data>\n"
    "        <data id=\"d2\">{#b}</data>\n"
    "        <data id=\"d3\">{/b}</data>\n"
    "      </originalData>\n"
    "      <segment state=\"initial\">\n"
    "        <source>Hi <ph id=\"1\" dataRef=\"d1\"/> &amp; <sc id=\"2\" dataRef=\"d2\"/>you"
    "<ec startRef=\"2\" dataRef=\"d3\"/> {x}</source>\n"
    "        <target><sc id=\"2\" dataRef=\"d2\"/>Sen<ec startRef=\"2\" dataRef=\"d3\"/> "
    "<ph id=\"1\" dataRef=\"d1\"/> &lt;3</target>\n"
    "      </segment>\n"
    "    </unit>\n"
    "    <unit id=\"u2\" name=\"bye\">\n"
    "      <segment>\n"
    "        <source>Bye</source>\n"
    "      </segment>\n"
    "    </unit>\n"
    "    <unit id=\"u3\" name=\"multi\">\n"
    "      <originalData>\n"
    "        <data id=\"d1\">.input {$n :integer}\n.match $n\none {{</data>\n"
    "        <data id=\"d2\">{#i}</data>\n"
    "        <data id=\"d3\">}}\n* {{</data>\n"
    "        <data id=\"d4\">{$n}</data>\n"
    "        <data id=\"d5\">}}</data>\n"
    "      </originalData>\n"
    "      <segment>\n"
    "        <source><ph id=\"1\" dataRef=\"d1\" canDelete=\"no\"/>One "
    "<sc id=\"2\" dataRef=\"d2\" isolated=\"yes\"/>file<ph id=\"3\" dataRef=\"d3\" "
    "canDelete=\"no\"/><ph id=\"4\" dataRef=\"d4\"/> files<ph id=\"5\" dataRef=\"d5\" "
    "canDelete=\"no\"/></source>\n"
    "      </segment>\n"
    "    </unit>\n"
    "  </file>\n"
    "</xliff>\n";

static void TestExport(void)
{
    Write("en.mf2", s_source);
    Write("tr.mf2", "@locale tr\n---\n@mloc-source 0000000000000000\n"
                    "hello = {#b}Sen{/b} {$name} <3\nbrand = Moul\n");
    char* xliff = Exchange(false, "tr.mf2", "tr.xlf") ? Read("tr.xlf") : nullptr;
    CHECK(xliff != nullptr && strcmp(xliff, s_exported) == 0,
          "the export: notes, the size restriction, codes over data shared by source and "
          "target, markup paired and isolated, syntax protected, escapes undone, states");
    if (xliff != nullptr && strcmp(xliff, s_exported) != 0)
    {
        fputs(xliff, stderr);
    }
    free(xliff);
}

// A CAT tool's file: its own prefix, a <pc> for paired markup, <mrk>, a
// <cp>, a unit split into two segments, a target reordered.
static const char s_tool[] =
    "<?xml version=\"1.0\"?>\n"
    "<xlf:xliff xmlns:xlf=\"urn:oasis:names:tc:xliff:document:2.2\" version=\"2.2\" "
    "srcLang=\"en\" trgLang=\"tr\"><xlf:file id=\"f\">\n"
    "<xlf:unit id=\"a\" name=\"hello\"><xlf:originalData>"
    "<xlf:data id=\"d1\">{<xlf:cp hex=\"24\"/>name}</xlf:data><xlf:data id=\"d2\">{#b}</xlf:data>"
    "<xlf:data id=\"d3\">{/b}</xlf:data></xlf:originalData>"
    "<xlf:segment state=\"final\"><xlf:source>Hi <xlf:ph id=\"1\" dataRef=\"d1\"/> &amp; "
    "</xlf:source><xlf:target><xlf:mrk id=\"m\" translate=\"yes\">Selam</xlf:mrk> "
    "<xlf:ph id=\"1\" dataRef=\"d1\"/> </xlf:target></xlf:segment>"
    "<xlf:segment state=\"final\"><xlf:source><xlf:pc id=\"2\" dataRefStart=\"d2\" "
    "dataRefEnd=\"d3\">you</xlf:pc> {x}</xlf:source><xlf:target><xlf:pc id=\"2\" "
    "dataRefStart=\"d2\" dataRefEnd=\"d3\">sen</xlf:pc> {<xlf:cp hex=\"E9\"/><xlf:cp "
    "hex=\"80\"/>}</xlf:target>"
    "</xlf:segment></xlf:unit>\n"
    "<xlf:unit id=\"b\" name=\"bye\"><xlf:segment state=\"initial\"><xlf:source>Bye</xlf:source>"
    "<xlf:target>Ho\xC5\x9F\xC3\xA7\x61 kal</xlf:target></xlf:segment></xlf:unit>\n"
    "<xlf:unit id=\"c\" name=\"brand\"><xlf:segment><xlf:source>Maul</xlf:source>"
    "<xlf:target>Moul</xlf:target></xlf:segment></xlf:unit>\n"
    "<xlf:unit id=\"d\" name=\"multi\"><xlf:segment><xlf:source>x</xlf:source></xlf:segment>"
    "</xlf:unit>\n"
    "</xlf:file></xlf:xliff>\n";

static void TestImport(void)
{
    Write("en.mf2", s_source);
    Write("empty.mf2", "@locale tr\n---\n");
    Write("tool.xlf", s_tool);
    char* tr = Exchange(true, "empty.mf2", "tool.xlf") ? Read("empty.mf2") : nullptr;
    CHECK(tr != nullptr &&
              strstr(tr, "@mloc-source bd63af3bf1889f78\nhello = Selam {$name} {#b}sen{/b} "
                         "\\{\xC3\xA9\\x80\\}\n") != nullptr &&
              strstr(tr, "\nbye = Ho\xC5\x9F\xC3\xA7\x61 kal\n") != nullptr &&
              strstr(tr, "@mloc-source bd63af3bf1889f78\nbye") == nullptr &&
              strstr(tr, "Moul") == nullptr && strstr(tr, "multi") == nullptr,
          "a tool's file: its prefix, <mrk>, <pc>, <cp> (in data, of two bytes) and two segments "
          "read back, the source's "
          "hash recorded; an initial one unrecorded; a kept key and an untranslated unit left");
    free(tr);
}

static void TestRefusals(void)
{
    Write("en.mf2", s_source);
    Write("empty.mf2", "@locale tr\n---\n");
    char* before = Read("empty.mf2");
    const char* head =
        "<?xml version=\"1.0\"?><xliff xmlns=\"urn:oasis:names:tc:xliff:document:2.2\""
        " version=\"2.2\" srcLang=\"en\"><file id=\"f\">";
    char text[2048];
    snprintf(text, sizeof(text),
             "%s<unit id=\"a\"><segment><source>x</source><target>y</target></segment></unit>"
             "</file></xliff>",
             head);
    Write("noname.xlf", text);
    snprintf(text, sizeof(text),
             "%s<unit id=\"a\" name=\"bye\"><segment><source>x</source><target><ph id=\"1\" "
             "dataRef=\"d9\"/></target></segment></unit></file></xliff>",
             head);
    Write("nodata.xlf", text);
    snprintf(text, sizeof(text),
             "%s<unit id=\"a\" name=\"bye\"><originalData><data id=\"d1\">{$</data>"
             "</originalData><segment><source>x</source><target><ph id=\"1\" dataRef=\"d1\"/>"
             "</target></segment></unit></file></xliff>",
             head);
    Write("bad.xlf", text);
    Write("other.xlf", "<?xml version=\"1.0\"?><html/>");
    Write("broken.xlf", "<xliff><file>");
    CHECK(
        !Exchange(true, "empty.mf2", "noname.xlf") && !Exchange(true, "empty.mf2", "nodata.xlf") &&
            !Exchange(true, "empty.mf2", "bad.xlf") && !Exchange(true, "empty.mf2", "other.xlf") &&
            !Exchange(true, "empty.mf2", "broken.xlf") && !Exchange(true, "empty.mf2", "none.xlf"),
        "a unit without a name, a code naming no data, a target that does not compile, another "
        "document, broken XML, a file that is not");
    char s[1024];
    char t[1024];
    char x[1024];
    Path(s, sizeof(s), "en.mf2");
    Path(t, sizeof(t), "empty.mf2");
    Path(x, sizeof(x), "other.xlf");
    FILE* report = tmpfile();
    char said[512] = {0};
    if (report != nullptr)
    {
        (void)CatalogImportXliff(s, t, x, report);
        rewind(report);
        size_t read = fread(said, 1, sizeof(said) - 1, report);
        said[read] = '\0';
        fclose(report);
    }
    CHECK(strstr(said, "other.xlf: not an XLIFF document\n") != nullptr,
          "another document, reported as such");
    char* after = Read("empty.mf2");
    CHECK(before != nullptr && after != nullptr && strcmp(before, after) == 0,
          "a refused import writes nothing");
    free(before);
    free(after);
}

// An export with no size restriction, a note of a parameter alone, a
// control character written as <cp>, and a target code the source does
// not have, numbered anew.
static void TestExportEdges(void)
{
    Write("en.mf2", "@locale en\n---\n@param $name The user\nhello = Hi\\u0001 {$name}\n");
    Write("tr.mf2", "@locale tr\n---\nhello = {$name} {$name}\n");
    char* xliff = Exchange(false, "tr.mf2", "edge.xlf") ? Read("edge.xlf") : nullptr;
    CHECK(xliff != nullptr && strstr(xliff, "slr:profiles") == nullptr &&
              strstr(xliff, "category=\"comment\"") == nullptr &&
              strstr(xliff, "<note category=\"param\">$name The user</note>") != nullptr &&
              strstr(xliff, "<source>Hi<cp hex=\"0001\"/> <ph id=\"1\" dataRef=\"d1\"/>") !=
                  nullptr &&
              strstr(xliff, "<target><ph id=\"1\" dataRef=\"d1\"/> <ph id=\"2\" dataRef=\"d1\"/>"
                            "</target>") != nullptr,
          "no restriction, no profiles; a parameter's note alone; a control character as <cp>; a "
          "second use of a code numbered anew");
    free(xliff);
}

static void TestRoundTrip(void)
{
    Write("en.mf2", s_source);
    const char* tr = "@locale tr\n---\n@mloc-source 0000000000000000\n"
                     "hello = {#b}Sen{/b} {$name} \\\\ \\{\\}\n"
                     "multi = .input {$n :integer}\n .match $n\n one {{Bir {#i}dosya}}\n"
                     " * {{{$n} dosya}}\n";
    Write("trip.mf2", tr);
    Write("copy.mf2", "@locale tr\n---\n@mloc-source 0000000000000000\nhello = x\n");
    char* copy = nullptr;
    if (Exchange(false, "trip.mf2", "trip.xlf") && Exchange(true, "copy.mf2", "trip.xlf"))
    {
        copy = Read("copy.mf2");
    }
    CHECK(copy != nullptr &&
              strstr(copy, "@mloc-source 0000000000000000\nhello = {#b}Sen{/b} {$name} \\\\ "
                           "\\{\\}\n") != nullptr &&
              strstr(copy, "multi = .input {$n :integer}\n") != nullptr &&
              strstr(copy, "one {{Bir {#i}dosya}}") != nullptr,
          "export then import: the values back with their escapes, the stale record kept");
    free(copy);
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    TestExport();
    TestImport();
    TestRefusals();
    TestExportEdges();
    TestRoundTrip();
    return s_failures == 0 ? 0 : 1;
}
