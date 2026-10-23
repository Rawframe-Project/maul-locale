# Grammar

Suffixes and particles on runtime text and numbers, read from how the text's end is spoken. Sources: `src/grammar*.c`, `src/message_grammar.c`, and the lexicons in `src/generated/turkish_marks.h`, `finnish_types.h` and `hanja_finals.h`.

## Why

CLDR lists each language's cases (`grammaticalFeatures.xml`) but no rule for attaching a suffix to a word known only at run time, and ICU has no such API. The suffix depends on the sound of the word's end (vowel harmony, consonant assimilation, buffer consonants), so a number needs its spoken form and an abbreviation its letters' names. Turkic languages, Hungarian and Finnish share the mechanism; Korean particles depend on whether the last syllable has a final consonant. The rules are code, as the conventions ask for algorithms over a closed set; only the word lists they need are data.

## The inflector

An inflector (`grammar.c`) holds a locale's rules, its number formatter and, when the set has `spellout.mloc`, its `%spellout-numbering` and `%spellout-ordinal` formatters. Without them a number is refused, never guessed; Uzbek and Turkmen have no spell-out rules in CLDR, so their number words are in code.

**Reading the end** is shared: after any closing quotation marks or brackets, digits (with separators between them) are read by the last word of their spoken form, a fraction by its digits' number, digits and a period as an ordinal in the languages that write ordinals so; a word in capitals by its last letter's name ("TDK" ends in "ke"); a word as written, with a final y after a consonant said as i ("Harry'ye") and a word without a vowel spelled out ("PwC'ye"). A hint replaces the text for reading, and a short table of loanwords that break harmony ("saate", "rolü") follows the spelling guides.

## Languages

- **Turkic** (`grammar_turkic.c` and a table per language, `grammar_tr.c`, `grammar_az.c`, `grammar_kk.c`, `grammar_ky.c`, `grammar_tk.c`, `grammar_uz.c`): one engine over each language's letters and their classes, its harmony vowels, the consonant each archiphoneme takes after each class of sound, its suffix templates (plural, possessives, each case after a stem and after a possessive, copula, question particle), its letter names and its boundary mark. A letter a table does not list is read by a shared Latin table, so a Latin word in a Cyrillic text still has a sound. Turkish puts an apostrophe before the suffixes of a name, number or abbreviation, as the Turkish Language Association's spelling guide says; a common noun takes none and its stem changes by the lexicon below.
- **Hungarian** (`grammar_hu*.c`): harmony by the last vowel that is not i, í or é, with the Academy's rules for neutral vowels and compounds; the cases, plural and possessives with their linking vowels; a final a or e lengthened; old spellings of names read in modern letters (Kossuth's th as t).
- **Finnish** (`grammar_fi*.c`): a word's inflection type and consonant gradation decide its endings. The ending usually tells them; `finnish_types.h` holds the 792 endings and 3,954 words for which it does not, from Kotus's word list. A compound declines by its last word; place names ending in a common word decline as that word.
- **Korean** (`grammar_ko.c`): a particle's form after a final consonant or after a vowel (the (으)로 family also after ㄹ). The final comes from the Hangul syllable's arithmetic, or for Hanja from `hanja_finals.h`, two bits per code point from Unihan's kHangul readings. A text whose end cannot be read writes both forms ("은(는)") unless the request asks to be exact.

## Lexicons

The external word lists are generated into C tables by scripts with a `--check` mode that CI runs against the pinned sources ([updating data](../operations/updating-data.md#grammar-lexicons)):

| Table | Source | What it holds |
|---|---|---|
| `turkish_marks.h` | Zemberek-NLP's dictionaries (Apache 2.0) | Which common nouns keep or soften their final stop, take inverse harmony, double a consonant, drop a vowel, or end in a compound's possessive: 2,615 words, 20 KB, front-coded. |
| `finnish_types.h` | Kotus's word list of modern Finnish, version 1 (CC BY 3.0) | The inflection type and gradation where the ending does not tell them. |
| `hanja_finals.h` | Unihan's kHangul field (Unicode License) | Whether a Hanja's reading ends in a vowel, ㄹ or another final. |

The Turkish marks are written against Zemberek's own softening rule, which the library follows: the lexicon lists the exceptions, not every word.

## Proof and its limits

The only generators to compare with are Apertium's morphological transducers, which also err (apertium-tur writes "roli" where the spelling guide has "rolü"). The oracle scripts in `tools/oracle/grammar/` compare the library with Apertium and with Wiktionary's inflection tables; the tests pin the spelling guides' own examples. A difference from Apertium is a reason to read the guide, not a bug by itself.

## In messages

`mloc:case`, `mloc:particle` and `mloc:gender` (`message_grammar.c`) make values of their own over a text or a number value. A number is inflected as its own function writes it, so grouping and digits are the ones the reader sees. Binding prepares the inflector once per expression. An option value the function does not know is ignored, as MF2 ignores a bad option ("Rodrigo" for `case=dativ`). A request the language refuses (a partitive in Turkish, a particle outside Korean) is a bad option at formatting, and the placeholder's fallback stands. Korean writes both forms instead of failing, since in a message "Apple이(가)" reads better than a fallback.
