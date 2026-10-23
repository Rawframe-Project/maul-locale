// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The public collator over the built-in data and the collation data file:
// the root's defaults, each -u- key, options over the keys, values passed
// over, a locale's tailoring found by co, its default and its chain, the
// rules' settings under the keys, sort keys against the compare, the keys'
// version, and the refusals.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "test_harness.h"

#include "maul-locale/collate.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
// The built-in data, for the parents, then the collation file.
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};

// A collator for the locale with the options; false when not made.
static bool Make(const char* id, const mlocCollatorOptions* options, mlocCollator* out)
{
    mlocLocale locale;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitCollator(out, &s_data, &locale, options) == mloc_success;
}

// The order of a and b under the locale and options, checked against its
// mirror; 2 when the collator is not made or the two disagree.
static int Order(const char* id, const mlocCollatorOptions* options, const char* a, const char* b)
{
    mlocCollator c;
    int ab = 2;
    int ba = 2;
    if (!Make(id, options, &c) ||
        mlocCompare(&c, a, strlen(a), b, strlen(b), &ab) != mloc_success ||
        mlocCompare(&c, b, strlen(b), a, strlen(a), &ba) != mloc_success)
    {
        return 2;
    }
    return ab == -ba ? ab : 2;
}

// Han one (U+4E00, one stroke, yi) and a (U+963F, seven strokes, a).
#define HAN_ONE "\xE4\xB8\x80"
#define HAN_A   "\xE9\x98\xBF"

static void TestTailorings(void)
{
    CHECK(Order("es", nullptr, "nz",
                "\xC3\xB1"
                "a") == -1 &&
              Order("en", nullptr, "nz",
                    "\xC3\xB1"
                    "a") == 1 &&
              Order("es-MX", nullptr, "nz",
                    "\xC3\xB1"
                    "a") == -1,
          "es: n tilde a letter after n, through es-MX's chain; en the root's");
    CHECK(Order("de-u-co-phonebk", nullptr, "ad", "\xC3\xA4") == -1 &&
              Order("de", nullptr, "ad", "\xC3\xA4") == 1 &&
              Order("es-u-co-phonebk", nullptr, "nz",
                    "\xC3\xB1"
                    "a") == -1,
          "co: de's phonebook a umlaut as ae; a type the chain lacks gives the default");
    CHECK(Order("zh", nullptr, HAN_A, HAN_ONE) == -1 &&
              Order("zh-u-co-stroke", nullptr, HAN_ONE, HAN_A) == -1 &&
              Order("zh-Hant", nullptr, HAN_ONE, HAN_A) == -1 &&
              Order("yue", nullptr, HAN_ONE, HAN_A) == -1 &&
              Order("zh-u-co-standard", nullptr, HAN_ONE, HAN_A) == -1 &&
              Order("zh-u-co-phonebk", nullptr, HAN_A, HAN_ONE) == -1,
          "zh: pinyin by default, stroke by co, zh-Hant's and yue's default stroke; standard, "
          "which zh lacks, the root's; a type no unit has, the default pinyin");
    // Hiragana and katakana iteration marks (U+309D, U+30FD): equal in
    // ja, apart in the root's search.
    CHECK(Order("ja-u-co-searchjl", nullptr, "\xE3\x82\x9D", "\xE3\x83\xBD") == -1 &&
              Order("ja", nullptr, "\xE3\x82\x9D", "\xE3\x83\xBD") == 0 &&
              Order("ja-u-co-private-kana", nullptr, "\xE3\x82\x9D", "\xE3\x83\xBD") == 0,
          "a search type the chain lacks as search, the root's; a private type as none");
    CHECK(Order("nb", nullptr, "z", "\xC3\xA5") == -1 &&
              Order("nn", nullptr, "z", "\xC3\xA5") == -1 &&
              Order("und", nullptr, "z", "\xC3\xA5") == 1,
          "nb and nn: a ring after z, the rules of their parent no");
    // U+316B1 before U+25969 in the long pinyin list; the short one
    // names neither, so the root's code-point order holds.
    CHECK(Order("zh", nullptr, "\xF0\xA5\xA5\xA9", "\xF0\xB1\x9A\xB1") == -1 &&
              Order("zh", nullptr, HAN_A, "\xF0\xA5\xA5\xA9") == -1,
          "zh's short pinyin: Han it does not name after the Han it does, in the root's order");
    // U+2A6D9 just after U+4E00 in radical 1's strokes, U+4E01 after it.
    CHECK(Order("ja-u-co-unihan", nullptr, "\xF0\xAA\x9B\x99", "\xE4\xB8\x81") == -1 &&
              Order("ko-u-co-unihan", nullptr, "\xF0\xAA\x9B\x99", "\xE4\xB8\x81") == -1 &&
              Order("ja", nullptr, "\xF0\xAA\x9B\x99", "\xE4\xB8\x81") == 1,
          "unihan: Han by radical and stroke; the root by code point");
    CHECK(Order("da", nullptr, "A", "a") == -1 && Order("da-u-kf-lower", nullptr, "a", "A") == -1,
          "da's rules: upper case first, under kf");
    mlocCollatorOptions lower = {.caseFirst = mloc_caseFirstLower};
    CHECK(Order("da", &lower, "a", "A") == -1 && Order("ru", nullptr, "\xD1\x8F", "a") == -1 &&
              Order("ru-u-kr-latn", nullptr, "a", "\xD1\x8F") == -1,
          "the options over the rules; ru's rules reorder Cyrillic first, under kr");
}

