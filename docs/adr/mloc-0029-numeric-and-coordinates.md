# mloc-0029. Numeric durations and coordinates: CLDR's patterns, written as ICU writes them

Status: Accepted

## Context

ICU4C's MeasureFormat has a numeric width that writes a mixed unit of
hours, minutes and seconds by the locale's duration patterns
(`durationUnits`: "h:mm", "m:ss", "h:mm:ss"; "1:05:09"), and CLDR 49
gives each locale coordinate patterns (`coordinateUnit`: "{0}°N",
"{0} degrees west") by width. Records mloc-0024 and mloc-0027 write
units and mixed units by name only.

## Decision

- **Data** (`src/unit_tables.h`): a unit of each locale's fifteen
  patterns (three duration patterns, then the four direction patterns
  of each of three widths) in units.mloc, filed
  where the resolved patterns differ from the chain's, read as one
  record (`mlocFindUnitPatterns`).
- **Numeric** (`mloc_unitWidthNumeric`): a mixed unit whose parts are,
  in order, hours and minutes, minutes and seconds, or hours, minutes
  and seconds (hour-and-second taking the last pattern with minutes 0,
  as ICU does) is split as record mloc-0027 splits it and written in
  its pattern: each field's amount with the locale's symbols, two
  digits where the field letter is doubled, the larger fields grouped,
  quoted text literal. A negative or non-finite amount, and any other
  unit, are written narrow, as ICU writes them; the display name of the
  width is narrow's.
- **Coordinates** (`mlocUnitOptions.coordinate`): an angle, simple or
  mixed, as a latitude or longitude: the direction by the amount's sign
  (north or south, east or west; NaN north or east, as unsigned), the
  amount written unsigned whatever the sign display, inside the
  direction pattern of the width (numeric taking narrow's). Refused
  (mloc_errorInvalid): a unit not an angle, a usage, and ranges.
- **Proof** (`docs/locale/research/oracle/numeric`): ICU4C 79.1rc's
  formatMeasures on 105 locales by four mixed units by ten amounts:
  4,200 lines, all equal but 91 where ICU groups a field's thousands
  the locale's minimum grouping digits do not (Bulgarian "1 234:30");
  the library follows CLDR. The coordinate patterns read back equal to
  CLDR's JSON for every locale and width (657 records).

## Consequences

The numeric width and coordinates add a 15-pattern unit per locale
that differs (units.mloc grows to 3,163,838 bytes) and two code paths
over the mixed unit's split; the formatter stays 3,584 bytes.
