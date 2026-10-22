# mloc-0016. Locale keys and fallback per component

Status: Accepted

## Context

A part looks up its data for a locale the program gives, which is often
not one CLDR has a file for: `zh-TW`, `pt-BR`, `de-CH-1996`, or a
locale whose data a build left out. Walking the tag as written fails
in known ways: `zh-TW` truncates to `zh`, which is Simplified Chinese,
and `pt-Latn-BR` meets no file named so. UTS #35 gives each component
its own parent locales, which stand alone (they are not merged with
the main ones), and makes root the main parent of a locale whose
script is not its language's likely one. Of CLDR 49's 1,148 locale
files, 29 spell out their language's likely script (`sr_Cyrl`,
`zh_Hans`); each holds only its identity.

## Decision

- **Keys.** Data is filed under a locale's key: its CLDR id with the
  script it would get from likely subtags, left out when that is the
  language's own likely script, and the region as given. `zh-TW` has
  the key `zh_Hant_TW`, `pt-BR` has `pt_BR`, `sr-Cyrl-BA` has `sr_BA`,
  `und` has `root`. The generator files each CLDR locale under its key;
  where two files share one, the one whose id is the key is kept, and
  any other must hold no data of its own.
- **Fallback.** From a key, the next one is the component's explicit
  parent when CLDR lists one, else: the last variant dropped, else the
  region, else for the main component a script (always a non-likely
  one, by the key's form) gives root and for the others it is dropped,
  else root. Extensions are kept through the chain.
- **Components:** main, collation, plurals, segmentation and
  grammatical features, each with its own parent list from
  `supplementalData.xml`.
- `mlocGetParentLocale` gives the next locale of the chain; the parts
  walk the same chain through every source of a data set for the first
  that has their data, and report `mloc_fellBack` when it is not the
  locale's own key.

## Amendment (a), 2026-10-22: the main parents in the collation chain

- **A locale with a collation file of its own that collation's parent
  list does not name takes its main parent as its collation parent,**
  unless that parent is root. ICU4C's collation data follows the main
  parents this way: nb's and nn's files hold no rules, and both reach
  no's. Root, the main parent the non-likely-script rule gives
  (`bs_Cyrl`, `sr_Latn`, `zh_Hant`), is not taken: those locales keep
  their language's collation, as ICU4C's `inheritLanguageSubtag` does.
- The generator writes these parents after the listed ones, which win
  where both name a locale; the runtime chain and the generator's
  check read the one table.

## Consequences

A lookup never meets a different script than the one asked for in the
main component, and an authored `pt-BR` finds CLDR's `pt` data. The
generator checks the runtime chain against CLDR's own inheritance for
every locale file of the release.
