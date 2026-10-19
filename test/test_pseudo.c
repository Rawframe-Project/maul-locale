// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Pseudo-locales (record mloc-0058): exact en-XA and ar-XB forms of
// simple, selecting and escaped messages; every message of the MF2 test
// suite that compiles keeps compiling in both forms, and its canonical
// text, with the pseudo marks taken away and accents undone, is the
// source's; mloccat pseudo writes catalog sources the set compiles. Run
// with a directory to write into.

#include "compile.h"
#include "file.h"
#include "pseudo.h"
#include "test_harness.h"

#include "maul-locale/message.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SuiteCase
{
    const char* file;
    const char* locale;
    bool bidi;
    const char* source;
    size_t length;
    const char* expected;
    const char* errors;
    const char* arguments;
} SuiteCase;

static const SuiteCase s_suite[] = {
#include "mf2_suite.inc"
};

static const char* s_directory;

static bool Gives(const char* source, PseudoKind kind, const char* expected)
{
    size_t length = 0;
    char* pseudo = PseudoMessage(source, strlen(source), kind, &length);
    bool same = pseudo != nullptr && length == strlen(expected) && strcmp(pseudo, expected) == 0;
    if (!same)
    {
        fprintf(stderr, "  %s -> %s\n", source, pseudo != nullptr ? pseudo : "(none)");
    }
    free(pseudo);
    return same;
}

static void TestForms(void)
{
    CHECK(
        Gives(
            "Hello, {$name}!", pseudoAccented,
            "[\xC4\xA4\xC3\xA9\xC4\xBC\xC4\xBC\xC3\xB6, {$name}!\xC2\xB7\xC2\xB7\xC2\xB7\xC2\xB7]"),
        "en-XA: letters accented, the placeholder kept, 40% padding in brackets");
    CHECK(Gives("Hello, {$name}!", pseudoBidi,
                "\xE2\x80\xAEHello, \xE2\x80\xAC{$name}\xE2\x80\xAE!\xE2\x80\xAC"),
          "ar-XB: each run between RLO and PDF, the placeholder outside");
    CHECK(Gives(".input {$n :integer}\n.match $n\none {{a {#b}x{/b}}}\n* {{{$n}}}", pseudoAccented,
                ".input {$n :integer}\n.match $n\none {{[\xC3\xA5 "
                "{#b}\xE1\xBA\x8B{/b}\xC2\xB7\xC2\xB7]}}"
                "\n* {{[{$n}]}}"),
          "a selector message: only its patterns, markup kept, a pattern of no text bracketed "
          "alone");
    CHECK(Gives(".input {$x :string}\n.match $x\n|{a}| {{A}}\n* {{B}}", pseudoAccented,
                ".input {$x :string}\n.match $x\n|{a}| {{[\xC3\x85\xC2\xB7]}}\n* "
                "{{[\xC6\x81\xC2\xB7]}}"),
          "a quoted key holding braces is not a pattern");
    CHECK(
        Gives("a \\{b\\} {|c d| :string} e", pseudoAccented,
              "[\xC3\xA5 \\{\xC6\x80\\} {|c d| :string} \xC3\xA9\xC2\xB7\xC2\xB7\xC2\xB7\xC2\xB7]"),
        "escapes kept as escapes and counted as one character; a literal untouched");
    CHECK(Gives("\xE2\x80\x8E  {{x}}", pseudoAccented, "\xE2\x80\x8E  {{[\xE1\xBA\x8B\xC2\xB7]}}"),
          "a quoted pattern after a mark and spaces");
    size_t length = 0;
    CHECK(PseudoMessage("{$", 2, pseudoAccented, &length) == nullptr,
          "a message that does not compile has no pseudo form");
}