// Thai sara e (U+0E40) before ko kai (U+0E01), and after it.
#define THAI_E_KO "\xE0\xB9\x80\xE0\xB8\x81"
#define THAI_KO_E "\xE0\xB8\x81\xE0\xB9\x80"

static void TestImportedRules(void)
{
    CHECK(Order("bs-Cyrl", nullptr, "\xD1\x8F", "a") == -1 &&
              Order("bs", nullptr, "a", "\xD1\x8F") == -1,
          "bs-Cyrl's rules import sr's, its reordering too");
    CHECK(Order("und", nullptr, THAI_E_KO, THAI_KO_E) == 0 &&
              Order("und-u-co-search", nullptr, THAI_E_KO, THAI_KO_E) == 1 &&
              Order("fi-u-co-search", nullptr, THAI_E_KO, THAI_KO_E) == 1,
          "search suppresses Thai's prevowel contractions, through imports too");
}

// Every type of every locale the collation file holds makes a collator:
// its reordering is one the root can make.
static void TestEveryType(void)
{
    static const char* const types[] = {"standard", "search", "searchjl", "phonebk", "trad",
                                        "dict",     "eor",    "emoji",    "compat",  "pinyin",
                                        "stroke",   "zhuyin", "unihan",   "phonetic"};
    size_t made = 0;
    size_t tried = 0;
    for (size_t i = 0; i < mlocGetDataLocaleCount(&s_sources[1]); i++)
    {
        const char* id = nullptr;
        size_t length = 0;
        if (mlocGetDataLocale(&s_sources[1], i, &id, &length) != mloc_success)
        {
            continue;
        }
        char base[64];
        bool root = length == 4 && memcmp(id, "root", 4) == 0;
        snprintf(base, sizeof(base), "%.*s", root ? 3 : (int)length, root ? "und" : id);
        for (char* p = base; *p != '\0'; p++)
        {
            *p = *p == '_' ? '-' : *p;
        }
        for (size_t t = 0; t < sizeof(types) / sizeof(types[0]); t++)
        {
            char tag[128];
            mlocCollator c;
            snprintf(tag, sizeof(tag), "%s-u-co-%s", base, types[t]);
            made += Make(tag, nullptr, &c);
            tried++;
        }
    }
    CHECK(tried > 1000 && made == tried, "every locale's every type makes a collator");
}

static void TestTailoringVersions(void)
{
    mlocCollator c;
    uint64_t root = 0;
    uint64_t en = 1;
    uint64_t es = 2;
    uint64_t esMx = 3;
    uint64_t ru = 4;
    CHECK(Make("und", nullptr, &c) && mlocGetCollationVersion(&c, &root) == mloc_success &&
              Make("en", nullptr, &c) && mlocGetCollationVersion(&c, &en) == mloc_success &&
              Make("es", nullptr, &c) && mlocGetCollationVersion(&c, &es) == mloc_success &&
              Make("es-MX", nullptr, &c) && mlocGetCollationVersion(&c, &esMx) == mloc_success &&
              Make("ru", nullptr, &c) && mlocGetCollationVersion(&c, &ru) == mloc_success &&
              root == en && es == esMx && es != root && ru != root && ru != es,
          "the version over the tailoring too: es-MX has es's");
    mlocDataSet alone = {&s_sources[1], 1};
    mlocLocale locale;
    CHECK(mlocParseLocale("es", 2, &locale).status == mloc_success &&
              mlocInitCollator(&c, &alone, &locale, nullptr) == mloc_errorNotFound &&
              mlocParseLocale("und", 3, &locale).status == mloc_success &&
              mlocInitCollator(&c, &alone, &locale, nullptr) == mloc_success,
          "a tailored locale needs the parents, the root does not");
}

