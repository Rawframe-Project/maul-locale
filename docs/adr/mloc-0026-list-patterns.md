# mloc-0026. List patterns: compiled in by style, joined as ICU joins them

Status: Accepted

## Context

CLDR 49 gives each locale nine list styles (and, or and unit, each
wide, short and narrow), four patterns each: for two items, the start,
the middle and the end. Root aliases the narrow and short styles to
the wider ones and the wide unit style to the short one. Mixed units
("5 ft, 3 in") join their parts by the unit lists, and the list part
(mloc-0013's fifth step) formats lists of any items. ICU4C's ListFormatter takes
each pattern from the first locale having it, then the style its root
alias names, and changes Spanish's "y" and "o" and Hebrew's vav by the
item that follows.

## Decision

- **Data** (`src/list_tables.h`): compiled in, the list part's units.
  The distinct pattern sets, a unit of no locale in the table shape of
  the number part (four string indexes a set); per locale key the set of
  each style (eighteen bytes), filed where it differs from the key's
  chain. Values are CLDR's resolution, root's aliases among styles
  followed from the locale, which gives what ICU reads: list paths are
  singletons in CLDR's logical groups, so the production step drops
  only markers and repeats. The data grows by 14 KB.
- **Validation:** each pattern holds "{0}" and "{1}" once and no other
  brace; a locale's set index is checked against the sets unit when it
  is read, as a validator reads no other unit.
- **Joining** (`src/list_join.c`): one item as it is, two by "two",
  more by the start, middle and end patterns, written left to right
  with no partial copy (a pattern is the text before its "{0}" and the
  text after it). ICU's contextual changes, for the language asked for:
  Spanish's "{0} y {1}" becomes "e" before an item starting with i or
  hi (not hia, hie), "{0} o {1}" becomes "u" before o, ho, 8, or "11"
  alone or before a space; Hebrew's "{0} ו{1}" takes a dash before an
  item whose first code point is not of the Hebrew script (Unicode 18).
- **Scope:** the join is internal; mixed units use it first, and the
  list part adds its public functions over the same units.
- **Proof** (`docs/locale/research/oracle/lists` in the docs
  repository): ICU4C 79.1rc against the library on 107 locales by nine
  styles by 28 lists (one to five items, the contextual cases): all
  26,964 agree.

## Consequences

Lists need no data file of their own. A locale's lists cost one small
unit; the sets are shared. The contextual rules are code, not data:
CLDR gives none, so they follow ICU's and change with it.
