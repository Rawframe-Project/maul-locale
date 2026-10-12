# mloc-0017. Language matching as ICU's LocaleMatcher does it

Status: Accepted

## Context

A program has the languages a user wants, in order, and the locales it
has data or translations for, and must pick one. CLDR gives the
distances in `languageInfo.xml` (language, script and region rules, the
region groups they name, and the paradigm locales) and UTS #35 sketches
how to use them, but it publishes no test data for the result, and the
sketch leaves open the choices that decide it: how ties break, how the
second wanted language weighs against the first, what a region group
means for a region inside it. ICU's LocaleMatcher (ICU4C and ICU4J
agree, ICU 65 and later) settles each of them, its builder turns the
same CLDR file into its distance table, and its data-driven test of 931
cases is the only body of expected answers. Other libraries match by
truncating tags (the RFC 4647 lookup), which gives `zh-TW` to a `zh`
user, or by exact prefixes, and every one of them has grown special
cases for Chinese, Serbian and Norwegian that the distances already
hold.

## Decision

- **Semantics.** Matching gives what ICU 79's LocaleMatcher gives for
  the same locales and options: each locale maximized with likely
  subtags (an exact `und` stays `und`; a private use tag and the
  pseudo locales XA, XB, XC and PSACCENT, PSBIDI, PSCRACK match only
  their own kind), the distance of language, script and region from
  CLDR's rules, the region partitions built from the match variables
  and the territory containment, a micro distance for subtags the tag
  spelled out, ties broken toward the more likely subtags, the
  supported locales tried in the order default, paradigm locales,
  rest, and each further wanted language demoted by CLDR's distance
  between two English regions.
- **Options:** favoring the script over the language, no demotion,
  matching only both ways, a threshold other than CLDR's default script
  distance, and a default locale or none.
- **Data.** The generator ports ICU's builder (`DistanceTable`,
  `PartitionInfo`, `TerritoryContainment` of its cldr-to-icu tool) and
  writes the pruned table as a unit of its own: language pairs, script
  pairs, region partition pairs, the region to partition map and the
  paradigm locales.
- **Shape.** The supported locales are prepared once into an array of
  small entries the caller owns, in the order they are tried; each
  match then maximizes only the wanted languages. The result is the
  index of the supported locale and of the wanted language that chose
  it; a separate call makes the locale to use from the two (the
  supported one with the wanted one's region, variants and
  extensions). Locales are given canonicalized; matching replaces no
  aliases.
- **Proof.** ICU 79's `localeMatcherTest.txt` is fetched with its
  SHA-256 pinned and run in CI; a case that differs is listed with its
  reason, as the resolution exceptions are.

## Consequences

Users of ICU get the same answers, and the answers change only with
CLDR's distances. Matching costs one maximization per wanted language
and a few table lookups per supported locale.
