# mloc-0049. Grammar: Finnish by Kotus's inflection types

Status: Accepted

## Context

Finnish has 14 nominal cases in both numbers, possessive suffixes and a
question clitic, two-way harmony with neutral i and e, and consonant
gradation: a stop that weakens before a closed syllable ("katu":
"kadun") or, in the types whose nominative ends in a consonant,
strengthens in the vowel stem ("rengas": "renkaan"). Which stems a word
takes is its inflection type, one of Kotus's 49 for nominals, and its
gradation, one of 13 letters; the ending tells both for most words, not
for all ("ovi": "oven", "lovi": "loven", but "kivi" "kiven" and "tuli"
"tulen" and "pari" "parin"). Kotus publishes its word list of modern
Finnish, with each word's type and gradation, under GNU LGPL, EUPL 1.1
and CC BY 3.0.

## Decision

- **Its own module**, `grammar_fi.c` (with `grammar_fi_word.c`: a
  word's letters, harmony and gradation), behind the inflector's
  reading of a text's end, and a table `src/generated/finnish_types.h` that
  `tools/gen_finnish_types.py` writes from Kotus's word list, version 1,
  taken under CC BY 3.0 at a pinned commit of its GitHub copy
  (`tools/kotus-release.txt`, the file's SHA-256 checked; CI fetches it
  and fails when the table is stale). The table keeps only what the
  ending does not tell: 792 endings of one to five letters with the
  type and gradation most words ending so have, where it differs from
  the ending one letter shorter's and three words or more end so; and
  3,954 words the endings, or a shorter word of the table ending them,
  give another kind, alone or as a compound's last word (58 KB in
  all).
- **A word's kind**: a common noun whole, then by its last word of
  three letters or more after two or more ("viikunanlehti" by
  "lehti"), then by its longest ending; a word no ending knows and not
  in -nen as a name. A name by its ending only (the table's words are
  common nouns): a final consonant takes i ("Bush": "Bushin"), a
  double stop or nk grades ("Pekka": "Pekan", "Helsinki":
  "Helsingissä").
- **The stems** by the type's row: the letters dropped, the vowel stem,
  the partitive's, the illative's and the plural's endings; the strong
  grade before an open syllable, the weak before a closed one, the
  strong everywhere in the vowel stem of types 32 to 49 but 40 (whose
  -uus grades as a vowel-final word: "kalleuden", "kalleuteen"); a
  type 18 long vowel shortened before the plural's i ("maissa"), a
  diphthong kept ("tauissa"). The grading consonant is the one before
  the last syllable, of three letters at most. A k gone leaves an
  apostrophe between two same vowels after a long vowel or a diphthong
  ("vaa'an", "liu'un"; but "koon") and writes a diphthong's i before a
  vowel as j ("ajan").
- **Harmony** by the last vowel that is not i or e; a compound's by its
  last word where the table finds it.
- **The endings** in order: the case, then the possessive (the
  nominative's and genitive's on the strong vowel stem, the illative
  without its n, -ksi as -kse, the third person as a long vowel and n
  or -nsA), then the question clitic -kO.
- **Digits, capitals and ordinals** (Kielitoimisto's guidance): a colon
  and the ending as the number, its ordinal or the letters' names are
  said ("5:een", "17:ää", "100:aa", "5:nnen", "EU:hun", "BBC:ssä");
  refused in the plural, with a possessor, in the instructive and the
  comitative. A hinted name whose written end is not said takes an
  apostrophe ("Bordeaux'ssa").
- No copula (refused).

## Proof

- `test_grammar_fi`: the 13 cases of a type 1 word with t:d and a type
  27 word, the types the oracle shows most, the plural stems, names,
  compounds' harmony, the k gone, possessives and the clitic, digits,
  ordinals, abbreviations, hints and refusals.
- Kotus: the endings predict the type and gradation of 91.4% of its
  28,338 nominals; the table gives every one its own.
- Oracle (run in the private research only): Wiktionary's Finnish
  declension tables as kaikki.org extracts them (read only, never
  shipped), 2.74 million forms of common nouns. They agree 96.6% in the
  genitive, 94.8% in the partitive, 93.5% in the inessive, 96.9% in the
  illative, 94.0% in the essive, 96.6% in the translative, 93.5% in the
  abessive; in the plural 96.0% in the genitive, 92.7% in the partitive
  and the inessive, 95.8% in the illative. A third of the rest is a
  compound whose last word has only i and e and the table lacks
  ("kuunsirppi": "kuunsirpissä"); the others are words Kotus lacks,
  plural nouns and loanwords said otherwise than written.
- `fuzz_grammar` draws Finnish in its eighth slot, its partitive,
  abessive and comitative in the places of cases it lacks.

## Consequences

- A compound whose last word has only i and e and is not in the table
  takes its first word's harmony; the table's words could carry their
  harmony as a later amendment, with a source for it.
- A loanword said otherwise than written takes a hint.
- Native speakers review the test before a release.
