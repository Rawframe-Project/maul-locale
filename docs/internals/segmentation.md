# Segmentation

How `segment.h` turns CLDR's segments files into breaks on Maul Unicode's iterators. The [user page](../user/segmentation.md) says what a caller sees; this page says how the parts fit.

## Contents

- [The data](#the-data)
- [The segmenter](#the-segmenter)
- [The filters](#the-filters)
- [Text in pieces](#text-in-pieces)

## The data

CLDR writes a tailoring as ICU rule syntax: variables and rules over Unicode properties. The library has no rule engine; Maul Unicode's iterators implement UAX #29 and UAX #14 directly, with two hooks for what CLDR's files change: class overrides for words and sentences, and the CJ-as-ideograph line option. What no hook covers, the Japanese word rules and the suppressions, the library filters itself. So `tools/mlocgen/segment_gen.c` recognizes the forms CLDR 49 writes and turns each into a hook's setting or a filter's data, refusing any other form with its file and reason, so a new CLDR release shows what to add:

- a class variable with characters added (`[[$STerm] [; ;]]`) becomes overrides, in the library's own class numbers, mapped to Maul Unicode's when a segmenter is made, so a renumbering there cannot change a file;
- the `$ID` and `$NS` line variables become the CJ flag;
- Japanese's word variables and rules 13.3 and 13.4 become the Japanese flag;
- the standard suppressions become ICU4C's two tables, built as its `SimpleFilteredBreakIteratorBuilder` builds them.

`src/segment_tables.h` lays the unit out and says how a segmenter reads the tables; `segments.mloc` holds a unit per locale whose file tailors something.

## The segmenter

`mlocInitSegmenter` finds, for its kind, the first unit along the locale's segmentation chain that gives that kind, copies its overrides into the segmenter, keeps a pointer to its suppressions, and starts Maul Unicode's iterator with them. A line segmenter reads `-u-lb` before the unit's flag; a sentence segmenter turns its suppressions on only with `-u-ss-standard`. Without a filter a segmenter is the iterator: each call passes its break through.

## The filters

Two filters take breaks away; neither adds one.

- **Suppressions** (`src/segment_filter.c`) follow ICU4C's `breakExceptionAt`: step back over one space, find the longest backward entry the text before ends with, suppress at a match, and walk the forward entries from a partial's start. The forward table's sorted entries stand for ICU4C's trie: the entries that begin with the bytes read are a range, and the walk's results (no match, a final value, an intermediate value at the end of the text) are read off the range at each code point's end.
- **Japanese words** keep two Hiragana, or two ideographs (Ideographic, with U+3005, U+3007 and U+303B), with the Extend, Format and ZWJ characters after the first, in one word. A cursor walks forward from break to break, keeping the class of the last character that is not Extend, Format or ZWJ.

## Text in pieces

A filter reads text around a break, and Maul Unicode may report a break long after the text before it has gone: a sentence after "Mr. (1, 2, 3" waits for the next letter. The segmenter keeps the last 72 bytes of the earlier pieces, enough for every break the iterator can still report near the end of the text it has. The one sentence break it can hold back further is the end of the last run of a terminator, closing punctuation and spaces, since nothing between that run and the next letter can be a break; so when a piece is let go, the segmenter scans the runs and decides that position's suppression while its text is at hand. A word break held back further is always before a mid-word character, never between two Hiragana or two ideographs.

A break whose filter needs text not yet fed waits in a queue while the iterator goes on to the end of its piece. Only when the iterator asks for more text does the segmenter ask the caller, so a caller never feeds a piece before the last one is done with. `test_segment` checks that its own cases give the same breaks whole and in pieces of one to eight bytes, and every oracle case in pieces of one and three.
