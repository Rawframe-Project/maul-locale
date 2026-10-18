# mloc-0047. Grammar: the other Turkic languages by tables of one engine

Status: Accepted

## Context

Record mloc-0046 inflects Turkish. Kazakh, Kyrgyz, Azerbaijani, Uzbek
and Turkmen share its mechanism (suffixes chosen by the spoken end) but
differ in four ways: the boundary mark (Turkish's apostrophe; a hyphen
after an abbreviation and after digits in Kazakh and Azerbaijani, as
Kazakh's abbreviation guidance and Azerbaijani's orthography norms
write: "АҚШ-тың", "BMT-yə", "2-yə"; Uzbek joins its abbreviations,
"ToshDUning"), harmony (two or four ways, none in Uzbek), the first
consonant of a suffix (Kazakh and Kyrgyz choose it by the class of the
stem's last sound: "-лар", "-дар", "-тар") and stem changes. Copies of
the engine per language would drift apart.

## Decision

- **One engine** (`grammar_turkic.c`) and **a table per language**
  (`grammar_tr.c`, `grammar_kk.c`, `grammar_ky.c`): the language's letters with their
  class (vowel with its backness and rounding, glide, liquid, nasal,
  other voiced, voiceless), its A and I vowels by harmony, its onsets
  (an archiphoneme's consonant after each class of sound, back and
  front), its suffix templates (plural, possessives, each case after a
  stem, after a third person's possessive and after a first or second
  person's, copula, question particle), its letter names, its boundary
  mark and where it goes, and its stem rule. A letter a table does not
  list is read by a shared Latin table, so a Latin word in a Cyrillic
  text has a sound.
- **Kazakh** (Cyrillic; another script is refused): two-way A (а, е)
  and I (ы, і); the onsets of apertium-kaz's model (l, d, t for the
  plural; n after a vowel or nasal in the genitive; ғ, қ and г, к in
  the dative, none after a first or second person's possessive:
  "кітабыма"; n before the ablative after a nasal; м, б, п in the
  instrumental and the question particle); ж ends a word as voiceless
  ("тәжтің"), a final к or г makes the suffixes front ("паркке"), и, у
  and ю give the harmony only to a word without another vowel, a final
  ь makes the suffixes front and is read as a vowel after a voiceless
  consonant and as l after another ("повестьнің", "князьдің") and is
  dropped before a vowel ("автомобилі"); a final п, к, қ after a vowel
  is voiced before a vowel in any word, names too ("кітабы",
  "Айтбегі"); no copula suffix (refused). Letters are named as the
  abbreviation guidance reads them, a consonant with ы and к, г with і
  ("ТМД-ның", "ҚР-ға"). A hyphen after an abbreviation read by its
  letters, after digits and after a word in another script
  ("Google-ге"); a name joins.
- The part 0x61 keeps its name (`mloc_inflectionPartApostrophe`) and is
  documented as the boundary mark, whichever character it is.
- **Kyrgyz** (Cyrillic, amended 2026-10-18): four-way A (а, о, е, ө)
  and I (ы, у, и, ү), A rounded after о and ө but not after у
  ("кулдар", "кулдун"; the letter flag `turkicFlat`); onsets by four
  classes (a vowel, a glide, another voiced consonant, a voiceless one:
  "кардын", "тоолор"), the dative's onset dropped after a first or
  second person's possessive ("китебиме"); no instrumental case (CLDR)
  and no copula suffix; the question particle joined ("китеппи"); no
  mark at all, abbreviations and digits joined as the Kyrgyz press
  writes them ("АКШнын", "УКМКга", "350дөн"); a final ь dropped before
  any suffix ("княздын", "лагерге"), a final й and a suffix's у written
  ю ("тоюм"), a final п or к voiced before a vowel, names too
  ("китеби", "Айбеги"). Letters are named the Russian way (ка, эл, эр,
  ша). A stem rule now sees the suffixes written for it, to change their
  first letter where the two fuse.
- The order of the rest: Azerbaijani, Uzbek, Turkmen, each a table and
  an amendment here.

## Proof

- `test_grammar_turkic`: Kazakh names of each last sound in the six
  cases, common nouns after a voiceless consonant and ж, the plural's
  three onsets, the possessives and the cases after them, loans (к, ь),
  a name's voiced stop, the question particle, abbreviations, digits, a
  Latin word, a formatted number, the refusals; Kyrgyz the same in five
  cases, the fused ю, the dropped ь, the joined marks and particle.
- apertium-kaz's generator (GPL, run in the private research only) over
  its lexicon's 6,664 names and 13,565 nouns, nine requests each: 97 to
  99.8% agree; the rest are loans its lexicon marks one by one
  (harmony after a middle и, a double consonant written once, a final
  consonant cluster read front) and its own disagreements between
  cases of one word ("каминге" but "каминнан").
- apertium-kir over 3,916 names and 4,353 nouns: 99.5 to 99.9% agree;
  the rest are its words with a lexical stem ("мурдуна"), lowercased
  forms and abbreviations its lexicon gives no form.
- `fuzz_grammar` draws Turkish, Kazakh or Kyrgyz; the Turkish tests and the
  Turkish oracle give the same forms as before the engine was shared.

## Consequences

- A Kazakh loan whose harmony its letters do not show takes a hint.
- An abbreviation said as a word ("АҚШ" as [ақш]) takes the word
  reading or a hint, as in Turkish.
- Native speakers review each table before a release.
