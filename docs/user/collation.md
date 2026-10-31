# Collation

Sorting text by its bytes puts "Zebra" before "apple" and "é" after "z". Collation sorts it as a language's readers expect: Swedish puts "ö" after "z", German phone books read "ö" as "oe", Spanish once sorted "ch" as one letter, and Chinese sorts by pinyin or by stroke. With `collation.mloc` in the set, `maul-locale/collate.h` compares and sorts text in any CLDR locale's order, as ICU4C's `Collator` does, byte for byte in its sort keys; `search.h` finds text, and `index.h` builds the "A, B, C" headings of a sorted list.

## Comparing

`mlocInitCollator` makes a locale's collator. The locale's `-u-co` key picks among its orders (`de-u-co-phonebk`, `zh-u-co-stroke`), and its other `-u-k` keys set the options, which the call's options override:

- **Strength:** what counts as different. Primary compares letters only ("a" equals "á" and "A"), secondary adds accents, tertiary case and variants (the default), quaternary the ignored punctuation, identical everything.
- **Alternate shifted** ignores spaces, punctuation and, by the max variable, symbols until the quaternary level: "e-mail" sorts with "email".
- **Case first** sorts upper or lower case first; a **case level** distinguishes case even at primary strength.
- **Numeric** compares runs of digits by value: "file2" before "file10".
- **Reorder** puts scripts and groups in another order: Greek before Latin, digits after letters.

```c
mlocCollator collator;
int order = 0;
if (mlocInitCollator(&set, &locale, nullptr, &collator) == mloc_success &&
    mlocCollator_Compare(&collator, "öl", 3, "zebra", 5, &order) == mloc_success)
{
    // de: order is -1, "öl" first; sv: 1, "ö" sorts after "z"
}
```

`mlocCollator_CompareUtf16` compares UTF-16 text with the same result, reading a lone surrogate as U+FFFD. A collator is a value that may be shared by every thread. Text is compared as Unicode normalizes it, whatever its form.

## Sort keys

To sort many strings, or to store an order in a database, `mlocCollator_GetSortKey` writes a key per string: bytes whose `memcmp` order is the collator's order. The keys are ICU4C's bytes for the same locale and options, except for Han ideographs, which keep CLDR's root order by code point where ICU4C's default data orders them by radical and stroke. Store `mlocCollator_GetVersion` and the options beside the keys: the version changes whenever the key of some text could change (another collation data or key format) and tells when stored keys must be rebuilt; the options are not part of it.

## Searching

`mlocInitSearcher` makes a searcher for a pattern under a collator, and `mlocSearcher_FindNext` and `mlocSearcher_FindPrevious` find it in a text, as ICU4C's string search does: at primary strength "resume" finds "Résumé", and matches never split a grapheme cluster or a contraction. The comparison can treat an unaccented letter in the pattern, or any base letter, as a wildcard for its accented forms.

```c
mlocCollatorOptions loose = {.strength = mloc_strengthPrimary};
mlocCollator collator;
mlocSearcher searcher;
size_t start = 0;
size_t end = 0;
if (mlocInitCollator(&set, &locale, &loose, &collator) == mloc_success &&
    mlocInitSearcher(&collator, "resume", 6, nullptr, &searcher) == mloc_success &&
    mlocSearcher_FindNext(&searcher, text, length, 0, &start, &end) == mloc_success)
{
    // text "My Résumé.pdf": start 3, end 11
}
```

## Alphabetic indexes

`mlocBuildIndex` builds the buckets of a contacts list from a locale's index labels, as ICU4C's `AlphabeticIndex` does: "A" to "Z" in English, "A" to "Z" with "Å", "Ä", "Ö" in Swedish, kana rows in Japanese, and an underflow and overflow bucket for names of other scripts. `mlocFindIndexBucket` gives the bucket a name falls into. The index is built into caller memory once per locale and read from any thread; its bytes may be copied or moved but not otherwise changed.