static void TestDefaults(void)
{
    CHECK(Order("en", nullptr, "a", "b") == -1 && Order("en", nullptr, "a", "A") == -1 &&
              Order("en", nullptr, "a", "\xC3\xA1") == -1 &&
              Order("en", nullptr, "a b", "ab") == -1 && Order("en", nullptr, "a2", "a10") == 1 &&
              Order("en", nullptr, "\xCE\xB1", "a") == 1 && Order("en", nullptr, "", "") == 0,
          "the root's defaults: tertiary, non-ignorable, digits as text, no reordering");
    mlocCollator c;
    int order = 2;
    CHECK(Make("en", nullptr, &c) && mlocCompare(&c, nullptr, 0, "a", 1, &order) == mloc_success &&
              order == -1 &&
              mlocCompare(&c, "a\xFF", 2, "a\xEF\xBF\xBD", 4, &order) == mloc_success && order == 0,
          "an empty text as null, a bad byte as U+FFFD");
}

static void TestKeys(void)
{
    CHECK(Order("en-u-ks-level1", nullptr, "a", "\xC3\x81") == 0 &&
              Order("en-u-ks-level2", nullptr, "a", "A") == 0 &&
              Order("en-u-ks-level2", nullptr, "a", "\xC3\xA1") == -1 &&
              Order("en-u-ks-identic", nullptr, "\xC3\xA1", "a\xCC\x81") == 0,
          "ks: the strength");
    CHECK(Order("en-u-ka-shifted", nullptr, "a-b", "ab") == 0 &&
              Order("en-u-ka-shifted-ks-level4", nullptr, "a-b", "ab") == -1 &&
              Order("en-u-ka-shifted", nullptr, "a$b", "ab") == -1 &&
              Order("en-u-ka-shifted-kv-currency", nullptr, "a$b", "ab") == 0 &&
              Order("en-u-ka-shifted-kv-space", nullptr, "a-b", "ab") == -1,
          "ka and kv: shifted up to the max variable");
    CHECK(Order("en-u-kf-upper", nullptr, "A", "a") == -1 &&
              Order("en-u-kf-lower", nullptr, "a", "A") == -1 &&
              Order("en-u-kc-ks-level1", nullptr, "a", "A") == -1 &&
              Order("en-u-kc-false-ks-level1", nullptr, "a", "A") == 0,
          "kf and kc: case first and the case level, a key alone being true");
    CHECK(Order("en-u-kn", nullptr, "a2", "a10") == -1 &&
              Order("en-u-kn-false", nullptr, "a2", "a10") == 1 &&
              Order("fr-u-kb", nullptr, "cot\xC3\xA9", "c\xC3\xB4te") == 1 &&
              Order("fr", nullptr, "cot\xC3\xA9", "c\xC3\xB4te") == -1,
          "kn and kb: numeric and backwards secondaries");
    CHECK(Order("und-u-co-emoji-kn", nullptr, "a2", "a10") == -1 &&
              Order("und-u-co-emoji-kn", nullptr, "a2\xEF\xB8\x8F\xE2\x83\xA3", "a10") == -1 &&
              Order("und-u-co-emoji", nullptr, "a2", "a10") == 1,
          "kn over digits the emoji type gives contractions (keycaps), as ICU4C's digit tags");
    CHECK(Order("en-u-kr-grek-latn", nullptr, "\xCE\xB1", "a") == -1 &&
              Order("en-u-kr-digit-punct", nullptr, "1", ".") == -1 &&
              Order("en-u-kr-cyrl-others-latn", nullptr, "\xCE\xB1", "a") == -1,
          "kr: the scripts and groups reordered");
    CHECK(Order("en-u-ks-level9", nullptr, "a", "A") == -1 &&
              Order("en-u-kf-other", nullptr, "a", "A") == -1 &&
              Order("en-u-kr-grek-grek", nullptr, "\xCE\xB1", "a") == 1 &&
              Order("en-u-kr-qaaa", nullptr, "\xCE\xB1", "a") == 1,
          "values not a key's, or a kr the root cannot order, passed over");
}

