# mloc-0048. Grammar: Hungarian by the Academy's rules and a small table

Status: Accepted

## Context

Hungarian has 17 nominal cases in CLDR (the nominative and 16 others),
three-way harmony, linking vowels, a lengthened final vowel, the v of
-val and -vá assimilated to the last consonant, and stems that change
before a linking vowel. Its suffixes do not fit the Turkic engine of
record mloc-0047 (archiphonemes on a stem that only softens): the
assimilation writes the stem's last consonant again, by rules on
digraphs, old family names' letters and double letters. No lexicon of
these stems is available under a licence the library can ship
(hunspell's hu_HU is GPL, LGPL or MPL 1.1; morphdb.hu and emMorph are
not MIT-compatible).

## Decision

- **Its own module** behind the inflector's reading of a text's end:
  `grammar_hu_sound.c` reads how a word sounds (harmony, the final
  vowel, or the final consonant in modern letters: Kossuth's th as t,
  Madách's ch as cs, Marx's x as ksz, a final y after a consonant but
  g, l, n, t as i; AkH 12, 86, 216), `grammar_hu.c` writes the suffixes
  in order and the joined word's changed end, `grammar_hu_words.c` holds
  the table.
- **Harmony** by the last vowel that is not i, í or é; one of these
  after a back vowel leaves the word back ("kávéban", "papírban"), two
  or more make it front ("analízisben"), as the oracle counts them; a
  compound's harmony is its last word's ("számítógépek").
- **Suffixes**: the cases as CLDR names them (essive as -ként, the
  productive essive-formal; the dative -nak), the plural, the
  possessives of one thing and of many (-m, -d, -(j)a, -nk, -tok,
  -(j)uk; -(j)aim ...), each after a linking vowel where it takes one:
  o, e, ö, or a and e after a lowering stem or a suffix ("házat",
  "házakat", "házamat"), u and ü in the possessives' plural. A final a
  or e (and a foreign word's o, ö) is lengthened before all but -ként
  (AkH 29, 216). The accusative's t joins j, l, ly, n, ny, r, s, sz, z,
  zs, after a long one only in a name ("Bonnt", "orrot"). The third
  person's j by the word's end, as the oracle counts it: after a vowel
  and two consonants, never after a sibilant, a palatal, j, h or v,
  after one consonant n, d, b, p (and m, t, k, l in one syllable).
- **The table** (220 words, written from the grammars and the rules and
  each checked against the oracle): lowering stems ("ház", "könyv",
  "ló"), stems that shorten, drop a vowel or take a v before a linking
  vowel ("víz": "vizet", "bokor": "bokrot", "ló": "lovat"), which also
  change in the superessive where it says ("bokron", "lovon"; but
  "vízen"), or keep the word with -ja in the third person ("útja"),
  back stems of front vowels ("híd": "hídon") and stems whose
  accusative takes a linking vowel ("méz": "mézet"). A common noun is
  looked up whole, then by its last word of three letters or more
  ("étterem", "könyvtár"); a name only whole ("Héraklész" is not
  "ész"); a number's word by its last word ("huszonhárom": "23-at").
  The productive -alom and -elem of three syllables or more drop their
  vowel ("nyugalmat"); ordinals in -dik and participles in -ott, -ött,
  -ult, -ült, -ölt lower ("harmadikat", "felnőttek").
- **Joining** (AkH 82, 92, 93, 160, 216, 217, 282, 288, 292): the v of
  -val and -vá written as the last consonant again, a digraph doubled
  in its first letter ("Kodállyal", "kulccsal"), an old or foreign
  spelling kept and the sound's modern letters after it ("Kossuthtal",
  "Madáchcsal", "Marxszal"); three same letters written two in a common
  noun ("tollal", "orra") and kept with a hyphen in a name ("Papp-pal",
  "Mann-nak"). A hyphen after digits, ordinals' period, capitals (read
  by their letters, or as a word the caller says, never lengthened:
  "MTA-n", "ENSZ-szel"), a period or a closing mark, the suffix as the
  number or letters are said ("5-tel", "3-at", "10-et", "4-gyel",
  "3.-at"); and after a hinted name whose written end is not said
  ("Rousseau-val"), a said consonant joining ("Balzackal") and a said
  final vowel lengthened ("Goethének").
- No genitive, copula or question suffix (refused).

## Proof

- `test_grammar_hu`: the sixteen cases of a back lowering stem, a front
  rounded stem that drops its vowel and a final a; harmony; the table's
  stems; compounds; names; assimilation as AkH 160 writes names; the
  possessives; digits, ordinals and abbreviations; hints; refusals.
- Oracle (run in the private research only): Wiktionary's Hungarian inflection
  tables as kaikki.org extracts them (read only, never shipped), 21,413
  nouns and 2,619 names. Nouns agree 98.6 to 99.9% in the cases, 88.3%
  in the accusative, 85 to 86% in the plural, 95 to 97% in the first
  and second persons' possessives and 88.8% in the third's; weighted to
  the 20,000 most frequent words of a subtitle frequency list, 99.4% in
  the dative, 90.6% in the accusative, 92.9% in the plural, 96.5% in
  the first person and 88.8% in the third. Names agree 98.4 to 99.6% in
  the cases, 95.9% in the accusative, 93.7% in the plural. The rest are
  stems no rule tells (vowel drop in "lepel", lowering in "vad"),
  loanwords said otherwise than written ("musicalek", "show-k"), and
  forms of inflected words the dictionary lists as headwords.
- apertium-hun is no oracle: its paradigms swap the dative and the
  accusative and give every name one paradigm.
- `fuzz_grammar` draws Hungarian in its seventh slot and eight of its
  cases in a bit no seed used before.

## Consequences

- A common noun's stem that the table lacks takes the rules' form; the
  table grows by the frequency of the words the oracle shows wrong.
- A loanword said otherwise than written ("design", "Disney") takes a
  hint, which also gives the hyphen of a silent end.
- An abbreviation read as a word ("NATO", "MÁV") takes the word reading.
- Native speakers review the table and the test before a release.
