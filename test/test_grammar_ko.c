// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Korean particles (maul-locale/grammar.h): every particle after a vowel,
// a consonant and ㄹ, numbers by their Sino-Korean reading, Latin capitals
// by their letters' names, Hanja by their readings, both forms where the
// end cannot be read, a gloss in parentheses, hints and the refusals.

#include "test_harness.h"

#include "maul-locale/grammar.h"

#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocInflector s_ko;
static char s_text[512];

static mlocInflection Particle(mlocKoreanParticle particle)
{
    mlocInflection r;
    memset(&r, 0, sizeof(r));
    r.particle = particle;
    return r;
}

static bool Gives(const char* text, const mlocInflection* request, const char* expected)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult r = mlocInflectText(&s_ko, text, strlen(text), request, &output);
    bool good = r == mloc_success && output.length == strlen(expected) &&
                memcmp(s_text, expected, output.length) == 0;
    if (!good)
    {
        printf("  [%s] -> %d [%.*s], not [%s]\n", text, (int)r, (int)output.length, s_text,
               expected);
    }
    return good;
}

// Whether a particle gives its forms after 사과 (a vowel), 책 (a
// consonant), 지하철 (ㄹ) and a Latin word (unread).
static bool Forms(mlocKoreanParticle particle, const char* afterVowel, const char* afterConsonant,
                  const char* afterRieul, const char* unread)
{
    mlocInflection r = Particle(particle);
    char vowel[64];
    char consonant[64];
    char rieul[64];
    char both[64];
    snprintf(vowel, sizeof(vowel), "사과%s", afterVowel);
    snprintf(consonant, sizeof(consonant), "책%s", afterConsonant);
    snprintf(rieul, sizeof(rieul), "지하철%s", afterRieul);
    snprintf(both, sizeof(both), "Apple%s", unread);
    return Gives("사과", &r, vowel) && Gives("책", &r, consonant) && Gives("지하철", &r, rieul) &&
           Gives("Apple", &r, both);
}

static void TestHangul(void)
{
    CHECK(Forms(mloc_particleTopic, "는", "은", "은", "은(는)") &&
              Forms(mloc_particleSubject, "가", "이", "이", "이(가)") &&
              Forms(mloc_particleObject, "를", "을", "을", "을(를)") &&
              Forms(mloc_particleComitative, "와", "과", "과", "과(와)") &&
              Forms(mloc_particleComitativeSpoken, "랑", "이랑", "이랑", "(이)랑") &&
              Forms(mloc_particleVocative, "야", "아", "아", "아(야)") &&
              Forms(mloc_particleCopulaPolite, "예요", "이에요", "이에요", "이에요(예요)") &&
              Forms(mloc_particleCopula, "다", "이다", "이다", "(이)다"),
          "the particles of two forms, ㄹ a consonant");
    CHECK(Forms(mloc_particleDirection, "로", "으로", "로", "(으)로") &&
              Forms(mloc_particleCapacity, "로서", "으로서", "로서", "(으)로서") &&
              Forms(mloc_particleMeans, "로써", "으로써", "로써", "(으)로써") &&
              Forms(mloc_particleSource, "로부터", "으로부터", "로부터", "(으)로부터"),
          "the (으)로 family: ㄹ as a vowel");
    CHECK(Forms(mloc_particleChoice, "나", "이나", "이나", "(이)나") &&
              Forms(mloc_particleAny, "든", "이든", "이든", "(이)든") &&
              Forms(mloc_particleQuotation, "라", "이라", "이라", "(이)라") &&
              Forms(mloc_particleDefinition, "란", "이란", "이란", "(이)란"),
          "the particles whose vowel form ends the consonant form");
    mlocInflection none = Particle(mloc_particleNone);
    mlocInflection topic = Particle(mloc_particleTopic);
    CHECK(Gives("사과", &none, "사과") && Gives("“책”", &topic, "“책”은") &&
              Gives("서울(Seoul)", &topic, "서울(Seoul)은") && Gives("ㅋ", &topic, "ㅋ은") &&
              Gives("ㅏ", &topic, "ㅏ는") && Gives("한국 사람", &topic, "한국 사람은"),
          "no particle; closing marks, a gloss, jamo, the last word");
    CHECK(Gives("가", &topic, "가는") && Gives("힣", &topic, "힣은") &&
              Gives("ㄱ", &topic, "ㄱ은") && Gives("ㆎ", &topic, "ㆎ는"),
          "the first and last syllables and jamo");
    mlocInflection subject = Particle(mloc_particleSubject);
    CHECK(Gives("나", &subject, "내가") && Gives("너", &subject, "네가") &&
              Gives("저", &subject, "제가") && Gives("누구", &subject, "누가") &&
              Gives("그리고 나", &subject, "그리고 내가") && Gives("나", &topic, "나는") &&
              Gives("저녁", &subject, "저녁이"),
          "the pronouns' stems before 가");
}

