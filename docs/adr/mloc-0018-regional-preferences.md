# mloc-0018. Regional preferences from one region, with the keys on top

Status: Accepted

## Context

Dates need the hour cycle and the week, units the measurement system,
printing the paper size; CLDR keeps them by region in
`supplementalData.xml` (timeData, weekData, measurementData), and
`rgScope.xml` names them as what the -u-rg key redirects. UTS #35
gives the order only for the first day (-u-fw, -u-rg, -u-ca, the
region, -u-sd, the likely region, 001) and for c12 and c24 (-u-rg, the
region, else 001). Libraries that return one value per call recompute
the region each time, and some read the hour cycle of a bare "en" from
001 (24 hours) where every user expects the US's.

## Decision

- One call, `mlocGetLocalePreferences`, fills one struct: the region
  used, the hour cycle with the allowed formats in order, the first
  day, minimal days and weekend, the measurement system and the paper
  size.
- The region is the first of -u-rg, the locale's region, -u-sd, the
  likely region, 001, for every field; a region without data takes
  001's. So "en" has the US's 12 hours, as ICU and ECMA-402 give.
- Hours are looked up for the language in the region first (timeData
  lists `ca_ES`, `en_001`), then for the region.
- Keys override: -u-hc (c12 and c24 by the allowed formats), -u-fw,
  -u-ms; -u-ca-iso8601 starts weeks on Monday unless -u-fw or -u-rg
  give the first day, and needs four days in the first week, as ISO
  8601 does. A value a key does not take is left out.
- The temperature category of measurementData is left to the units'
  preferences, which CLDR moves the measurement system towards.
- `tools/check_preferences.py` compares every region CLDR's JSON names,
  each as -u-rg too, and every language and region of timeData, in CI.

## Consequences

A formatter asks once and keeps the struct; one region decides every
value, so they never come from two places for one locale.
