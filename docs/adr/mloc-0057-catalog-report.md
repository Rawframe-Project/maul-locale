# mloc-0057. The catalog report: coverage and findings per locale

Status: Accepted

## Context

A program's translations drift. Keys are added to the source and not
yet translated. Source messages change after their translation
(record mloc-0056). Keys are removed from the source while translations
keep them. A translator translates a brand name the source keeps. A
translation runs longer than its button. A team needs this counted per
locale, readable by people and by a CI job that fails on what it
chooses.

## Decision

- **`mloccat report [--json] [--fail-on KINDS] SOURCE [TRANSLATION...]`**
  reads the set as the compiler does. For each locale it gives:
  - **Coverage:** the translated keys of those the source asks to
    translate. A key the source keeps (`@translate no`) is not counted.
  - **The findings:**
    - `missing`: a key to translate that the translation lacks.
    - `stale`: a translation made from another source message.
    - `unrecorded`: a translation with no `@mloc-source`. It is listed
      apart from `stale`, since nothing says it is old.
    - `orphaned`: a translation's key the source lacks.
    - `differs`: a translation of a kept key. The compiler ignores it,
      so the translator's work would be lost silently.
    - `long`: a message above its source entry's `@mloc-max-length`.
      - Length is counted in code points, XLIFF's default profile.
      - The text is formatted in its own locale, without isolation
        marks, using the source entry's `@mloc-example` arguments
        (`$name=value` parts, each value running to the next ` $`).
      - A variable without an example is written as its fallback
        (`{$name}`).

  The source is reported too, for its own long messages.
- **Output:** a text table of counts per locale, then a line per finding
  (`PATH: KIND KEY`). With `--json`, one object holds the source's
  locale, the key count, and per locale its counts and each kind's
  keys; a long message also carries its length and limit.
- **Exit status:**
  - 1 when a finding of a kind in `--fail-on` (a comma-separated list)
    is found;
  - 2 for a set that does not read, or an unknown option or kind;
  - else 0.
- **Checks:** `@mloc-example` must begin with a variable and hold an
  `=`.
- **Tool structure:**
  - `report.c` builds the findings.
  - `report_write.c` writes text and JSON.
  - `json.c` holds the JSON string writer that `mloccat read` uses too.

## Proof

- `test_report` reports a source, a Turkish translation and a Finnish
  one:
  - **The source:** its own long message, 21 code points against a
    limit of 12, with the example name.
  - **The Turkish translation:** coverage 3 of 4; each kind found
    exactly once. Not reported:
    - a kept key equal to the source;
    - a recorded, unchanged translation;
    - a message within its limit.

    The long message is measured at 23 code points in its locale.
  - **The Finnish translation:** its own count and length.
  - **The output:**
    - the text table's row and the finding lines;
    - the JSON's head and the Turkish locale's object.
  - **The kind lists:**
    - every name and the empty list are read;
    - an unknown name, a capital and a prefix are refused.

## Consequences

- A CI job runs `mloccat report --fail-on missing,stale,long` to keep a
  release's translations complete.
- The length check measures one variant per set of examples. A selector
  message needs examples that pick its longest variant.
