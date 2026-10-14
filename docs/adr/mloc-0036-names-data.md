# mloc-0036. The names data: CLDR's display names by code, filed per key in a file of their own

Status: Accepted

## Context

The names part (L18 step 5) names languages, scripts, regions,
variants, keys and their values, and whole locales, as UTS #35 Part 2's
Locale Display Name Algorithm builds them. CLDR 49 holds about 240,000
such names in 434 locales, megabytes of text that a program naming
nothing should not ship. ICU4C resolves its names along the resource
chain, the short or variant form falling back to the name; ICU4X's
names data for all locales is 4.9 MB and leaves keys and values out.

## Decision

- **A file of its own,** `names.mloc`, as the currency names, unit
  names and spell-out rules are. Subdivisions, five megabytes used only
  by the `sd` and `rg` values, go to a second file later.
- **One table of codes** (`src/names_tables.h`), each a namespace letter
  and what it names in BCP 47's case: `l:en-GB`, `s:Latn`, `r:419`,
  `v:valencia`, `k:ca`, `t:ca-gregory`, the display patterns
  (`p:localePattern`), the code patterns (`c:region`), the measurement
  systems (`m:uksystem`) and the yes and no values (`b:yes`). Keys and
  values are by their BCP 47 ids, which a locale id holds, CLDR's LDML
  names mapped through `common/bcp47`'s aliases.
- **Eight fields** per code: the name, short, long, variant, menu, core
  (a language's menu core, a value's scope core), extension (a
  language's menu extension) and stand-alone (a script's). The three
  alts that are no field ("biot", "chagos", Finnish's "secondary") are
  codes of their own: `r:IO%chagos`.
- **Filed per key** where the library's lookup would give another
  value: the field along the chain, else the name along the chain. That
  lookup gives CLDR's resolution, whose alt fallback is the same.
- **A lean unit,** not the sparse shape: per entry a key (the code's
  index times 8 plus the field, u16) and a text offset (u16), so an
  entry costs 4 bytes besides its text, against 9 in the sparse shape
  (the file is 4.3 MB instead of 5.2). One unit per key for each of
  languages, scripts, regions, variants, keys with values, and the
  patterns, which keeps each unit's text far below 64 KB (the largest,
  Fulah in Adlam's languages, is 19 KB).

- **Capitalization data:** the compiled-in context transforms gain the
  six usages ICU's LocaleDisplayNames reads (languages, script,
  territory, variant, key, keyValue), and `names.mloc` carries the
  titlecase of each lowercase letter that begins a name, in the
  spell-out titlecase table's layout, so names are titlecased without
  the spell-out file. The generator refuses a name whose first letter
  ICU titlecases by its language's own rules (Dutch "ij", Lithuanian
  "i" with a dot, Armenian ech-yiwn); none does in CLDR 49.

## Proof

`test_names_data` checks the validators against units built by hand and
spoiled one field at a time, and names from the file in every
namespace. A check run outside the repository looked up, in each of
CLDR's 1,148 locale files, each of the 1,861 display name paths CLDR
has: the library's lookup gave CLDR's resolution (the generator's
`resolve`) in all 2,136,428 cases. The one difference is a code CLDR
writes only as an inheritance marker (Finnish's `fa` secondary), which
the generator leaves out because no locale names it.

## Consequences

The file is 4.3 MB, of which 3.5 MB is text that differs between
locales; keys and values take 1 MB of it, which ICU4X does not carry.
A name costs one binary search of the codes and, per key along the
chain, one of its unit's keys.
