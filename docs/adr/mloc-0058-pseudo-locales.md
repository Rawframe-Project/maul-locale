# mloc-0058. Pseudo-locales: en-XA and ar-XB written from the source

Status: Accepted

## Context

Most localization bugs show before any translation exists:
- text hard-coded in the program instead of taken from a catalog;
- strings concatenated instead of formatted as one message;
- layouts that cut off longer text;
- layouts that break right to left.

Pseudo-locales make them visible with the source language still
readable. `en-XA` (accented) and `ar-XB` (bidi) are the usual names: XA
and XB are private-use regions, and Android's `aapt2` writes both. Its
pseudo-localizer (Apache-2.0) was read for its ratios and kept as the
reference. Nothing was copied.

## Decision

- **`mloccat pseudo DIRECTORY SOURCE`** writes `en-XA.mf2` and
  `ar-XB.mf2` into the directory: catalog sources of the set with the
  pseudo locale.
  - The program compiles them with the set and loads them as
    translations.
  - Entries the source keeps (`@translate no`) and obsolete entries are
    written as they are.
- **Only pattern text changes.** Placeholders, markup, declarations,
  options, variant keys (quoted ones too) and escapes stay as written.
  - The message's leading space and bidi marks (MF2's `o`) stay
    outside the pattern.
  - A complex message changes only inside its quoted patterns.
- **`en-XA`:**
  - Each ASCII letter becomes one precomposed accented letter
    ("Ĥéļļö"), so the text stays in NFC. The map is written out in
    `pseudo.c`.
  - Each pattern is wrapped in brackets, so truncation and
    concatenation show.
  - Each pattern is padded with middle dots: 40 percent of its text's
    code points, rounded up. An escape counts as one; placeholders do
    not count.
- **`ar-XB`:** each run of a pattern's text is wrapped in RLO and PDF,
  so it shows mirrored in a right-to-left layout. Placeholders stay
  outside the runs.
- **Every pseudo message is compiled** before it is written. A message
  whose pseudo form would not compile is reported and nothing is
  written. No case is known.

## Proof

- `test_pseudo`:
  - **Exact forms:**
    - `en-XA` and `ar-XB` of a message with a placeholder;
    - a selector message with markup, and a pattern with no text;
    - a quoted variant key holding braces;
    - escapes and a quoted literal in a placeholder;
    - a quoted pattern after a mark and spaces.
  - **The MF2 test suite:** all 300 of its 461 messages that compile
    keep compiling in both forms. Each one's canonical text, with
    accents undone and the pseudo marks removed, equals the source's.
  - **The tool:**
    - It writes both files, with the locale set and the kept and
      obsolete entries as written.
    - The set of source, `en-XA` and `ar-XB` compiles.
    - A directory that does not exist, or a source that does not read,
      fails.

## Consequences

- A program shows `en-XA` like any locale (the catalog chain of record
  mloc-0055, with `en` after it). Plural rules and formats come from
  `en`, so only the text looks foreign.
- The 40 percent padding is applied to short text as well. Android pads
  short strings more, but one ratio keeps the length report (record
  mloc-0057) predictable.
