# mloc-0059. PO exchange: one translation per file, the key in msgctxt

Status: Accepted

## Context

Many translators and services work in GNU gettext's PO files: Poedit,
Weblate, Transifex and Crowdin read and write them. PO keys entries by
their source text, while a catalog keys them by a stable key. PO
selects plurals with its own msgid_plural and msgstr[n], while MF2
selects inside the message. The W3C message resource README shows the
mapping this record follows: the key as msgctxt, MF2 text as msgid and
msgstr, and the `mf2-format` flag.

## Decision

- **`mloccat export-po SOURCE TRANSLATION PO`** writes one translation:
  - **The header:**
    - the source file's name as `Project-Id-Version`;
    - the translation's `Language`;
    - UTF-8;
    - `X-Generator: mloccat`;
    - empty translator fields, so the output is the same each time.
  - **One entry per key the source asks to translate**, in the source's
    order, flagged `mf2-format`:
    - `msgctxt` is the key, `msgid` the source's MF2 text, and `msgstr`
      the translation's, or empty when it has none.
    - The source entry's comment and its `@param` lines become
      extracted comments (`#.`).
    - A stale translation (record mloc-0056) is flagged `fuzzy`.
      Research 59 planned the old source as `#| msgid`, but the record
      holds only its hash, so the flag alone marks it.
    - Kept keys (`@translate no`) are left out.
  - A string holding a line feed before its end is split after each
    one, as gettext writes it.
- **`mloccat import-po SOURCE TRANSLATION PO`** puts each translated
  entry back:
  - A key the translation lacked is added to the section the source
    has it in. The section is added if missing.
  - Each value must compile; if any does not, the import is refused and
    nothing is written.
  - A non-fuzzy entry records its msgid's hash as the source it
    answers. If the source changed after the export, the translation
    then shows stale, as it should. A fuzzy entry keeps the record it
    had.
  - Skipped and reported: an empty msgstr, a kept key, and a key the
    source lacks.
  - Refused: an entry without msgctxt, a plural entry, and a file that
    does not read.
  - The file is written through `ResourceWriteFile`, the shared edit
    helpers that `mloccat accept` now uses too.
- **The PO reader** (`po.c`) reads gettext's forms:
  - C escapes, octal and hex;
  - strings continued over lines;
  - CRLF;
  - extracted comments and flags.

  It reads past translator comments, references, previous strings and
  obsolete entries. It refuses text after a string, an unknown keyword,
  a keyword given twice, and an entry missing its msgid or msgstr.
- **Shared tool code:**
  - `catalog_set` indexes each input's entries by key
    (`CatalogFindEntry`) and keeps each entry's section.
  - `stale` tells an entry's state (`SourceStateOf`), shared by
    compile, report and export.
  - `resource_edit` sets values and metadata, finds or adds entries, and
    replaces files.

## Proof

- `test_po`:
  - **The reader:**
    - The header; escapes (`\t \" \\`, octal, hex); a string over two
      lines; CRLF; extracted comments with an empty line; flags among
      others; the msgid's line.
    - An obsolete entry is read past, and an entry may have no context.
    - The writer's output reads back the same, with the split string
      as gettext writes it.
  - **The refusals:** a plural entry, an open string, an unknown
    escape, a string with no keyword, a keyword twice, an entry without
    msgstr or msgid, an unknown keyword, text after a string.
  - **An export, byte for byte:** the header, the comment and
    `@param`, fuzzy for the stale entry, an empty msgstr, and the kept
    key left out.
  - **An import into an empty translation:**
    - A translation is recorded with its msgid's hash.
    - A fuzzy one is not recorded.
    - An empty msgstr, a kept key and an unknown key are left out.
    - A message that does not compile, an entry without msgctxt, a
      plural entry and a missing file are each refused, and nothing is
      written.
  - **Export then import:** the values come back, the stale record is
    kept, and the fresh entry is recorded.
- **GNU gettext 0.21, run here as an oracle:**
  - The suite set: the 313 messages of the MF2 test suite that compile,
    as a source, with its `en-XA` pseudo-locale as the translation.
  - `msgfmt -c` accepts the export.
  - `msgcat` rewrites it, wrapped and with `--no-wrap`. Each rewrite
    imports back to the same 313 values.

## Consequences

- One PO file per locale, as gettext keeps them. A program's build
  exports and imports each translation by itself.
- PO has no stable place for a translation's record. A translator's
  tool that keeps flags but drops metadata still round-trips the text,
  and the import records it afresh.