static void TestNumbers(void)
{
    mlocInflection topic = Particle(mloc_particleTopic);
    mlocInflection direction = Particle(mloc_particleDirection);
    CHECK(Gives("0", &topic, "0은") && Gives("1", &topic, "1은") && Gives("2", &topic, "2는") &&
              Gives("3", &topic, "3은") && Gives("4", &topic, "4는") && Gives("6", &topic, "6은") &&
              Gives("9", &topic, "9는") && Gives("10", &topic, "10은") &&
              Gives("100", &topic, "100은") && Gives("1000", &topic, "1000은") &&
              Gives("10000", &topic, "10000은") &&
              Gives("1000000000000", &topic, "1000000000000는") && Gives("1.5", &topic, "1.5는"),
          "numbers by their Sino-Korean reading's last syllable");
    CHECK(Gives("1", &direction, "1로") && Gives("7", &direction, "7로") &&
              Gives("8", &direction, "8로") && Gives("3", &direction, "3으로") &&
              Gives("50%", &direction, "50%로"),
          "일, 칠 and 팔 end in ㄹ; a percent sign reads 퍼센트");
}

static void TestLetters(void)
{
    mlocInflection topic = Particle(mloc_particleTopic);
    mlocInflection direction = Particle(mloc_particleDirection);
    CHECK(Gives("KBS", &topic, "KBS는") && Gives("UN", &topic, "UN은") &&
              Gives("IBM", &topic, "IBM은") && Gives("URL", &direction, "URL로") &&
              Gives("BBC", &topic, "BBC는") && Gives("PDF", &topic, "PDF는"),
          "capitals by their letters' Korean names");
    CHECK(Gives("A", &topic, "A는") && Gives("XYZ", &topic, "XYZ는"), "the first and last letters");
    mlocInflection hinted = Particle(mloc_particleTopic);
    hinted.hint = "애플";
    hinted.hintLength = strlen(hinted.hint);
    mlocInflection word = Particle(mloc_particleTopic);
    word.reading = mloc_readingWord;
    word.hint = "나토";
    word.hintLength = strlen(word.hint);
    CHECK(Gives("Apple", &hinted, "Apple은") && Gives("NATO", &word, "NATO는"),
          "a Latin word's reading by its hint");
}

static void TestOtherScripts(void)
{
    mlocInflection topic = Particle(mloc_particleTopic);
    mlocInflection direction = Particle(mloc_particleDirection);
    CHECK(Gives("學校", &topic, "學校는") && Gives("金", &topic, "金은") &&
              Gives("一", &direction, "一로") && Gives("東京", &topic, "東京은") &&
              Gives("樂", &topic, "樂은(는)"),
          "Hanja by their readings' finals; differing finals both forms");
    mlocInflection exact = Particle(mloc_particleTopic);
    exact.exact = true;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_ko, "Apple", 5, &exact, &output) == mloc_errorNotFound &&
              Gives("とうきょう", &topic, "とうきょう은(는)"),
          "an unread end: both forms, or refused when asked to be exact");
}

static void TestRefusals(void)
{
    mlocInflection r = Particle(mloc_particleTopic);
    r.grammaticalCase = mloc_caseDative;
    mlocInflection plural = Particle(mloc_particleTopic);
    plural.plural = true;
    mlocInflection unknown = Particle((mlocKoreanParticle)17);
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_ko, "책", 3, &r, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_ko, "책", 3, &plural, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_ko, "책", 3, &unknown, &output) == mloc_errorInvalid,
          "no case, plural or unknown particle");
}

int main(void)
{
    static mlocData sources[2];
    alignas(8) static uint8_t spellout[8 << 20];
    mlocOpenBuiltinData(&sources[0]);
    FILE* file = fopen(MLOC_SPELLOUT_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(spellout, 1, sizeof(spellout), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    if (size == 0 || mlocOpenData(&sources[1], spellout, size) != mloc_success)
    {
        fprintf(stderr, "test_grammar_ko: cannot open %s\n", MLOC_SPELLOUT_FILE);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    mlocLocale ko;
    if (mlocParseLocale("ko", 2, &ko).status != mloc_success ||
        mlocInitInflector(&s_ko, &s_data, &ko) != mloc_success)
    {
        fputs("test_grammar_ko: cannot make the inflector\n", stderr);
        return 2;
    }
    TestHangul();
    TestNumbers();
    TestLetters();
    TestOtherScripts();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