static void TestOptions(void)
{
    mlocCollatorOptions o = {.strength = mloc_strengthPrimary};
    CHECK(Order("en-u-ks-level3", &o, "a", "A") == 0, "the options over the keys: strength");
    o = (mlocCollatorOptions){.numeric = mloc_switchOff};
    CHECK(Order("en-u-kn", &o, "a2", "a10") == 1, "the options over the keys: numeric off");
    o = (mlocCollatorOptions){.alternate = mloc_alternateShifted,
                              .maxVariable = mloc_maxVariableSymbol,
                              .caseFirst = mloc_caseFirstUpper,
                              .caseLevel = mloc_switchOn,
                              .backwards = mloc_switchOn};
    CHECK(Order("en", &o, "a+b", "ab") == 0 && Order("en", &o, "A", "a") == -1 &&
              Order("en", &o, "cot\xC3\xA9", "c\xC3\xB4te") == 1,
          "alternate, max variable, case first, case level and backwards as options");
    o = (mlocCollatorOptions){.reorder = "Grek-LATN", .reorderLength = 9};
    CHECK(Order("en", &o, "\xCE\xB1", "a") == -1, "a reorder option in any case");
    o = (mlocCollatorOptions){.reorder = "GREK-ZZZZ-LATN", .reorderLength = 14};
    mlocCollatorOptions groups = {.reorder = "currency-digit", .reorderLength = 14};
    CHECK(Order("en", &o, "\xCE\xB1", "\xD0\xB4") == -1 && Order("en", &o, "\xD0\xB4", "a") == -1 &&
              Order("en", &groups, "$", ".") == 1 && Order("en", &groups, "$", "1") == -1,
          "Zzzz written as a script, and the eight letters of currency");
    o = (mlocCollatorOptions){.strength = mloc_strengthIdentical};
    CHECK(Order("en-u-ks-level1", &o, "\xC3\xA1", "a\xCC\x81") == 0 &&
              Order("en-u-ks-level1", &o, "a", "A") == -1,
          "the identical strength as an option");
    o = (mlocCollatorOptions){.alternate = mloc_alternateShifted,
                              .maxVariable = mloc_maxVariablePunctuation};
    mlocCollatorOptions currency = {.alternate = mloc_alternateShifted,
                                    .maxVariable = mloc_maxVariableCurrency};
    CHECK(Order("en-u-kv-symbol", &o, "a+b", "ab") == -1 &&
              Order("en-u-kv-symbol", &o, "a-b", "ab") == 0 &&
              Order("en", &currency, "a$b", "ab") == 0,
          "the max variable as an option, over the locale's, up to currency");
    o = (mlocCollatorOptions){.caseLevel = mloc_switchOff};
    CHECK(Order("en-u-kc-ks-level1", &o, "a", "A") == 0,
          "the case level switched off as an option");
    o = (mlocCollatorOptions){.reorder = "", .reorderLength = 0};
    CHECK(Order("en-u-kr-grek", &o, "\xCE\xB1", "a") == 1, "an empty reorder option: none");
}

// The order of two texts' keys under the locale and options, as memcmp
// gives it; 2 when a key is not made.
static int KeyOrder(const char* id, const mlocCollatorOptions* options, const char* a,
                    const char* b)
{
    mlocCollator c;
    uint8_t keys[2][256];
    size_t lengths[2] = {0, 0};
    const char* texts[2] = {a, b};
    if (!Make(id, options, &c))
    {
        return 2;
    }
    for (int i = 0; i < 2; i++)
    {
        if (mlocGetSortKey(&c, texts[i], strlen(texts[i]), keys[i], sizeof(keys[i]), &lengths[i]) !=
                mloc_success ||
            lengths[i] == 0 || keys[i][lengths[i] - 1] != 0)
        {
            return 2;
        }
    }
    int r = memcmp(keys[0], keys[1], lengths[0] < lengths[1] ? lengths[0] : lengths[1]);
    r = r != 0 ? r : (lengths[0] > lengths[1]) - (lengths[0] < lengths[1]);
    return (r > 0) - (r < 0);
}

