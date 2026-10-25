// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's XML reader: what CLDR's files use is read, anything
// else is refused with its line.

#include "test_harness.h"
#include "xml.h"

#include <stdlib.h>
#include <string.h>

static XmlNode* Parse(const char* text, char* error, size_t capacity)
{
    return XmlParse(text, strlen(text), error, capacity);
}

static void TestElementsAttributesAndText(void)
{
    char error[256] = "";
    XmlNode* root = Parse("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
                          "<!DOCTYPE ldml SYSTEM \"../../common/dtd/ldml.dtd\">\n"
                          "<!-- a comment -->\n"
                          "<ldml>\n"
                          "  <identity><language type='tr'/></identity>\n"
                          "  <symbols numberSystem=\"latn\"><decimal>,</decimal></symbols>\n"
                          "</ldml>\n",
                          error, sizeof(error));
    CHECK(root != nullptr, "a CLDR-shaped document parses");
    if (root == nullptr)
    {
        return;
    }
    CHECK(strcmp(root->name, "ldml") == 0 && root->childCount == 2, "root and its children");
    XmlNode* identity = XmlFindChild(root, "identity");
    XmlNode* language = identity == nullptr ? nullptr : XmlFindChild(identity, "language");
    CHECK(language != nullptr && strcmp(XmlGetAttribute(language, "type"), "tr") == 0,
          "single-quoted attribute");
    CHECK(language != nullptr && language->line == 5, "the start tag's line");
    XmlNode* symbols = XmlFindChild(root, "symbols");
    XmlNode* decimal = symbols == nullptr ? nullptr : XmlFindChild(symbols, "decimal");
    CHECK(decimal != nullptr && strcmp(decimal->text, ",") == 0 && decimal->textLength == 1,
          "character data");
    CHECK(XmlGetAttribute(symbols, "missing") == nullptr, "an absent attribute is null");
    CHECK(XmlFindChild(root, "missing") == nullptr, "an absent child is null");
    XmlFree(root);
}

static void TestReferencesAndCdata(void)
{
    char error[256] = "";
    XmlNode* root = Parse("<a x=\"&lt;&amp;&#x130;&#305;&quot;\" y=\"1\t2\n3\">"
                          "&gt;&apos;<![CDATA[&C<ç<<<Ç]]>&#x1F600;</a>",
                          error, sizeof(error));
    CHECK(root != nullptr, "references and CDATA parse");
    if (root == nullptr)
    {
        return;
    }
    CHECK(strcmp(XmlGetAttribute(root, "x"), "<&\xC4\xB0\xC4\xB1\"") == 0,
          "entities and character references in an attribute");
    CHECK(strcmp(XmlGetAttribute(root, "y"), "1 2 3") == 0,
          "white space in an attribute becomes spaces");
    XmlNode* spaced = Parse("<c value = \"x\" other =\n'y'/>", error, sizeof(error));
    CHECK(spaced != nullptr && strcmp(XmlGetAttribute(spaced, "value"), "x") == 0 &&
              strcmp(XmlGetAttribute(spaced, "other"), "y") == 0,
          "white space around '=' (supplemental/characters.xml)");
    XmlFree(spaced);
    CHECK(strcmp(root->text, ">'&C<\xC3\xA7<<<\xC3\x87\xF0\x9F\x98\x80") == 0,
          "CDATA is kept as written, references decoded");
    XmlFree(root);
}

static void TestBomAndDoctypeWithSubset(void)
{
    char error[256] = "";
    XmlNode* root =
        Parse("\xEF\xBB\xBF<!DOCTYPE a [ <!ENTITY e \"]>\"> ]><a/>", error, sizeof(error));
    CHECK(root != nullptr && strcmp(root->name, "a") == 0, "a BOM and an internal subset");
    XmlFree(root);
}

static void ExpectRefused(const char* text, int line, const char* reason, const char* label)
{
    char error[256] = "";
    XmlNode* root = Parse(text, error, sizeof(error));
    char prefix[32];
    snprintf(prefix, sizeof(prefix), "line %d: ", line);
    CHECK(root == nullptr, label);
    CHECK(strncmp(error, prefix, strlen(prefix)) == 0 && strstr(error, reason) != nullptr, label);
    if (root != nullptr)
    {
        XmlFree(root);
    }
}

