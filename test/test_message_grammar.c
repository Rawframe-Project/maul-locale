// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloc:case and mloc:particle (maul-locale/message.h): names, common
// nouns, literals, hints by variable and numbers as their formatters write
// them in Turkish, Finnish and Hungarian; Korean particles; the faults of
// a bad or refused request, a locale without rules and an operand neither
// text nor number; mloc:gender selecting on a list of persons by each
// rule.

#include "test_harness.h"

#include "maul-locale/message.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static uint8_t s_compiled[1 << 16];
static uint8_t s_bound[1 << 16];
static char s_text[1 << 10];

static const mlocMessageFaultKind s_badOption[] = {mloc_messageFaultBadOption, 0};
static const mlocMessageFaultKind s_badOperand[] = {mloc_messageFaultBadOperand, 0};
static const mlocMessageFaultKind s_unsupported[] = {mloc_messageFaultUnsupportedOperation, 0};

// Whether a message formats in a locale to a text with the faults given
// in order (0 ending the list), its parts reported when parts is set.
static bool Formats(const char* tag, const char* message, const mlocMessageArgument* arguments,
                    size_t count, const char* text, const mlocMessageFaultKind* expected)
{
    static mlocMessageFormatter formatter;
    mlocLocale locale;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    if (mlocParseLocale(tag, strlen(tag), &locale).status != mloc_success ||
        mlocInitMessageFormatter(&s_data, &locale, &options, &formatter) != mloc_success ||
        mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocMessageFormatter_Bind(&formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
            mloc_success)
    {
        printf("  [%s] does not compile and bind in %s\n", message, tag);
        return false;
    }
    mlocMessageFault list[8];
    mlocMessageFaults faults = {list, 8, 0};
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    mlocResult result = mlocFormatMessage(s_bound, arguments, count, &output, &faults);
    size_t expectedCount = 0;
    while (expected != NULL && expected[expectedCount] != mloc_messageFaultNone)
    {
        expectedCount++;
    }
    bool good = result == (expectedCount > 0 ? mloc_withFallbacks : mloc_success) &&
                output.length == strlen(text) && memcmp(s_text, text, output.length) == 0 &&
                faults.count == expectedCount;
    for (size_t i = 0; good && i < expectedCount; i++)
    {
        good = list[i].kind == expected[i];
    }
    if (!good)
    {
        printf("  [%s] in %s -> %d [%.*s], %zu faults", message, tag, (int)result,
               (int)output.length, s_text, faults.count);
        for (size_t i = 0; i < faults.count && i < 8; i++)
        {
            printf(" %d", (int)list[i].kind);
        }
        printf("\n");
    }
    return good;
}

static mlocMessageArgument Text(const char* name, const char* text)
{
    return (mlocMessageArgument){
        name, strlen(name), {mloc_messageValueString, 0, {.string = {text, strlen(text)}}}};
}

static void TestCase(void)
{
    mlocMessageArgument name = Text("n", "Rodrigo");
    CHECK(Formats("tr", "{$n :mloc:case case=dative}|{$n :mloc:case case=locative}", &name, 1,
                  "Rodrigo'ya|Rodrigo'da", NULL) &&
              Formats("tr", "{$n :mloc:case case=genitive possessor=3}", &name, 1, "Rodrigo'sunun",
                      NULL) &&
              Formats("tr", "{$n :mloc:case case=locative copula=true question=true}", &name, 1,
                      "Rodrigo'dadır mı", NULL),
          "a name in Turkish's cases, a possessor, the copula and the question");
    mlocMessageArgument word = Text("w", "kitap");
    CHECK(Formats("tr", "{$w :mloc:case case=accusative noun=common number=plural}", &word, 1,
                  "kitapları", NULL) &&
              Formats("tr", "{kitap :mloc:case case=dative noun=common}", NULL, 0, "kitaba", NULL),
          "a common noun, a literal operand");
    mlocMessageArgument hinted[] = {Text("n", "Jacques"), Text("h", "jak")};
    CHECK(Formats("tr", "{$n :mloc:case case=dative hint=$h}", hinted, 2, "Jacques'a", NULL),
          "a hint by a variable");
    mlocMessageArgument year = {"y", 1, {mloc_messageValueInteger, 0, {.integer = 1985}}};
    CHECK(Formats("tr", ".local $i = {$y :integer} {{{$i :mloc:case case=locative}}}", &year, 1,
                  "1.985'te", NULL) &&
              Formats("tr", "{$y :mloc:case case=locative}", &year, 1, "1.985'te", NULL) &&
              Formats("tr",
                      ".local $i = {$y :integer useGrouping=never} "
                      "{{{$i :mloc:case case=ablative}}}",
                      &year, 1, "1985'ten", NULL),
          "a number as its formatter writes it; a numeric argument as :number");
    mlocMessageArgument city = Text("c", "Helsinki");
    mlocMessageArgument house = Text("h", "ház");
    CHECK(Formats("fi", "{$c :mloc:case case=inessive}", &city, 1, "Helsingissä", NULL) &&
              Formats("hu", "{$h :mloc:case case=inessive noun=common}", &house, 1, "házban", NULL),
          "Finnish and Hungarian");
}

static void TestParticle(void)
{
    mlocMessageArgument book = Text("b", "책");
    mlocMessageArgument apple = Text("a", "Apple");
    mlocMessageArgument hinted[] = {apple, Text("h", "애플")};
    CHECK(
        Formats("ko", "{$b :mloc:particle particle=topic}|{$b :mloc:particle particle=direction}",
                &book, 1, "책은|책으로", NULL) &&
            Formats("ko", "{$a :mloc:particle particle=subject}", &apple, 1, "Apple이(가)", NULL) &&
            Formats("ko", "{$a :mloc:particle particle=subject hint=$h}", hinted, 2, "Apple이",
                    NULL),
        "Korean particles, both forms for an unread end, a hint");
    mlocMessageArgument year = {"y", 1, {mloc_messageValueInteger, 0, {.integer = 21}}};
    CHECK(Formats("ko", "{$y :mloc:particle particle=object}", &year, 1, "21을", NULL),
          "a number by its reading");
}

static void TestFaults(void)
{
    mlocMessageArgument name = Text("n", "Rodrigo");
    static const mlocListItem items[] = {{"a", 1}};
    mlocMessageArgument list = {"l", 1, {mloc_messageValueList, 0, {.list = {items, 1}}}};
    mlocMessageArgument bad[] = {name, Text("c", "dative")};
    CHECK(Formats("tr", "{$n :mloc:case case=dativ}", &name, 1, "Rodrigo", s_badOption) &&
              Formats("tr", "{$n :mloc:case case=$c}", bad, 2, "Rodrigo", s_badOption),
          "an unknown or variable case is a bad option, ignored as MF2 ignores one");
    CHECK(Formats("tr", "{$n :mloc:case case=partitive}", &name, 1, "{$n}", s_badOption) &&
              Formats("tr", "{$n :mloc:particle particle=topic}", &name, 1, "{$n}", s_badOption),
          "a request the language refuses is a bad option");
    CHECK(Formats("en", "{$n :mloc:case case=dative}", &name, 1, "{$n}", s_unsupported) &&
              Formats("tr", "{$l :mloc:case case=dative}", &list, 1, "{$l}", s_badOperand),
          "a locale without rules; an operand neither text nor number");
}

// The message that selects on a list of persons' gender.
static const char s_select[] =
    ".input {$p :mloc:gender} .match $p male {{He}} female {{She}} other {{They}} * {{?}}";

static mlocMessageArgument Persons(const mlocListItem* items, size_t count)
{
    return (mlocMessageArgument){"p", 1, {mloc_messageValueList, 0, {.list = {items, count}}}};
}

static void TestGender(void)
{
    static const mlocListItem mixed[] = {{"male", 4}, {"female", 6}};
    static const mlocListItem men[] = {{"male", 4}, {"male", 4}, {"male", 4}};
    static const mlocListItem women[] = {{"female", 6}, {"female", 6}};
    static const mlocListItem one[] = {{"male", 4}};
    static const mlocListItem unknown[] = {{"other", 5}, {"female", 6}};
    mlocMessageArgument a = Persons(mixed, 2);
    mlocMessageArgument m = Persons(men, 3);
    mlocMessageArgument w = Persons(women, 2);
    mlocMessageArgument o = Persons(one, 1);
    mlocMessageArgument u = Persons(unknown, 2);
    CHECK(Formats("en", s_select, &a, 1, "They", NULL) &&
              Formats("en", s_select, &m, 1, "They", NULL) &&
              Formats("en", s_select, &o, 1, "He", NULL),
          "neutral: a list is other, one person that person");
    CHECK(Formats("el", s_select, &m, 1, "He", NULL) &&
              Formats("el", s_select, &w, 1, "She", NULL) &&
              Formats("el", s_select, &a, 1, "They", NULL),
          "mixed neutral");
    CHECK(Formats("fr", s_select, &a, 1, "He", NULL) &&
              Formats("fr", s_select, &w, 1, "She", NULL) &&
              Formats("fr", s_select, &u, 1, "He", NULL) &&
              Formats("he", s_select, &a, 1, "He", NULL),
          "male taints, an unknown person among them");
    mlocMessageArgument single = Text("p", "female");
    mlocMessageArgument neither = Text("p", "other");
    CHECK(Formats("fr", s_select, &neither, 1, "They", NULL),
          "one person of neither gender stays other in a male-taints locale");
    CHECK(Formats("fr", s_select, &single, 1, "She", NULL) &&
              Formats("en", ".local $g = {female :mloc:gender} .match $g female {{She}} * {{?}}",
                      NULL, 0, "She", NULL),
          "one person's text, a literal");
    static const mlocListItem bad[] = {{"male", 4}, {"man", 3}};
    mlocMessageArgument b = Persons(bad, 2);
    CHECK(Formats("en", s_select, &b, 1, "?",
                  (const mlocMessageFaultKind[]){mloc_messageFaultBadOperand,
                                                 mloc_messageFaultBadSelector, 0}) &&
              Formats("en", ".input {$p :mloc:gender} .match $p woman {{W}} * {{?}}", &a, 1, "?",
                      (const mlocMessageFaultKind[]){mloc_messageFaultBadVariantKey, 0}) &&
              Formats("en", "{$p :mloc:gender}", &a, 1, "{$p}", s_unsupported),
          "a bad person, a bad key; a gender has no text");
}

static bool Open(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t size = file == NULL ? 0 : fread(buffer, 1, capacity, file);
    if (file != NULL)
    {
        fclose(file);
    }
    if (size == 0 || size >= capacity || mlocOpenData(buffer, size, out) != mloc_success)
    {
        fprintf(stderr, "test_message_grammar: cannot open %s\n", path);
        return false;
    }
    return true;
}

int main(void)
{
    static mlocData sources[2];
    alignas(8) static uint8_t spellout[8 << 20];
    mlocOpenBuiltinData(&sources[0]);
    if (!Open(MLOC_SPELLOUT_FILE, spellout, sizeof spellout, &sources[1]))
    {
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    TestCase();
    TestParticle();
    TestFaults();
    TestGender();
    return s_failures == 0 ? 0 : 1;
}
