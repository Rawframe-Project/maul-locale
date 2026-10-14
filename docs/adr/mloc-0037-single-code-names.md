# mloc-0037. Single-code display names: ICU4C's lookups, BCP 47's codes

Status: Accepted

## Context

Record mloc-0036 laid out CLDR's display names by code. Programs name a
language, script, region, variant, key, key's value or measurement
system alone (a language menu, a calendar picker) before they name a
whole locale. ICU4C's LocaleDisplayNames does that with a length, a
substitution flag and five capitalization contexts; ECMA-402's
Intl.DisplayNames takes BCP 47 codes with a style and a fallback; ICU4X's
experimental names have narrow, short, long and menu styles.

## Decision

- **API** (`maul-locale/names.h`): `mlocGetName` writes a locale's name
  for a code of a type into an `mlocOutput`, with options for the form
  (CLDR's name or any of the seven alternates record mloc-0036 keeps),
  the fallback (the code, or nothing) and the capitalization.
- **Codes are BCP 47's,** in any case: "ca-gregory", not ICU's
  "calendar" and "gregorian"; variants in any case, where ICU takes
  only uppercase. A language id may use '_' for '-'. A code not of its
  type's syntax is refused (`mloc_errorSyntax`).
- **Lookup as ICU4C's:** the form along the locale's chain, else the
  name along the chain; a language id the chain does not name is looked
  up again under its CLDR alias ("tl" as "fil", "tw" as "ak"), so a
  locale's own name for a deprecated code wins, as ICU's data gives it.
- **Fallback:** the code in its canonical case (a key and value's
  value alone), with `mloc_fellBack`; or `mloc_errorNotFound`.
- **Capitalization as ICU's:** the first letter titlecased at the
  beginning of a sentence always, in a list or menu or alone as the
  locale's context transforms for the type's usage say, the code of a
  fallback too; ASCII and Turkish and Azerbaijani "i" by the library, the
  rest by `names.mloc`'s titlecase table.

## Proof

`test_names` covers each type and form, the fallbacks, the contexts,
the output protocol and every refusal. `conform_names` compares the
library with ICU4C 79's LocaleDisplayNames (no substitution) for every
code of every one of ICU's 906 locales in the standard and short forms
and the three capitalization contexts: 7,510,740 names, of which all
but two classes agree:

- "root" (4,530 names): ICU writes the text "root"; the library reads
  "root" as "und", as its locale parser does, and gives und's name.
- Portuguese outside Brazil in a list or menu (6,336 names, 11 locales
  under pt_PT): CLDR's pt_PT sets the languages' uiListOrMenu transform
  to "no-change" over pt's "titlecase-firstword". ICU 79 titlecases
  there all the same, so pt's value reaches it past pt_PT's; the library
  takes CLDR's resolution, the nearest value.

## Consequences

A name costs the code's canonical form, one binary search of the codes
and one per key along the chain. The whole-locale name (UTS #35's
algorithm, the dialect names and extensions) builds on this call.