static void TestRefusals(void)
{
    ExpectRefused("<a>\n<b></a>", 2, "does not close <b>", "a mismatched end tag");
    ExpectRefused("<a>\n\n<b>", 3, "<b> is not closed", "an element left open");
    ExpectRefused("<a x='1' x='2'/>", 1, "given twice", "a repeated attribute");
    ExpectRefused("<a>&nbsp;</a>", 1, "not one XML predefines", "an undefined entity");
    ExpectRefused("<a>&#xD800;</a>", 1, "does not allow", "a surrogate reference");
    ExpectRefused("<a>&#0;</a>", 1, "does not allow", "a NUL reference");
    ExpectRefused("<a>&#12a;</a>", 1, "malformed", "a malformed reference");
    ExpectRefused("<a>&amp</a>", 1, "not closed", "an unclosed reference");
    ExpectRefused("<a x=1/>", 1, "value was expected", "an unquoted value");
    ExpectRefused("<a x='1/>", 1, "not closed", "an unclosed value");
    ExpectRefused("<a/><b/>", 1, "after the root", "two roots");
    ExpectRefused("  ", 1, "no root element", "no root");
    ExpectRefused("<a><!-- open", 1, "comment is not closed", "an unclosed comment");
    ExpectRefused("<a><![CDATA[x", 1, "CDATA section is not closed", "an unclosed CDATA");
    ExpectRefused("<1a/>", 1, "name was expected", "a name starting with a digit");

    // An end tag cut off right after its name, in a buffer of exactly the
    // document's bytes: nothing after them is read.
    static const char cut[] = "<a></a";
    char* exact = malloc(sizeof(cut) - 1);
    CHECK(exact != nullptr, "a buffer for the cut end tag");
    if (exact != nullptr)
    {
        memcpy(exact, cut, sizeof(cut) - 1);
        char error[256] = "";
        XmlNode* root = XmlParse(exact, sizeof(cut) - 1, error, sizeof(error));
        CHECK(root == nullptr && strstr(error, "line 1: ") == error &&
                  strstr(error, "end tag of <a> is malformed") != nullptr,
              "an end tag cut off at the end of the input");
        XmlFree(root);
        free(exact);
    }

    // XML allows no U+0000, as written or as a reference, and a NUL would
    // cut a text or a value short.
    static const char* const nuls[] = {"<a>\nx\0y</a>", "<a>\n<b x='1\0' /></a>"};
    for (size_t i = 0; i < 2; i++)
    {
        char error[256] = "";
        size_t length = strlen(nuls[i]) + 1 + strlen(nuls[i] + strlen(nuls[i]) + 1);
        XmlNode* root = XmlParse(nuls[i], length, error, sizeof(error));
        CHECK(root == nullptr && strstr(error, "line 2: ") == error &&
                  strstr(error, "NUL") != nullptr,
              "a NUL byte in text or a value, at its line");
        XmlFree(root);
    }
}

static void TestDepthLimit(void)
{
    enum
    {
        depth = 300
    };
    static char text[depth * 7 + 1];
    char* p = text;
    for (int i = 0; i < depth; i++)
    {
        memcpy(p, "<a>", 3);
        p += 3;
    }
    for (int i = 0; i < depth; i++)
    {
        memcpy(p, "</a>", 4);
        p += 4;
    }
    *p = '\0';
    char error[256] = "";
    XmlNode* root = Parse(text, error, sizeof(error));
    CHECK(root == nullptr && strstr(error, "deeper than") != nullptr, "nesting is bounded");
}

static void TestMixedContent(void)
{
    char error[256] = "";
    XmlNode* root = Parse("<a>x<b/>y&amp;z<c>in</c><![CDATA[<w>]]></a>", error, sizeof(error));
    CHECK(root != nullptr && strcmp(root->text, "xy&z<w>") == 0 && root->childCount == 2 &&
              root->children[0]->textAt == 1 && root->children[1]->textAt == 4 &&
              root->textAt == 0 && strcmp(root->children[1]->text, "in") == 0,
          "each child stands where it began in its parent's text");
    XmlFree(root);
}

int main(void)
{
    TestElementsAttributesAndText();
    TestReferencesAndCdata();
    TestBomAndDoctypeWithSubset();
    TestRefusals();
    TestDepthLimit();
    TestMixedContent();
    return s_failures == 0 ? 0 : 1;
}