// The canonical text of a message, accents undone and pseudo marks
// taken away.
static char* Plain(const char* text, size_t length)
{
    static uint8_t s_compiled[1 << 16];
    static char s_text[1 << 16];
    size_t size = 0;
    mlocOutput out = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    if (mlocCompileMessage(text, length, s_compiled, sizeof(s_compiled), &size, nullptr).status !=
            mloc_success ||
        mlocWriteMessageText(s_compiled, size, &out) != mloc_success)
    {
        return nullptr;
    }
    static const char* const s_marks[] = {"[", "]", "\xC2\xB7", "\xE2\x80\xAE", "\xE2\x80\xAC"};
    static const char s_letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static const char* const s_accented[52] = {
        "\xC3\x85",     "\xC6\x81",     "\xC3\x87", "\xC4\x90",     "\xC3\x89", "\xC6\x91",
        "\xC4\x9C",     "\xC4\xA4",     "\xC3\x8E", "\xC4\xB4",     "\xC4\xB6", "\xC4\xBB",
        "\xE1\xB9\x80", "\xC3\x91",     "\xC3\x96", "\xC3\x9E",     "\xC7\xAA", "\xC5\x94",
        "\xC5\xA0",     "\xC5\xA2",     "\xC3\x9B", "\xE1\xB9\xBC", "\xC5\xB4", "\xE1\xBA\x8A",
        "\xC3\x9D",     "\xC5\xBD",     "\xC3\xA5", "\xC6\x80",     "\xC3\xA7", "\xC3\xB0",
        "\xC3\xA9",     "\xC6\x92",     "\xC4\x9D", "\xC4\xA5",     "\xC3\xAE", "\xC4\xB5",
        "\xC4\xB7",     "\xC4\xBC",     "\xC9\xB1", "\xC3\xB1",     "\xC3\xB6", "\xC3\xBE",
        "\xC7\xAB",     "\xC5\x95",     "\xC5\xA1", "\xC5\xA3",     "\xC3\xBB", "\xE1\xB9\xBD",
        "\xC5\xB5",     "\xE1\xBA\x8B", "\xC3\xBD", "\xC5\xBE",
    };
    // A pattern quoted only because it begins with a dot or a space is
    // written bare once its first character is a bracket.
    size_t from = 0;
    size_t to = out.length;
    if (to >= 4 && memcmp(s_text, "{{", 2) == 0 && memcmp(s_text + to - 2, "}}", 2) == 0)
    {
        from = 2;
        to -= 2;
    }
    char* plain = malloc(out.length + 1);
    size_t at = 0;
    for (size_t i = from; plain != nullptr && i < to;)
    {
        size_t matched = 0;
        for (size_t m = 0; matched == 0 && m < sizeof(s_marks) / sizeof(s_marks[0]); m++)
        {
            size_t n = strlen(s_marks[m]);
            matched = i + n <= to && memcmp(s_text + i, s_marks[m], n) == 0 ? n : 0;
        }
        for (int k = 0; matched == 0 && k < 52; k++)
        {
            size_t n = strlen(s_accented[k]);
            if (i + n <= to && memcmp(s_text + i, s_accented[k], n) == 0)
            {
                plain[at++] = s_letters[k];
                matched = n;
            }
        }
        if (matched == 0)
        {
            plain[at++] = s_text[i];
            matched = 1;
        }
        i += matched;
    }
    if (plain != nullptr)
    {
        plain[at] = '\0';
    }
    return plain;
}

static void TestSuite(void)
{
    size_t compiled = 0;
    size_t kept = 0;
    for (size_t i = 0; i < sizeof(s_suite) / sizeof(s_suite[0]); i++)
    {
        const SuiteCase* c = &s_suite[i];
        char* source = Plain(c->source, c->length);
        if (source == nullptr)
        {
            continue;
        }
        compiled++;
        bool same = true;
        for (int kind = pseudoAccented; kind <= pseudoBidi; kind++)
        {
            size_t length = 0;
            char* pseudo = PseudoMessage(c->source, c->length, (PseudoKind)kind, &length);
            char* plain = pseudo != nullptr ? Plain(pseudo, length) : nullptr;
            same = same && plain != nullptr && strcmp(plain, source) == 0;
            free(plain);
            free(pseudo);
        }
        kept += same;
        if (!same)
        {
            fprintf(stderr, "  suite %s: %.*s\n", c->file, (int)c->length, c->source);
        }
        free(source);
    }
    // The suite's 461 rows hold 300 messages that compile.
    CHECK(compiled == 300 && kept == compiled,
          "every suite message that compiles keeps compiling, and its text, in both forms");
}

static void TestTool(void)
{
    char source[1024];
    snprintf(source, sizeof(source), "%s/en.mf2", s_directory);
    const char* text = "@locale en\n---\nhello = Hello {$name}\n@translate no\nbrand = Maul\n"
                       "@obsolete\nold = {$\n";
    (void)FileWrite(source, text, strlen(text));
    FILE* report = tmpfile();
    FILE* to = report != nullptr ? report : stderr;
    char xa[1024];
    char xb[1024];
    snprintf(xa, sizeof(xa), "%s/en-XA.mf2", s_directory);
    snprintf(xb, sizeof(xb), "%s/ar-XB.mf2", s_directory);
    char* paths[] = {source, xa, xb};
    bool ok = CatalogPseudo(s_directory, source, to) && CatalogCompile(s_directory, paths, 3, to);
    size_t size = 0;
    char* written = FileRead(xa, &size);
    CHECK(ok && written != nullptr && strstr(written, "@locale en-XA\n") != nullptr &&
              strstr(written, "brand = Maul\n") != nullptr &&
              strstr(written, "old = {$\n") != nullptr &&
              strstr(written, "hello = [\xC4\xA4\xC3\xA9\xC4\xBC\xC4\xBC\xC3\xB6 {$name}") !=
                  nullptr,
          "mloccat pseudo: the locale set, the kept and the obsolete as written, the set compiles");
    free(written);
    char missing[1024];
    snprintf(missing, sizeof(missing), "%s/none/x", s_directory);
    CHECK(!CatalogPseudo(missing, source, to) && !CatalogPseudo(s_directory, missing, to),
          "a directory that is not, a source that does not read");
    if (report != nullptr)
    {
        fclose(report);
    }
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    TestForms();
    TestSuite();
    TestTool();
    return s_failures == 0 ? 0 : 1;
}
