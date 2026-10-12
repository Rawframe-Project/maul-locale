# mloc-0015. The locale value, strict parsing and the output struct

Status: Accepted

## Context

Every part takes a locale, so its form decides the cost of every call
that resolves one. UTS #35 defines the grammar of a `unicode_locale_id`
and its canonical syntax. Lenient parsers (accepting `en-US-`,
duplicate variants or repeated keys, and fixing them silently) are a
regret of other libraries. Every formatting part also needs one way to
write text (record mloc-0007).

## Decision

- `mlocLocale` is a 128-byte value with readable fields: the language
  (8 bytes), script (4) and region (4), each in canonical case and
  NUL-padded, and the variants and extensions as canonical text (112
  bytes, NUL-padded). Every byte is defined, so two locales compare
  with `memcmp`, and nothing is allocated.
- `mlocParseLocale` accepts the UTS #35 grammar with `-` or `_`, in any
  case, and `root` as `und`. It refuses, with the byte offset of the
  subtag at fault: a malformed subtag, a tag that begins with a script
  (specialized use, not CLDR's), and duplicates (variants, singletons,
  keys, attributes). It applies canonical syntax: case, variants and
  attributes in order, extensions by singleton with private use last,
  keys in order, a `u` value of `true` removed. Replacing aliases is
  canonicalization, a separate call that reports what it changed.
- A locale whose variants and extensions pass 112 bytes, or that has
  more than 64 subtags, is refused with `mloc_errorCapacity`.
- `mlocOutput` is the one way any part writes text: a buffer, its
  capacity, the length written (or needed, with `mloc_errorCapacity`,
  and then the buffer's contents are unspecified), and an optional part
  callback with its context, called in order after the whole text is
  written and only on success. No NUL is written.

## Consequences

Resolving a locale for data reads fixed fields, never a string to
split. A tag such as `en-u-ca-gregory-nu-latn-hc-h23-co-phonebk-fw-mon`
fits; one with dozens of extension values does not and is refused, not
cut.
