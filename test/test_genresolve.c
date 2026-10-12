// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's CLDR inheritance on a small tree of its own
// (test/cldr/common): parents, inheritance markers, draft statuses,
// root's aliases and the alt and count fallbacks, each in one case.
// tools/check_resolution.py checks the same resolver on all of CLDR.
//
// usage: test_genresolve <fixture common dir>

#include "cldr.h"
#include "test_harness.h"

#include <string.h>

static Cldr s_cldr;

// The resolved text for a chain, or "<none>".
static const char* Text(const char* locale, const char* chain)
{
    const Leaf* leaf = CldrResolve(&s_cldr, locale, chain, componentMain);
    return leaf == nullptr ? "<none>" : leaf->text;
}

static int Is(const char* locale, const char* chain, const char* expected)
{
    return strcmp(Text(locale, chain), expected) == 0;
}

#define SYMBOL(name)  "//ldml/numbers/symbols[@numberSystem=\"latn\"]/" name
#define MONTHS        "//ldml/dates/calendars/calendar[@type=\"gregorian\"]/months/"
#define FORMAT(width) MONTHS "monthContext[@type=\"format\"]/monthWidth[@type=\"" width "\"]/"
#define ALONE(width)  MONTHS "monthContext[@type=\"stand-alone\"]/monthWidth[@type=\"" width "\"]/"
#define HOUR(length)  "//ldml/units/unitLength[@type=\"" length "\"]/unit[@type=\"duration-hour\"]/"
#define LANGUAGE(rest) "//ldml/localeDisplayNames/languages/language[@type=" rest "]"

static void TestParents(void)
{
    CHECK(strcmp(CldrParent(&s_cldr, "xx_YY", componentMain), "xx") == 0, "truncation");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_ZZ", componentMain), "xx_YY") == 0, "an explicit parent");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_Cyrl", componentMain), "root") == 0,
          "a non-likely script falls to root");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_Grek", componentMain), "root") == 0,
          "and one that is not listed");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_Latn", componentMain), "xx") == 0,
          "the likely script truncates");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_Grek", componentCollation), "xx") == 0,
          "the rule is the main component's alone");
    CHECK(strcmp(CldrParent(&s_cldr, "xx", componentMain), "root") == 0, "a language to root");
    CHECK(CldrParent(&s_cldr, "root", componentMain) == nullptr, "root has no parent");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_YY", componentCollation), "root") == 0,
          "collation's own parents come first");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_ZZ", componentCollation), "xx") == 0,
          "and stand alone: the main ones do not apply");
    CHECK(strcmp(CldrParent(&s_cldr, "xx_Cyrl", componentCollation), "xx") == 0,
          "nor does the non-likely script's root");
}

static void TestVertical(void)
{
    CHECK(Is("xx", SYMBOL("decimal"), ","), "a locale's own value");
    CHECK(Is("xx", SYMBOL("nan"), "NaN"), "root's value");
    CHECK(Is("xx_ZZ", SYMBOL("decimal"), ","), "through an explicit parent");
    CHECK(Is("xx_Cyrl", SYMBOL("decimal"), "."), "a non-likely script skips its language");
    CHECK(Is("xx_YY_QQ", SYMBOL("decimal"), ","), "a locale without a file truncates");
    CHECK(Is("xx", SYMBOL("missing"), "<none>"), "a chain nobody has");
}

static void TestMarkersAndDrafts(void)
{
    CHECK(Is("xx_YY", SYMBOL("decimal"), ","), "the inheritance marker is skipped");
    CHECK(Is("xx_YY", SYMBOL("group"), "."), "an unconfirmed value is skipped");
    CHECK(Is("xx_YY", FORMAT("abbreviated") "month[@type=\"1\"]", "Jan."),
          "a contributed value is taken");
    CHECK(Is("xx_YY", FORMAT("abbreviated") "month[@type=\"2\"]", "February"),
          "a provisional value is skipped, then root's alias applies");
    CHECK(Is("xx_YY", LANGUAGE("\"en\""), "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85"),
          "the empty marker is a value");
}

static void TestAliases(void)
{
    CHECK(Is("xx", FORMAT("abbreviated") "month[@type=\"1\"]", "January"), "a sibling alias");
    CHECK(Is("root", FORMAT("abbreviated") "month[@type=\"2\"]", "M02"), "aliases in root");
    CHECK(Is("xx_YY", ALONE("abbreviated") "month[@type=\"1\"]", "Jan."),
          "an alias is looked up again from the locale asked for");
    CHECK(Is("xx_YY", ALONE("abbreviated") "month[@type=\"2\"]", "February"), "aliases chain");
    CHECK(Is("xx",
             "//ldml/dates/calendars/calendar[@type=\"buddhist\"]/months/"
             "monthContext[@type=\"format\"]/monthWidth[@type=\"wide\"]/month[@type=\"2\"]",
             "February"),
          "an alias on an ancestor element");
    CHECK(Is("xx", HOUR("narrow") "displayName", "hr"), "narrow units fall to short");
}

static void TestLaterals(void)
{
    CHECK(Is("xx", LANGUAGE("\"de\"][@alt=\"short\""), "Deutsch"), "alt falls away");
    CHECK(Is("xx", LANGUAGE("\"en\"][@menu=\"core\""), "English (core)"),
          "a distinguishing attribute other than alt stays");
    CHECK(Is("xx", HOUR("short") "unitPattern[@count=\"few\"]", "{0} hrs"),
          "a count falls to other");
    CHECK(Is("xx", HOUR("short") "displayName[@count=\"one\"]", "hr"),
          "a displayName's count falls to none");
    CHECK(Is("xx", HOUR("narrow") "unitPattern[@count=\"two\"]", "{0} hrs"),
          "an alias and a count fallback together");
}

static void TestValueAttributes(void)
{
    const Leaf* leaf = CldrResolve(&s_cldr, "xx_YY", "//ldml/layout/orientation", componentMain);
    CHECK(leaf != nullptr && leaf->valueCount == 2, "an empty element with value attributes");
    CHECK(leaf != nullptr && strcmp(LeafValue(leaf, "characters"), "right-to-left") == 0 &&
              strcmp(LeafValue(leaf, "lines"), "top-to-bottom") == 0,
          "their values");
    CHECK(leaf != nullptr && LeafValue(leaf, "missing") == nullptr, "an absent value");
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fputs("usage: test_genresolve <fixture common dir>\n", stderr);
        return 2;
    }
    char error[512] = "";
    if (!CldrOpen(&s_cldr, argv[1], error, sizeof(error)))
    {
        fprintf(stderr, "cannot open the fixture: %s\n", error);
        return 1;
    }
    TestParents();
    TestVertical();
    TestMarkersAndDrafts();
    TestAliases();
    TestLaterals();
    TestValueAttributes();
    CldrClose(&s_cldr);
    return s_failures == 0 ? 0 : 1;
}