// Whether a pair orders as expected by the compare, its mirror and its
// keys.
static bool Both(const char* id, const char* a, const char* b, int expected)
{
    return Order(id, nullptr, a, b) == expected && KeyOrder(id, nullptr, a, b) == expected;
}

// The identical prefix skipped before the compares: a prefix context read
// back past the cut (the middle dot after l), a contraction and a number
// the cut would split, and the settings that skip none (shifted: an
// ignorable after a variable element reads as 0; backwards secondaries:
// the prefix's are read last). Each pair orders otherwise when its rule is
// left out.
static void TestIdenticalPrefix(void)
{
    CHECK(Both("und", "l\xC2\xB7z", "lb", 1) && Both("cs", "cz", "ch", -1) &&
              Both("en-u-kn", "a12a", "a123", -1),
          "a prefix context before the cut, a contraction and a number across it");
    CHECK(Both("en-u-ka-shifted", "[", "[\x05\xCC\xA3\x0C", 0) &&
              Both("fr-CA", "\xCC\xA3", "\xCC\xA3\x1F\xCC\xA3\xCC\x88\xCC\x88", 1),
          "shifted and backwards secondaries: no prefix skipped");
}

static void TestSortKeys(void)
{
    static const char* const pairs[][2] = {
        {"a", "b"},    {"a", "A"},        {"a", "\xC3\xA1"}, {"a-b", "ab"},
        {"a2", "a10"}, {"\xCE\xB1", "a"}, {"", "a"},         {"cot\xC3\xA9", "c\xC3\xB4te"},
        {"ab", "ab"},
    };
    static const char* const ids[] = {
        "en",           "en-u-ks-level1", "en-u-ka-shifted-ks-level4", "en-u-kn",
        "en-u-kr-grek", "fr-u-kb",        "en-u-ks-identic",           "en-u-kc-kf-upper"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
    {
        for (size_t j = 0; j < sizeof(pairs) / sizeof(pairs[0]); j++)
        {
            int compared = Order(ids[i], nullptr, pairs[j][0], pairs[j][1]);
            ok = ok && compared != 2 &&
                 KeyOrder(ids[i], nullptr, pairs[j][0], pairs[j][1]) == compared;
        }
    }
    CHECK(ok, "keys order texts as the compare does, under each key and level");
    mlocCollator c;
    uint8_t key[64];
    memset(key, 0xEE, sizeof(key));
    size_t full = 0;
    size_t again = 0;
    size_t empty = 0;
    CHECK(Make("en", nullptr, &c) &&
              mlocGetSortKey(&c, "abc", 3, nullptr, 0, &full) == mloc_errorCapacity && full > 3 &&
              mlocGetSortKey(&c, "abc", 3, key, full - 1, &again) == mloc_errorCapacity &&
              again == full && key[full - 1] == 0xEE &&
              mlocGetSortKey(&c, "abc", 3, key, full, &again) == mloc_success && again == full &&
              key[full - 1] == 0 && key[full] == 0xEE &&
              mlocGetSortKey(&c, nullptr, 0, key, sizeof(key), &empty) == mloc_success &&
              empty == 3,
          "a key's length when it does not fit, nothing past the room, an empty text's key");
    mlocCollator other;
    mlocCollator copy;
    uint64_t version = 0;
    uint64_t second = 1;
    uint64_t copied = 2;
    mlocCollatorOptions numeric = {.numeric = mloc_switchOn};
    bool made = Make("en", nullptr, &c) && Make("fr-u-kr-grek", &numeric, &other);
    memcpy(&copy, &c, sizeof(c));
    CHECK(made && mlocGetCollationVersion(&c, &version) == mloc_success && version != 0 &&
              mlocGetCollationVersion(&other, &second) == mloc_success && second == version &&
              mlocGetCollationVersion(&copy, &copied) == mloc_success && copied == version,
          "the keys' version: the same for any locale and options over the same data, and for "
          "a copy");
}

// UTF-16 compares and keys as UTF-8 does, supplementary characters and
// the fast compare's range too; a lone surrogate as U+FFFD.
static void TestUtf16(void)
{
    mlocCollator c;
    // The last three: U+E000 just past the surrogates, a lead surrogate
    // at the end, and two trail surrogates (each a lone one).
    static const char* const utf8[] = {"",
                                       "a",
                                       "b",
                                       "A",
                                       "\xC3\xA9",
                                       "ch",
                                       "c",
                                       "\xE2\x80\x90",
                                       "\xF0\x9F\x98\x80",
                                       "\xF0\xA0\x80\x80x",
                                       "\xEF\xBF\xBDz",
                                       "\xEE\x80\x80",
                                       "a\xEF\xBF\xBD",
                                       "\xEF\xBF\xBD\xEF\xBF\xBD"};
    static const uint16_t u0[] = {0};
    static const uint16_t u1[] = {'a'};
    static const uint16_t u2[] = {'b'};
    static const uint16_t u3[] = {'A'};
    static const uint16_t u4[] = {0xE9};
    static const uint16_t u5[] = {'c', 'h'};
    static const uint16_t u6[] = {'c'};
    static const uint16_t u7[] = {0x2010};
    static const uint16_t u8[] = {0xD83D, 0xDE00};
    static const uint16_t u9[] = {0xD840, 0xDC00, 'x'};
    static const uint16_t u10[] = {0xDC00, 'z'};
    static const uint16_t u11[] = {0xE000};
    static const uint16_t u12[] = {'a', 0xD83D};
    static const uint16_t u13[] = {0xDC00, 0xDC01};
    static const uint16_t* const utf16[] = {u0, u1, u2, u3,  u4,  u5,  u6,
                                            u7, u8, u9, u10, u11, u12, u13};
    static const size_t lengths[] = {0, 1, 1, 1, 1, 2, 1, 1, 2, 3, 2, 1, 2, 2};
    static const char* const tags[] = {"en", "cs", "en-u-ks-identic", "en-u-ka-shifted"};
    bool ok = true;
    for (size_t t = 0; ok && t < sizeof(tags) / sizeof(tags[0]); t++)
    {
        ok = Make(tags[t], nullptr, &c);
        for (size_t i = 0; ok && i < sizeof(utf8) / sizeof(utf8[0]); i++)
        {
            uint8_t k8[256];
            uint8_t k16[256];
            size_t n8 = 0;
            size_t n16 = 0;
            ok =
                mlocGetSortKey(&c, utf8[i], strlen(utf8[i]), k8, sizeof(k8), &n8) == mloc_success &&
                mlocGetSortKeyUtf16(&c, utf16[i], lengths[i], k16, sizeof(k16), &n16) ==
                    mloc_success &&
                n8 == n16 && memcmp(k8, k16, n8) == 0;
            for (size_t j = 0; ok && j < sizeof(utf8) / sizeof(utf8[0]); j++)
            {
                int o8 = 2;
                int o16 = 3;
                ok = mlocCompare(&c, utf8[i], strlen(utf8[i]), utf8[j], strlen(utf8[j]), &o8) ==
                         mloc_success &&
                     mlocCompareUtf16(&c, utf16[i], lengths[i], utf16[j], lengths[j], &o16) ==
                         mloc_success &&
                     o8 == o16;
            }
        }
    }
    CHECK(ok, "UTF-16 orders and keys as UTF-8, a lone surrogate as U+FFFD");
    int order = 0;
    size_t n = 0;
    CHECK(Make("en", nullptr, &c) &&
              mlocCompareUtf16(&c, nullptr, 0, nullptr, 0, &order) == mloc_success && order == 0 &&
              mlocCompareUtf16(&c, nullptr, 1, u1, 1, &order) == mloc_errorInvalid &&
              mlocCompareUtf16(&c, u1, 1, u1, 1, nullptr) == mloc_errorInvalid &&
              mlocGetSortKeyUtf16(&c, nullptr, 0, nullptr, 0, &n) == mloc_errorCapacity &&
              mlocGetSortKeyUtf16(&c, u1, 1, nullptr, 0, nullptr) == mloc_errorInvalid &&
              mlocGetSortKeyUtf16(nullptr, u1, 1, nullptr, 0, &n) == mloc_errorInvalid,
          "UTF-16 refusals: null texts with lengths, null outputs, no collator");
    uint8_t exact[64];
    size_t needed = 0;
    CHECK(mlocGetSortKeyUtf16(&c, u1, 1, nullptr, 0, &needed) == mloc_errorCapacity && needed > 0 &&
              needed <= sizeof(exact) &&
              mlocGetSortKeyUtf16(&c, u1, 1, exact, needed, &n) == mloc_success && n == needed &&
              mlocGetSortKeyUtf16(&c, u1, 1, exact, needed - 1, &n) == mloc_errorCapacity,
          "a UTF-16 key in a buffer of its size, refused one byte short");
}

static void TestRefusals(void)
{
    mlocLocale en;
    mlocCollator c;
    memset(&c, 0x5A, sizeof(c));
    mlocCollator before = c;
    bool ok = mlocParseLocale("en", 2, &en).status == mloc_success;
    static const char* const bad[] = {
        "grek-", "-grek", "zz", "grek--latn", "grek-grek", "hira-kana", "others-latn-others",
        "spaces"};
    for (size_t i = 0; ok && i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        mlocCollatorOptions o = {.reorder = bad[i], .reorderLength = strlen(bad[i])};
        ok = mlocInitCollator(&c, &s_data, &en, &o) == mloc_errorInvalid;
    }
    CHECK(ok && memcmp(&c, &before, sizeof(c)) == 0,
          "reorder texts that are no list of codes, or name one twice, refused, the collator "
          "left as it was");
    mlocCollatorOptions full = {.reorder = "ethi-gran-kawi-bugi-newa", .reorderLength = 24};
    mlocCollatorOptions far = {.strength = (mlocCollationStrength)6};
    mlocCollatorOptions hollow = {.reorder = nullptr, .reorderLength = 3};
    mlocDataSet none = {nullptr, 0};
    CHECK(mlocInitCollator(&c, &s_data, &en, &full) == mloc_errorCapacity &&
              mlocInitCollator(&c, &s_data, &en, &far) == mloc_errorInvalid &&
              mlocInitCollator(&c, &s_data, &en, &hollow) == mloc_errorInvalid &&
              mlocInitCollator(&c, &none, &en, nullptr) == mloc_errorNotFound &&
              mlocInitCollator(nullptr, &s_data, &en, nullptr) == mloc_errorInvalid &&
              mlocInitCollator(&c, nullptr, &en, nullptr) == mloc_errorInvalid &&
              mlocInitCollator(&c, &s_data, nullptr, nullptr) == mloc_errorInvalid,
          "a reordering too large, options out of range, data without the root, null pointers");
    int order = 7;
    mlocCollator made;
    CHECK(Make("en", nullptr, &made) &&
              mlocCompare(&before, "a", 1, "b", 1, &order) == mloc_errorInvalid &&
              mlocCompare(nullptr, "a", 1, "b", 1, &order) == mloc_errorInvalid &&
              mlocCompare(&made, nullptr, 1, "b", 1, &order) == mloc_errorInvalid &&
              mlocCompare(&made, "a", 1, nullptr, 1, &order) == mloc_errorInvalid &&
              mlocCompare(&made, "a", 1, "b", 1, nullptr) == mloc_errorInvalid && order == 7,
          "compare refuses a collator the library did not make and null pointers");
    size_t length = 9;
    uint64_t version = 9;
    uint8_t key[8];
    CHECK(mlocGetSortKey(&before, "a", 1, key, sizeof(key), &length) == mloc_errorInvalid &&
              mlocGetSortKey(&made, "a", 1, key, sizeof(key), nullptr) == mloc_errorInvalid &&
              mlocGetSortKey(&made, nullptr, 1, key, sizeof(key), &length) == mloc_errorInvalid &&
              mlocGetSortKey(&made, "a", 1, nullptr, 4, &length) == mloc_errorInvalid &&
              mlocGetSortKey(nullptr, "a", 1, key, sizeof(key), &length) == mloc_errorInvalid &&
              length == 9 && mlocGetCollationVersion(&before, &version) == mloc_errorInvalid &&
              mlocGetCollationVersion(&made, nullptr) == mloc_errorInvalid && version == 9,
          "keys and the version refuse a collator the library did not make and null pointers");
}

int main(void)
{
    FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (size == 0 || size == sizeof(s_file) ||
        mlocOpenData(&s_sources[1], s_file, size) != mloc_success)
    {
        fputs("test_collate: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    TestDefaults();
    TestKeys();
    TestOptions();
    TestTailorings();
    TestImportedRules();
    TestTailoringVersions();
    TestEveryType();
    TestSortKeys();
    TestIdenticalPrefix();
    TestUtf16();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
