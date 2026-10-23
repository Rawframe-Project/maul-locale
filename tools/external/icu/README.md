# ICU's language matcher test

ICU's data-driven test of its `LocaleMatcher`, which `test/conform_match.c`
runs against the library's matching, with the lines where the two differ
on purpose in `tools/match-exceptions.txt`. Exactly as published at
`icu4c/source/test/testdata/localeMatcherTest.txt` of
`https://github.com/unicode-org/icu` at tag `release-79.1`, the ICU
release built on CLDR 49. Covered by the Unicode License v3,
`LICENSES/Unicode-3.0.txt`.

SHA-256 of each file:

```text
5ed4ec006dcc75afbe4162141dceddf2482a936c6b65affe62eacd6948e78f17  localeMatcherTest.txt
```
