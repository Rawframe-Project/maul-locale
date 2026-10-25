// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// XLIFF 2.2 exchange: an export's exact text (notes, the size restriction,
// codes over original data, markup paired and isolated, syntax protected,
// escapes undone, states); an import of a CAT tool's file (its own prefix,
// <pc>, <mrk>, <cp>, segments split, a reordered target) and its records;
// the refusals; export then import giving the translation back. Run with a
// directory to write into.

#include "exchange_xliff.h"
#include "file.h"
#include "stale.h"
#include "test_harness.h"
#include "xliff_pgs.h"

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

static const char s_source[] =
    "@locale en\n---\n# A greeting.\n@param $name The user\n"
    "@mloc-max-length 30\nhello = Hi {$name} & {#b}you{/b} \\{x\\}\n"
    "bye = Bye\n@translate no\nbrand = Maul\n"
    "multi = .local $c = {$n :integer}\n .match $c\n one {{One {#i}file}}\n"
    " * {{{$n} files}}\n";

static const char s_exported[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<xliff xmlns=\"urn:oasis:names:tc:xliff:document:2.2\"\n"
    "       xmlns:slr=\"urn:oasis:names:tc:xliff:sizerestriction:2.0\"\n"
    "       xmlns:pgs=\"urn:oasis:names:tc:xliff:pgs:1.0\"\n"
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
    "        <data id=\"d1\">.local $c = {$n :integer}\n.match $c\none {{</data>\n"
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
    snprintf(text, sizeof(text),
             "%s<unit id=\"a\" name=\"bye\"><originalData><data id=\"d1\">.input {$x :string} "
             ".input {$x :string} {{</data><data id=\"d2\">}}</data></originalData><segment>"
             "<source>x</source><target><ph id=\"1\" dataRef=\"d1\"/>a<ph id=\"2\" "
             "dataRef=\"d2\"/></target></segment></unit></file></xliff>",
             head);
    Write("twice.xlf", text);
    CHECK(!Exchange(true, "empty.mf2", "twice.xlf"),
          "a target whose data model is wrong (a variable declared twice) refused");
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

static const char s_selection[] =
    "@locale en\n---\nfiles = .input {$n :integer}\n .match $n\n 0 {{No files}}\n"
    " one {{One {#b}file{/b}}}\n * {{{$n} files}}\n"
    "place = .input {$p :number select=ordinal}\n .input {$g :string}\n .match $p $g\n"
    " one fem {{First, she}}\n * * {{{$p}th}}\n"
    "spaced = .input {$g :string}\n .match $g\n |a b| {{x}}\n * {{y}}\n"
    "exact = .input {$n :number select=exact}\n .match $n\n 1 {{one}}\n * {{many}}\n"
    "pair = .input {$n :integer}\n .input {$m :integer}\n .match $n $m\n 0 0 {{none}}\n"
    " 0 * {{n none}}\n zero * {{z}}\n * * {{rest}}\n"
    "sel = .input {$s :string}\n .match $s\n 9 {{nine}}\n 10 {{ten}}\n |a\\|b| {{pipe}}\n"
    " * {{other}}\n"
    "four = .input {$a :string}\n .input {$b :string}\n .input {$c :string}\n"
    " .input {$d :string}\n .match $a $b $c $d\n * * * * {{x}}\n"
    "five = .input {$a :string}\n .input {$b :string}\n .input {$c :string}\n"
    " .input {$d :string}\n .input {$e :string}\n .match $a $b $c $d $e\n * * * * * {{x}}\n"
    "plain = .input {$x :string}\n {{Hi {$x}}}\n";

// Whether each text is found after the one before.
static bool InOrder(const char* text, const char* const* parts, size_t count)
{
    const char* at = text;
    for (size_t i = 0; at != nullptr && i < count; i++)
    {
        at = strstr(at, parts[i]);
        at = at != nullptr ? at + strlen(parts[i]) : nullptr;
    }
    return at != nullptr;
}

static void TestSelectionExport(void)
{
    Write("en.mf2", s_selection);
    Write("ar.mf2", "@locale ar\n---\nfiles = .input {$n :integer}\n .match $n\n"
                    " 0 {{la milaffat}}\n one {{milaff}}\n two {{milaffan}}\n"
                    " few {{{$n} milaffat}}\n * {{{$n} milaff}}\n");
    char* x = Exchange(false, "ar.mf2", "ar.xlf") ? Read("ar.xlf") : nullptr;
    static const char* const cases[] = {
        "<unit id=\"u1\" name=\"files\" pgs:switch=\"plural:n\">",
        "ref=\"s2\" category=\"plural_examples\">0, 0.0</note>",
        "ref=\"s3\" category=\"plural_examples\">1, 1.0</note>",
        "ref=\"s5\" category=\"plural_examples\">3~10, 103~110, 203~210, 303~310, 403~410, "
        "503~510, …</note>",
        "<segment id=\"s1\" pgs:case=\"0\" state=\"translated\">\n"
        "        <source>No files</source>\n        <target>la milaffat</target>",
        "<segment id=\"s2\" pgs:case=\"zero\" state=\"translated\">\n"
        "        <source><ph id=\"1\" dataRef=\"d1\"/> files</source>\n"
        "        <target><ph id=\"1\" dataRef=\"d1\"/> milaff</target>",
        "<segment id=\"s3\" pgs:case=\"one\" state=\"translated\">\n"
        "        <source>One <sc id=\"2\" dataRef=\"d2\"/>file<ec startRef=\"2\" "
        "dataRef=\"d3\"/></source>\n        <target>milaff</target>",
        "pgs:case=\"two\"",
        "<target>milaffan</target>",
        "pgs:case=\"few\"",
        "<target><ph id=\"5\" dataRef=\"d1\"/> milaffat</target>",
        "<segment id=\"s6\" pgs:case=\"many\" state=\"translated\">",
        "<target><ph id=\"6\" dataRef=\"d1\"/> milaff</target>",
        "pgs:case=\"other\"",
        "<unit id=\"u2\" name=\"place\" pgs:switch=\"ordinal:p select:g\">",
        "<segment id=\"s1\" pgs:case=\"other fem\">\n        <source><ph id=\"1\" "
        "dataRef=\"d1\"/>th</source>",
        "<segment id=\"s2\" pgs:case=\"other other\">",
        "<unit id=\"u3\" name=\"spaced\">",
        "<segment>"};
    CHECK(x != nullptr && InOrder(x, cases, sizeof(cases) / sizeof(cases[0])) &&
              strstr(x, "pgs:case=\"one fem\"") == nullptr,
          "a selection message expanded to the target's categories: exact numbers first, "
          "CLDR's order, examples as notes, a missing category from '*', select keys and "
          "other, a key with a space left as one segment");
    static const char* const more[] = {
        "ref=\"s7\" category=\"plural_examples\">100~102, 200~202, 300~302, 400~402, "
        "500~502, 600~602, …, 0.1~0.9, 1.1~1.9, 2.1~2.5</note>",
        "name=\"place\" pgs:switch=\"ordinal:p select:g\">\n      <notes>\n        <note "
        "appliesTo=\"target\" ref=\"s1\" category=\"plural_examples\">0~1000, 10000, 100000, "
        "1000000</note>",
        "name=\"exact\" pgs:switch=\"select:n\">",
        "pgs:case=\"1\"",
        "pgs:case=\"other\"",
        "name=\"pair\" pgs:switch=\"plural:n plural:m\">",
        "ref=\"s8\" category=\"plural_examples\">$n: 0, 0.0</note>",
        "ref=\"s9\" category=\"plural_examples\">$n: 0, 0.0; $m: 0, 0.0</note>",
        "<segment id=\"s8\" pgs:case=\"zero 0\">\n        <source>z</source>",
        "<segment id=\"s49\" pgs:case=\"other other\">",
        "name=\"sel\" pgs:switch=\"select:s\">",
        "pgs:case=\"10\"",
        "pgs:case=\"9\"",
        "pgs:case=\"a|b\"",
        "pgs:case=\"other\"",
        "name=\"four\" pgs:switch=\"select:a select:b select:c select:d\">",
        "<segment id=\"s1\" pgs:case=\"other other other other\">",
        "<unit id=\"u8\" "
        "name=\"five\">\n      <originalData>",
        "<unit id=\"u9\" name=\"plain\">\n      <originalData>"};
    char* y = Read("ar.xlf");
    CHECK(y != nullptr && InOrder(y, more, sizeof(more) / sizeof(more[0])) &&
              strstr(y, "s50") == nullptr,
          "notes to their runs' ends; exact selection; two plurals, each value once, zero "
          "kept, notes naming each variable; select keys in byte order, unquoted; four "
          "selectors carried, five and none not");
    free(y);
    free(x);
}

static void TestSelectionImport(void)
{
    // ar.xlf from the export above, with the many segment translated anew.
    char* x = Read("ar.xlf");
    char* many = x != nullptr ? strstr(x, "pgs:case=\"many\"") : nullptr;
    char* target = many != nullptr ? strstr(many, "milaff</target>") : nullptr;
    if (target != nullptr)
    {
        memcpy(target, "MILAFF", 6);
        Write("ar-many.xlf", x);
    }
    Write("ar.mf2", "@locale ar\n---\n");
    char* t = Exchange(true, "ar.mf2", "ar-many.xlf") ? Read("ar.mf2") : nullptr;
    uint64_t hash = 0;
    const char* files = strstr(s_selection, ".input {$n");
    const char* end = strstr(s_selection, "place =");
    char record[64] = "";
    if (SourceHash(files, (size_t)(end - files), &hash))
    {
        char digits[sourceHashDigits + 1];
        SourceHashText(hash, digits);
        snprintf(record, sizeof(record), "@mloc-source %s\nfiles = ", digits);
    }
    CHECK(t != nullptr && strstr(t, record) != nullptr &&
              strstr(t, ".input {$n :integer}\n  .match $n\n  0 {{la milaffat}}\n"
                        "  zero {{{$n} milaff}}\n  one {{milaff}}\n  two {{milaffan}}\n"
                        "  few {{{$n} milaffat}}\n  many {{{$n} MILAFF}}\n"
                        "  * {{{$n} milaff}}\n") != nullptr &&
              strstr(t, "place") == nullptr,
          "segments folded into one message in their order, other as '*', the source's "
          "hash recorded, untranslated units left out");
    free(t);
    free(x);
}

// Replaces the first from in an export with to and imports it.
static bool ImportEdited(const char* x, const char* from, const char* to)
{
    const char* at = x != nullptr ? strstr(x, from) : nullptr;
    if (at == nullptr)
    {
        return true;
    }
    size_t head = (size_t)(at - x);
    char* edited = malloc(strlen(x) + strlen(to) + 1);
    if (edited == nullptr)
    {
        return true;
    }
    snprintf(edited, strlen(x) + strlen(to) + 1, "%.*s%s%s", (int)head, x, to, at + strlen(from));
    Write("edited.xlf", edited);
    free(edited);
    Write("ar.mf2", "@locale ar\n---\n");
    bool ok = Exchange(true, "ar.mf2", "edited.xlf");
    char* t = Read("ar.mf2");
    ok = ok || t == nullptr || strcmp(t, "@locale ar\n---\n") != 0;
    free(t);
    return ok;
}

static void TestSelectionKeys(void)
{
    // A case value MF2 cannot write bare is quoted; one ending in an
    // exponent's mark is no number (read to its end, not past it).
    char* x = Read("ar.xlf");
    static const char* const values[] = {"1e",  "1e-", "1.", "9",   "1.5", "1e-5",
                                         "x-y", "_a",  "Ab", "a/b", "a|b", "a b|c"};
    static const char* const keys[] = {"|1e|", "|1e-|", "|1.|", "9",     "1.5",     "1e-5",
                                       "x-y",  "_a",    "Ab",   "|a/b|", "|a\\|b|", ""};
    for (size_t i = 0; x != nullptr && i < 12; i++)
    {
        char to[64];
        snprintf(to, sizeof(to), "pgs:case=\"%s\"", values[i]);
        bool refused = ImportEdited(x, "pgs:case=\"0\"", to) == false;
        char* t = Read("ar.mf2");
        char want[64];
        snprintf(want, sizeof(want), "\n  %s {{la milaffat}}", keys[i]);
        CHECK(i == 11 ? refused : !refused && t != nullptr && strstr(t, want) != nullptr,
              "a case value written as a key: quoted when not a name or a number");
        free(t);
    }
    free(x);
}

static void TestSelectionRefusals(void)
{
    char* x = Read("ar.xlf");
    CHECK(x != nullptr && !ImportEdited(x, "pgs:switch=\"plural:n\"", "pgs:switch=\"plural:m\""),
          "a switch other than the source's refused, nothing written");
    CHECK(!ImportEdited(x, "name=\"files\"", "name=\"spaced\""),
          "a switch for a source PGS cannot carry refused");
    CHECK(!ImportEdited(x, " pgs:case=\"zero\"", ""), "a segment without a case refused");
    CHECK(!ImportEdited(x, "pgs:case=\"zero\"", "pgs:case=\"zero one\""),
          "a case of two values for one selector refused");
    CHECK(!ImportEdited(x, "<target>milaffan</target>", ""), "a unit translated in part refused");
    free(x);
}

// A select of count keys and '*' as a resource entry.
static void AppendSelect(char* out, size_t room, const char* key, int count)
{
    size_t at = strlen(out);
    at += (size_t)snprintf(out + at, room - at, "%s = .input {$s :string}\n .match $s\n", key);
    for (int i = 0; i < count; i++)
    {
        at += (size_t)snprintf(out + at, room - at, " k%d {{%d}}\n", i, i);
    }
    (void)snprintf(out + at, room - at, " * {{x}}\n");
}

static void TestSelectionBound(void)
{
    static char source[16384];
    snprintf(source, sizeof(source), "@locale en\n---\n");
    AppendSelect(source, sizeof(source), "most", xliffPgsMaxSegments - 1);
    AppendSelect(source, sizeof(source), "over", xliffPgsMaxSegments);
    Write("en.mf2", source);
    Write("ar.mf2", "@locale ar\n---\n");
    char* x = Exchange(false, "ar.mf2", "big.xlf") ? Read("big.xlf") : nullptr;
    CHECK(x != nullptr && strstr(x, "name=\"most\" pgs:switch=\"select:s\"") != nullptr &&
              strstr(x, "<segment id=\"s256\"") != nullptr &&
              strstr(x, "name=\"over\">") != nullptr,
          "256 segments carried, 257 left as one segment");
    free(x);
}

static void TestSelectionQuotedLineFeed(void)
{
    // A quoted key may hold a LF: a variant's keys run from the end of the
    // .match line or of the variant before, not from the last LF.
    static const char* const texts[] = {".input {$s :string}\n.match $s\n* {{x}}\n|b\nc| {{y}}\n",
                                        ".input {$s :string}\n.match $s\n|b\nc| {{y}}\n* {{x}}\n"};
    for (size_t t = 0; t < 2; t++)
    {
        PgsMessage m;
        bool read = PgsRead(texts[t], strlen(texts[t]), &m);
        size_t quoted = t == 0 ? 1 : 0;
        CHECK(read && m.selectorCount == 1 && m.variantCount == 2 &&
                  strcmp(m.variants[1 - quoted].keys[0], "*") == 0 &&
                  strcmp(m.variants[quoted].keys[0], "|b\nc|") == 0 &&
                  strcmp(m.variants[quoted].cases[0], "b\nc") == 0,
              "a quoted key with a LF read whole, last and first");
        if (!read)
        {
            continue;
        }
        char* const* keys[] = {m.variants[0].keys, m.variants[1].keys};
        char* patterns[] = {m.variants[0].pattern, m.variants[1].pattern};
        size_t length = 0;
        char* again = PgsWrite(m.head, keys, patterns, 2, 1, &length);
        PgsMessage n = {0};
        CHECK(again != nullptr && PgsRead(again, length, &n) && n.variantCount == 2 &&
                  strcmp(n.variants[quoted].keys[0], "|b\nc|") == 0,
              "a message with a LF in a key written back reads the same");
        PgsFree(&n);
        free(again);
        PgsFree(&m);
    }
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    TestExport();
    TestImport();
    TestRefusals();
    TestExportEdges();
    TestRoundTrip();
    TestSelectionExport();
    TestSelectionImport();
    TestSelectionKeys();
    TestSelectionRefusals();
    TestSelectionBound();
    TestSelectionQuotedLineFeed();
    return s_failures == 0 ? 0 : 1;
}
