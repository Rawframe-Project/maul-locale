# Locales

How locale identifiers are held, how a locale finds its data, and how matching, preferences and display names work. Sources: `src/locale_*.c`, `src/names*.c`.

## The locale value

`mlocLocale` is 128 bytes with readable fields: language (8), script (4), region (4), and the variants and extensions as canonical text (112), every field NUL-padded. Every byte is defined, so locales compare with `memcmp`, and resolving a locale for data reads fixed fields instead of splitting a string. A tag whose variants and extensions pass 112 bytes, or that has more than 64 subtags, is refused with `mloc_errorCapacity`, never cut.

Parsing (`locale_parse.c`) is strict and applies canonical syntax only: case, variant and attribute order, extensions by singleton with private use last, keys in order, a `u` value of `true` removed. Replacing aliases is canonicalization (`locale_canonical.c`, `locale_extensions.c`), a separate call that reports what it changed, because a program must know whether the tag it stored is the one it uses. Canonicalization follows UTS #35 Annex C, rules applied until none matches; the alias keys have a hash index in their table because a tag looks each of its fields up on every pass. Aliases that never stop replacing can only come from a crafted file and are refused with `mloc_errorFormat`.

Every other locale function expects a canonicalized locale and replaces no aliases itself. That keeps matching, fallback and formatting fast, and it is the one trap callers meet: a locale parsed but not canonicalized (`iw`, `sh`) finds the wrong data.

## Keys and chains

Data is filed under a locale's key ([glossary](glossary.md#locales)): the CLDR id with the likely script, dropped when it is the language's own. `zh-TW` is `zh_Hant_TW`, so traditional Chinese data is found whether the tag names the script or not; `sr-Cyrl-BA` is `sr_BA`. The generator files each CLDR locale under its key and checks that two files sharing a key hold no conflicting data.

From a key, the next one is the component's explicit parent when CLDR lists one (`en_AU` to `en_001`, `es_MX` to `es_419`), else the last variant dropped, else the region, else the script, where in the main component a script always goes to root: `zh_Hant` falls back to root, never to `zh`, so a lookup never meets a different script than the one asked for. The components (main, collation, plurals, segmentation, grammar) have parent lists of their own. Collation adds one rule: a locale with a collation file of its own that collation's parent list does not name takes its main parent (`nb` and `nn` reach `no`, whose rules they share), unless that parent is root, as ICU4C's data does.

`locale_fallback.c` walks a chain through every source of a data set; every part uses it. The generator checks the runtime chain against CLDR's own inheritance for every locale file of the release.

## Likely subtags and direction

Likely subtags (`locale_likely.c`) are one unit of no locale, searched on packed fields. Minimizing, parents and matching look the table up directly instead of maximizing whole locales. A locale's direction is its script's, from CLDR's script metadata compiled into `src/generated/script_directions.c`; the generator checks that this agrees with every CLDR locale's own character order.

## Matching

Matching (`locale_match.c`, `locale_distance.c`, `locale_lsr.c`) gives what ICU's `LocaleMatcher` gives for the same locales and options. The generator ports ICU's distance table builder (its `DistanceTable`, `PartitionInfo` and `TerritoryContainment`) and writes the pruned table; CI checks it against ICU's matcher test file. Supported locales are prepared once into small entries in the order they are tried (equivalents of the default first, then CLDR's paradigm locales, then the rest), so each match maximizes only the desired locales. Private use tags and the pseudo locales (`XA`, `XB`, `XC`) match only their own kind.

## Regional preferences

One region decides every preference of a locale (`locale_prefs.c`): `-u-rg`, else the region, else `-u-sd`'s region, else the likely region, else 001; a region without data takes 001's. Taking every field from one place means a locale never gets its hour cycle from one region and its first day from another. Hours are looked up for the language in the region first, since CLDR's time data lists a few pairs (`ca_ES`). Keys override field by field; `-u-ca-iso8601` also starts weeks on Monday with four days in the first week, as ISO 8601 does, unless `-u-fw` or `-u-rg` give the first day.

## Display names

Names are a file of their own (`names.mloc`, 4.3 MB), with subdivision names, used only for `sd` and `rg` values, in another. One table of codes names everything by BCP 47 ids with a namespace letter (`l:en-GB`, `s:Latn`, `r:419`, `t:ca-gregory`); CLDR's LDML key and type names are mapped through `common/bcp47`, so a code comes straight from a locale's subtags. Each code has eight fields (the name and CLDR's alternates). An entry costs 4 bytes besides its text, which keeps the file at 4.3 MB instead of 5.2.

Single-code names are looked up as ICU4C's `LocaleDisplayNames` does: the form along the chain, else the name along the chain; a language the chain does not name is looked up again under its CLDR alias, so a locale's own name for a deprecated code wins. Whole-locale names follow CLDR's `NameGetter`: the longest dialect name of the language with its script, region and variants, then the parts left out, then the extensions by their keys' patterns, joined by the locale's separator, brackets nested by root's replacement.

Capitalization follows CLDR's context transforms, which the compiled-in data holds per usage. ASCII and the Turkic dotted and dotless i are titlecased in code; every other letter by a titlecase table the generator writes into `names.mloc` from Maul Unicode, refusing text whose titlecasing would depend on a locale other than the Turkic ones.
