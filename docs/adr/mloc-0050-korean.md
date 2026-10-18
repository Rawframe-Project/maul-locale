# mloc-0050. Grammar: Korean particles by the final of the spoken end

Status: Accepted

## Context

A Korean particle takes one form after a final consonant (batchim) and
another after a vowel: 은/는, 이/가, 을/를. The (으)로 family also treats
ㄹ as a vowel ("지하철로"). The final of a Hangul syllable is arithmetic
(Unicode 3.12). A text's end is not always Hangul, though: a number is
said in Sino-Korean ("1" is 일 and ends in ㄹ, "2" is 이), a capital by
its Korean name, a Hanja by its reading. A Latin word, kana or a
symbol has no reading the text can give. Korean writing has a notation
for an end whose reading is unknown: both forms, the consonant form's
own syllables in parentheses ("은(는)", "(으)로").

## Decision

- **A request names one particle** (`mlocKoreanParticle`, 0 for none):
  topic 은/는, subject 이/가, object 을/를, comitative 과/와 and the spoken
  이랑/랑, vocative 아/야, the copula 이에요/예요 and 이다/다, the (으)로
  family (direction, capacity 으로서, means 으로써, source 으로부터),
  choice 이나/나, any 이든/든, quotation 이라/라, definition 이란/란.
  - Korean refuses a case, the plural, a possessor, the Turkic copula
    and the question.
  - Every other language refuses a particle.
- **The final** comes from the spoken end's last code point:
  - A Hangul syllable's `(cp - 0xAC00) % 28`: 0 is a vowel, 8 is ㄹ.
  - A compatibility jamo is its own letter.
  - A Hanja reads by a table generated from Unihan's kHangul field
    (`tools/gen_hanja_finals.py`). The version is the Unicode version
    maul-unicode follows, pinned in `tools/unihan-release.txt` and
    checked in CI. The table holds two bits per code point over the
    three CJK blocks: no reading or readings whose finals differ, a
    vowel, ㄹ, another final. That is 7 KB for 8,525 readings, of which
    49 differ.
- **What is spoken:**
  - A number by spell-out's ko cardinal rules, read to its last
    syllable ("10은", "1000000000000는", "1.5는"); Korean needs no
    ordinal rules.
  - A percent sign as 퍼센트.
  - Capitals by the loanword rules' letter names: 에이, 비, 씨 … 엘, 엠,
    엔 … 제트.
  - A hint (in Hangul) instead of the text.
  - A parenthesized gloss after a word leaves the word before it to
    read ("서울(Seoul)은"), as the orthography writes it.
  - Closing marks are skipped.
- **Both forms where the end cannot be read**: "은(는)", "이(가)",
  "(으)로", "(이)랑". With the request's `exact`, the inflector refuses
  with `mloc_errorNotFound` instead, for a caller who would rather give
  a hint.
- **The pronouns** 나, 너, 저 and 누구 take their own stems before 가
  (내가, 네가, 제가, 누가), as the dictionary gives them. Only the word
  alone or after a space counts.
- The text is kept whole except for those pronouns' stems; the particle
  is appended with no mark or space.

## Proof

- `test_grammar_ko`: each particle after a vowel, a consonant, ㄹ and an
  unread end; numbers; capitals; Hanja; the gloss; the pronouns;
  hints; `exact`; the refusals.
- Oracles (run in the private research only, never shipped):
  - es-hangul 2.4.0 (Toss, MIT): its 14 particles on all 11,172 Hangul
    syllables, and on the numbers 0 to 10,000 through its
    `numberToHangul` reading. All 296,422 cases agree but the three
    pronouns, whose contraction es-hangul does not make.
  - tossicat 0.8.0 (MIT): all 16 particles on the same syllables and
    numbers. All 317,595 cases it answers agree.
- `fuzz_grammar` draws Korean in a ninth slot. The slot is the reading's
  value 3, which no language has, so seeds that held it only tested its
  refusal.

## Consequences

- A Latin word is read only through a hint; without one it takes both
  forms, which is correct Korean, if less polished.
- A Hanja whose readings' finals differ (樂: 락, 악, 요) takes both forms.
- Native readers review the test before a release.
