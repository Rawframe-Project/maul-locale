// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Stale translations (record mloc-0056): the source hash of a message's
// canonical text, its hex form, mloccat accept writing it into a
// translation, and the compiler counting what went stale. Run with a
// directory to write into.

#include "accept.h"
#include "compile.h"
#include "file.h"
#include "stale.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_directory;

static uint64_t Hash(const char* text)
{
    uint64_t hash = 0;
    return SourceHash(text, strlen(text), &hash) ? hash : 0;
}

static void TestHash(void)
{
    // The values of the reference XXH64 (its Python binding) over the
    // canonical text: they are written into files, so they never change.
    CHECK(Hash("Hello {$name}") == 0x6A037F79ECE2314BU && Hash("Bye") == 0xF07932A934038771U,
          "the hash is XXH64 of the canonical text");
    CHECK(Hash("Hello {  $name  }") == Hash("Hello {$name}") &&
              Hash("{{Hello {$name}}}") == Hash("Hello {$name}") &&
              Hash("{|a| :string}") == Hash("{a :string}"),
          "whitespace and quoting do not count");
    CHECK(Hash("Hello {$name}!") != Hash("Hello {$name}") &&
              Hash("{$n :number minimumFractionDigits=1}") !=
                  Hash("{$n :number minimumFractionDigits=2}") &&
              Hash(".input {$n :integer}\n.match $n\none {{a}}\n* {{b}}") !=
                  Hash(".input {$n :integer}\n.match $n\none {{a}}\n* {{c}}"),
          "text, options and variants do");
    uint64_t hash = 0;
    CHECK(!SourceHash("{$", 2, &hash), "a message that does not compile has no hash");
    char text[sourceHashDigits + 1];
    SourceHashText(0x0123456789ABCDEFU, text);
    uint64_t back = 0;
    CHECK(strcmp(text, "0123456789abcdef") == 0 && SourceHashRead(text, 16, &back) &&
              back == 0x0123456789ABCDEFU,
          "16 lowercase hex digits, read back");
    CHECK(!SourceHashRead("0123456789ABCDEF", 16, &back) &&
              !SourceHashRead("0123456789abcde", 15, &back) &&
              !SourceHashRead("0123456789abcdeg", 16, &back) &&
              !SourceHashRead("0123456789abcdef0", 17, &back) &&
              !SourceHashRead("/:`g/:`g/:`g/:`g", 16, &back),
          "capitals, fifteen or seventeen digits, letters past f refused");
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

// Compiles en and tr and gives the report's text.
static char* Compile(void)
{
    char en[1024];
    char tr[1024];
    char out[1024];
    Path(en, sizeof(en), "en.mf2");
    Path(tr, sizeof(tr), "tr.mf2");
    Path(out, sizeof(out), ".");
    char* paths[] = {en, tr};
    FILE* report = tmpfile();
    if (report == nullptr)
    {
        return nullptr;
    }
    char* said = calloc(4096, 1);
    if (said != nullptr && CatalogCompile(out, paths, 2, report))
    {
        rewind(report);
        size_t read = fread(said, 1, 4095, report);
        said[read] = '\0';
    }
    fclose(report);
    return said;
}

static int Accept(char* const* keys, size_t count)
{
    char en[1024];
    char tr[1024];
    Path(en, sizeof(en), "en.mf2");
    Path(tr, sizeof(tr), "tr.mf2");
    FILE* report = tmpfile();
    int ok = CatalogAccept(en, tr, keys, count, report != nullptr ? report : stderr);
    if (report != nullptr)
    {
        fclose(report);
    }
    return ok;
}

static void TestAccept(void)
{
    Write("en.mf2", "@locale en\n---\nhello = Hello {$name}\nbye = Bye\n@translate no\n"
                    "brand = Maul\n");
    Write("tr.mf2", "@locale tr\n---\n# Selam\nhello = Merhaba {$name}\n"
                    "@mloc-source 0000000000000000\nbye = Ho\xC5\x9F\xC3\xA7\x61 kal\n"
                    "brand = Moul\nextra = Fazla\n");
    char* before = Compile();
    CHECK(Count(before, "1 stale") == 1, "a recorded hash of no source message is stale");
    free(before);
    char* nope[] = {"nope"};
    char* brand[] = {"brand"};
    char* unchanged = Read("tr.mf2");
    CHECK(!Accept(nope, 1) && !Accept(brand, 1), "a key not in both, or kept, is refused");
    char* after = Read("tr.mf2");
    CHECK(unchanged != nullptr && after != nullptr && strcmp(unchanged, after) == 0,
          "a refused accept leaves the file as it was");
    free(unchanged);
    free(after);
    char* hello[] = {"hello"};
    CHECK(Accept(hello, 1), "one key accepted");
    char* one = Read("tr.mf2");
    CHECK(Count(one, "@mloc-source 6a037f79ece2314b") == 1 &&
              Count(one, "@mloc-source 0000000000000000") == 1,
          "the named key only");
    free(one);
    CHECK(Accept(nullptr, 0), "every entry accepted");
    char* all = Read("tr.mf2");
    CHECK(Count(all, "@mloc-source") == 2 && Count(all, "@mloc-source f07932a934038771") == 1 &&
              Count(all, "# Selam") == 1 && Count(all, "extra = Fazla") == 1 &&
              Count(all, "brand = Moul") == 1,
          "each hash once, replaced not added; the kept and the orphan untouched; comments kept");
    free(all);
    char* fresh = Compile();
    CHECK(Count(fresh, "0 stale") == 2, "nothing stale once accepted");
    free(fresh);
    Write("en.mf2", "@locale en\n---\nhello = Hello {  $name  }\nbye = Goodbye\n@translate no\n"
                    "brand = Maul\n");
    char* changed = Compile();
    CHECK(Count(changed, "tr, 3 of 3 messages, 1 keys left out, 1 stale") == 1,
          "a source text changed is stale, its whitespace not");
    free(changed);
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    TestHash();
    TestAccept();
    return s_failures == 0 ? 0 : 1;
}
