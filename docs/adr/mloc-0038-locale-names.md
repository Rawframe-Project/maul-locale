# mloc-0038. Whole-locale names: UTS #35's algorithm as CLDR's tools run it

Status: Accepted

## Context

A locale's name in another ("British English", "Hindi (Western Digits,
Hybrid: English)") is UTS #35 Part 2's Locale Display Name Algorithm.
CLDR publishes the expected names of 111 locales in both modes
(`testData/localeIdentifiers/localeDisplayName.txt`), written by its own
NameGetter. ICU4C 79's LocaleDisplayNames differs from that file on
1,820 of its 3,433 names: it does not compose the t extension, writes
keys as "hours=h12", matches dialect names only as a prefix (no
"Mexican Spanish (Cyrillic)") and joins variants in capitals. ECMA-402
and ICU4X stop at the language id.

## Decision

- **API** (`maul-locale/names.h`): `mlocGetLocaleName` writes a locale's
  name in a display locale, with dialect names (the zero value, as
  ECMA-402's default) or standard names, a form for the component names
  and the capitalization contexts of language names.
- **The algorithm is NameGetter's** (CLDR release-49-beta3):
  - the language by the whole id (dialect names, no extensions), then
    language-script-region, language-script, language-region, the
    language; standard names take the language alone;
  - then the script and region the match left out and the variants;
  - then the u attributes and keys, the t extension's language and its
    fields, in that order, each by its value's name when the locale has
    one, else by the key-type pattern: the currency's symbol for cu, the
    reorder codes' script names for kr, the t language's own name joined
    by the separator after "Transform" (or h0's value's name), the
    subtags for the rest; a key without a value has "true";
  - then the other extensions and private use by their singletons, as
    written;
  - all joined by the locale's separator and pattern, every name's
    brackets nested by root's replacement, which the generator checks
    every locale inherits.
- **No allocation:** the parts are read from the locale's tail in place
  and written straight into the output; a pattern is split once at its
  placeholders, and one without "{0}" before "{1}" falls back to root's.

## Proof

`test_names` covers both modes, the extensions, the forms, the
capitalization, the output protocol and the refusals.
`conform_locale_names` runs CLDR's file in the cldr CI job: all 3,433
names agree, but for 15 listed in `tools/locale-name-exceptions.txt`:

- time zones (3) and subdivisions (4) stand as written until their
  names are in the data;
- root (8): CLDR's tools give root a u value's code as its name, so the
  key's name is left out; root has no names, and the library writes the
  key's code and the value.

## Consequences

A name costs one lookup per part and per key, plus the language's
longest match; a locale's tail holds at most 40 items of each kind.
