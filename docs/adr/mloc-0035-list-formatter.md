# mloc-0035. The list formatter: patterns kept in the formatter, items joined in place

Status: Accepted

## Context

Record mloc-0026 compiled in CLDR's list patterns and joined items as
ICU4C's ListFormatter does, for mixed units only; the list part
(mloc-0013's fifth step) gives programs the same joins. ICU4C's ListFormatter takes a
type (and, or, units) and a width, ICU4X's `ListFormatter` and
ECMA-402's `Intl.ListFormat` the same, and each reports items and the
text between them as parts.

## Decision

- **API** (`maul-locale/list.h`): `mlocInitListFormatter` keeps a
  locale's four patterns of a type and width (pointers into the data,
  which outlives the formatter) and its contextual rule;
  `mlocFormatList` joins an array of UTF-8 items into an `mlocOutput`.
  The items are written as they are, with no copy; nothing is
  allocated.
- **Edge cases:** no items write no text; one item is itself; an item
  with a null text and a length, or more than `UINT32_MAX` items, is
  refused.
- **Parts:** `mloc_listPartElement` for each item and
  `mloc_listPartLiteral` for the text between, as `formatToParts`
  gives them. Items are written with two part kinds in turn, so that a
  pattern with no text between its items (Japanese narrow units,
  "{0}{1}") still reports two parts; an empty item has no part.
- **Proof:** the joins are record mloc-0026's (26,964 lists equal to
  ICU4C 79); `test_list_format` covers the public call: none to four
  items, every type and width, the contextual changes through it, the
  parts, the output protocol and each refusal.

## Consequences

A list formatter is 64 bytes and costs one walk of the locale's chain
to make. The part kinds are the list's own, so a program reading the
parts of a list and of a number tells them by the call it made.
