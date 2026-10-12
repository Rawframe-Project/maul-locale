# mloc-0007. The shape of the API

Status: Accepted

## Context

Eighteen parts must read as one library and follow the family's API
rules, and formatting must never allocate.

## Decision

- Values (`mlocLocale`, `mlocDecimal`, the date and time values,
  `mlocAmount`, `mlocMeasure`) are fixed-size and copyable.
- Data is `mlocData` (one source) and `mlocDataSet` (a caller array of
  sources searched in order).
- A formatter is made by `mlocInit<Kind>(out, dataSet, locale,
  options)`, which resolves the locale through the part's fallback
  chain and returns `mloc_fellBack` when it used a fallback.
- Options are structs whose zero value is the default; conflicting
  options are refused.
- Every formatting call writes UTF-8 into one `mlocOutput`: the
  buffer, its capacity, the length written or needed, and an optional
  callback that receives the result's parts as spans. A buffer that is
  too short returns `mloc_errorCapacity` with the length needed; text
  is never cut.

## Consequences

Every part's calls look alike, and a UI can style any part of any
result without a second API.
