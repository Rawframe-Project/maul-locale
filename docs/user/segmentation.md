# Segmentation

Maul Unicode finds the words, sentences and line break opportunities of any text by Unicode's default rules (UAX #29 and UAX #14). Some languages tailor them: Greek ends a sentence at a semicolon, Japanese keeps a run of Hiragana one word, and English keeps "Mr. Smith" one sentence when asked. With `segments.mloc` in the set, `maul-locale/segment.h` applies the tailorings CLDR's segments files give, on Maul Unicode's iterators.

## Contents

- [Words, sentences and lines](#words-sentences-and-lines)
- [The keys](#the-keys)
- [Text in pieces](#text-in-pieces)
- [Differences from ICU4C](#differences-from-icu4c)

## Words, sentences and lines

`mlocInitSegmenter` starts a segmenter of one kind over a text, and `mlocSegmenter_NextBreak` reports each boundary after the start of the text, the last one being its end, as a byte offset; it returns `mloc_done` after the last, with the offset then 0, as it is after `mloc_needMoreText` or a failure, so keep the last boundary yourself. A line segmenter also reports whether the line must end there (after a line feed or another mandatory break, and at the end of the text). Grapheme clusters have no tailoring: take them from Maul Unicode.

```c
const char* text = "Mr. Smith left. He came.";
mlocSegmenter segmenter;
size_t end = 0;
size_t last = 0;
if (mlocInitSegmenter(&set, &locale, mloc_segmentSentence, text, strlen(text), false,
                      &segmenter) == mloc_success)
{
    while (mlocSegmenter_NextBreak(&segmenter, &end, nullptr) == mloc_success)
    {
        last = end; // en-u-ss-standard: 16, then 24; en: 4 (after "Mr. ") first
    }
}
```

A locale takes the tailoring of the first locale along its segmentation chain that has one, as other parts take their data: a variant, then the region, then the script are dropped, and `zh-Hant` goes to the root, not to `zh`. These are CLDR 49's:

| Locale | Tailoring |
|---|---|
| `el` | A sentence ends at U+003B and U+037E, the Greek question mark. |
| `en-US-u-va-posix` | U+002E is MidNum in words: "a.b" is three words, "3.14" one. `mlocCanonicalizeLocale` gives this locale for `en-US-posix` and `en_US_POSIX`; a locale parsed with the variant `posix` takes the same tailoring. |
| `ja` | Rule 13.3 keeps a run of Hiragana one word, and 13.4 a run of ideographs (with 々, 〇 and 〻). Lines break before small kana and the prolonged sound mark (CJ as ID). |
| `zh`, `zh-Hant` | Lines break before small kana and the prolonged sound mark. |
| `de`, `en`, `es`, `fr`, `it`, `pt`, `ru` | The abbreviations of the suppressions below. |

A segmenter keeps no pointer into its text once it has asked for the next piece, none into the data it was made from but its suppressions, so the data set must outlive it, and none into itself. It is a value of 512 bytes that needs no cleanup and may be copied, the copy going on where the segmenter was. A Thai, Lao, Khmer or Burmese word segmenter can be set with `mlocSegmenter_SetComplexBreaker`, as Maul Unicode's `muniSetComplexBreaker`. The breaker must not make that segmenter again; any other call on it from inside the breaker is refused with `mloc_errorInvalid` and changes nothing.

## The keys

- **`-u-ss-standard`** keeps a sentence going after the abbreviations CLDR lists for the language ("Mr.", "z.B.", "etc."), as ICU4C's `FilteredBreakIterator` does; without it, or with `-u-ss-none`, every sentence break stays. The abbreviations are matched as ICU4C matches them: a break after an abbreviation and one space stays only when the text before it ends with no listed abbreviation; one with a full stop inside ("z.B.", "Ph.D.") must be read whole forward from its start.
- **`-u-lb`** picks the line rules. `strict` breaks before small kana and the prolonged sound mark as UAX #14 does, keeping them with the character before; `normal` and `loose` may break before them, as CSS's normal line breaking does for Chinese and Japanese. Without the key a locale takes its language's: normal for Chinese and Japanese, strict for the others. Maul Unicode has no rules of its own for `loose`, so `loose` breaks as `normal`, which allows fewer breaks than ICU4C's loose, never more.

## Text in pieces

Text read from a file or a network may arrive in pieces. Start the segmenter with the first piece and `moreFollows` true; when `mlocSegmenter_NextBreak` returns `mloc_needMoreText`, hand it the next piece with `mlocSegmenter_Feed`. A piece may begin in the middle of a UTF-8 sequence, and the breaks are the same as for the text whole. The segmenter asks for a piece only after it is done with the last one, so a program may reuse the last piece's buffer for the next.

```c
mlocSegmenter segmenter;
size_t end = 0;
size_t last = 0;
mlocResult r =
    mlocInitSegmenter(&set, &locale, mloc_segmentLine, first, strlen(first), true, &segmenter);
while (r == mloc_success)
{
    r = mlocSegmenter_NextBreak(&segmenter, &end, nullptr);
    if (r == mloc_success)
    {
        last = end;
    }
    else if (r == mloc_needMoreText)
    {
        r = mlocSegmenter_Feed(&segmenter, second, strlen(second), false);
    }
}
```

## Differences from ICU4C

The library follows CLDR's segments files. ICU4C 79.1 agrees with it on every suppression of every locale, with `-u-ss-standard` and without, on the class overrides, and on plain text of every kind in each locale CLDR tailors, except where ICU4C uses data or rules of its own, which CLDR does not have. `test/test_segment.c` compares 2033 cases with ICU4C's answers (`tools/oracle/segment/`) and lists each one that differs in `tools/segment-exceptions.txt`:

| Case | ICU4C 79.1 | The library |
|---|---|---|
| `zh` and `zh-Hant` lines without `-u-lb` | Its strict `line_cj` rules: "あぁあーい" is `[あぁ][あー][い]`. | CLDR's normal: `[あ][ぁ][あ][ー][い]`. A program that wants ICU4C's (and Chromium's) Chinese lines asks for `zh-u-lb-strict`, which gives ICU4C's breaks apart from the quotation marks below. |
| `ko` lines | Its own `ko.txt`: normal without `-u-lb`, its `_cj` rules with it. | CLDR has no Korean file: the root's strict lines. `ko-u-lb-normal` gives ICU4C's Korean default. |
| `ja`, `zh`, `zh-Hant` and `ko` lines with `-u-lb`, and `zh`'s and `zh-Hant`'s default | Its `_cj` rules make U+201C an opening and U+201D a closing punctuation mark: "say “abc”def" may break after ”. | Both are quotation marks, as in UAX #14: no break after ” before a letter. |
| `ja`, `zh`, `zh-Hant` and `ko` lines with `-u-lb-normal` or `loose` | `line_normal_cj` and `line_loose_cj` let a line break before 〜 (U+301C) and ゠ (U+30A0). | They stay nonstarters. |
| `ja` words | Its own dictionary: "ひらがなとかんじ" is `[ひ][ら][が][な][と][かん][じ]`. | CLDR's rules 13.3 and 13.4: `[ひらがなとかんじ]`. |

Words in Chinese, Japanese, Korean, Thai, Lao, Khmer and Burmese are another matter: ICU4C finds them with dictionaries for every locale, which neither CLDR nor the library has; the library gives Maul Unicode's default word breaks there, or those of a complex breaker the program sets.
