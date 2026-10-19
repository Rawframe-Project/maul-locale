# mloc-0060. XLIFF 2.2 exchange: codes over original data

Status: Accepted

## Context

Professional translation runs through CAT tools and services that
exchange XLIFF: memoQ, Trados, Phrase, Crowdin and Okapi. XLIFF 2.2
(OASIS, 2025) marks inline codes so tools protect them. It also stores
the codes' native text in the unit's original data, so a file's
round trip loses nothing.

An MF2 message has things a translator must not change:
- placeholders and markup inside its patterns;
- in a complex message, declarations, selectors, variant keys and the
  quotes around each pattern.

## Decision

- **`mloccat export-xliff SOURCE TRANSLATION XLIFF`** writes one
  translation:
  - The `<file>` takes the source's name, `canResegment="no"` and
    `xml:space="preserve"`.
  - **One unit per key the source asks to translate:** `id` is `u` and
    its number, `name` is the key.
  - **Notes:** the source entry's comment and its `@param` lines, with
    the categories `comment` and `param`.
  - **Size:** `@mloc-max-length` is the Size and Length Restriction
    module's `slr:sizeRestriction`, under the file's
    `slr:profiles generalProfile="xliff:codepoints"`.
  - **One segment holds the message:**
    - Pattern text is text, with MF2's escapes undone.
    - Placeholders are `<ph>`.
    - Markup is `<sc>` and `<ec>`: paired within a pattern, or isolated
      when alone.
    - Syntax outside the patterns is `<ph canDelete="no">`.
    - Every code points at a `<data>` of the unit's original data that
      holds its MF2 text, one per distinct text, shared by source and
      target. Text and data joined give the message back.
  - **Codes in the target:** the target's codes take the numbers of the
    source's codes with the same data, so a reordered target stays
    linked.
  - **State:** `translated`, or `initial` when stale (record
    mloc-0056). A key with no translation has no target.
  - Characters XML cannot hold are written as `<cp>`.
- **The split** of a message into text, codes and syntax
  (`mf2_parts.c`) is shared with the pseudo-locales (record mloc-0058).
  `pseudo.c` now writes from it.
- **`mloccat import-xliff SOURCE TRANSLATION XLIFF`** reads each unit's
  target back:
  - Elements are matched by local name, so a tool's own prefix reads.
  - Every segment of a unit is joined in order, so a tool that split
    one still imports.
  - **Content to MF2:**
    - Text has `\`, `{` and `}` escaped.
    - A code becomes its data, and `<pc>` becomes its start data,
      content and end data.
    - `<cp>` becomes its character, and `<mrk>` its content.
    - `<sm>` and `<em>` mark nothing.
  - **Records:** a target whose segments are all past `initial` records
    the hash of its source read back. An `initial` one keeps its
    record.
  - **Skipped and reported:** kept keys, unknown keys and units without
    a target.
  - **Refused, and nothing written:** a unit without a name, a code
    naming no data, a target that does not compile, another document,
    broken XML.
- The XML reader of the generator (`tools/mlocgen/xml.c`) now records
  where each element stands in its parent's text, so mixed content
  keeps its order.
- **Not this step:** the Plural, Gender and Select module.
  - Its expansion of selection messages to the target's plural
    categories, with CLDR's examples as notes, is catalog slice (g2).
  - Until then a selection message is one segment with its syntax
    protected.

## Proof

- `test_xliff`:
  - **An export, byte for byte:**
    - notes and the size restriction;
    - data shared by a source and a reordered target;
    - markup paired and isolated, and the syntax of a selection
      message protected;
    - escapes undone, `&` and `<` escaped;
    - `initial` for the stale translation;
    - the kept key left out.
  - **A CAT tool's file imported:**
    - the `xlf:` prefix, `<mrk>`, `<pc>` and `<cp>`;
    - a unit split into two segments;
    - the source's hash recorded, checked against the reference XXH64;
    - an `initial` target unrecorded;
    - a kept key and an untranslated unit left out.
  - **Refusals:** a unit without a name, a code naming no data, a
    target that does not compile, another document, broken XML, a
    missing file. None writes anything.
  - **Export then import:** the values come back with their escapes,
    and the stale record is kept.
- `test_genxml`: children's places in mixed content, CDATA and entities
  among them.
- **xmllint 2.9.14 with the OASIS XLIFF 2.2 schemas, run here:**
  - The core schema with the size restriction and PGS modules validates
    the test's files.
  - The suite set (the MF2 test suite's 313 compiling messages with
    their `en-XA` pseudo-locale) validates. It imports back to the same
    messages. One differs only in spelling: a `\|` in pattern text came
    back as `|`, which MF2 reads the same.

## Consequences

- An import escapes only what MF2 requires in text. A message can come
  back spelled differently but not meaning differently.
- A tool that drops `canDelete="no"` lets a translator delete syntax.
  The import then refuses the message, since it no longer compiles.
