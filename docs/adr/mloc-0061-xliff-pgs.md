# mloc-0061. XLIFF's Plural, Gender and Select module: selection messages by case

Status: Accepted

## Context

Record mloc-0060 exchanged a selection message as one segment, with its
declarations, selectors and keys as protected codes. That spares the
syntax, but leaves two problems to the translator:

- **Categories.** The source has the source language's plural
  categories. The target may need others: Czech needs few and many,
  which English lacks; Arabic needs six.
- **Rules.** A translator knows the right form of a word for a number,
  but rarely CLDR's names for the categories.

XLIFF 2.2's Plural, Gender and Select module (section 4.9) answers both:

- `pgs:switch` on the unit names the selectors;
- each segment carries one variant, with `pgs:case` holding its tuple of
  values;
- section 4.9.7 recommends:
  - expanding plurals to the target locale's categories;
  - keeping a fixed segment order;
  - adding notes of each category's example numbers for the target.

## Decision

- **A message qualifies** when everything below holds; any other
  message stays one segment as in record mloc-0060.
  - Its only declarations are `.input`s of the variables it matches on,
    each used once, in at most 4 selectors.
  - Each selector's function is one of these:
    - `:number` or `:integer`: a plural; an ordinal with
      `select=ordinal`; a select with `select=exact`;
    - `:string`: a select.
  - Every key is a value PGS can carry: not empty, and with no space or
    control character. A plural key is a number or a category name.
  - No select key is the literal `other`, which PGS reads as the
    fallback.
  - It needs at most 256 segments.
  - It is read from its canonical text (mlocWriteMessageText).
- **The unit:**
  - **Switch:** `pgs:switch` names each selector as `plural:`,
    `ordinal:` or `select:` and its variable.
  - **Values:**
    - A plural or ordinal selector's values are the source's exact
      numbers in numeric order, then the target locale's categories in
      CLDR's order.
    - A select's values are the source's keys in byte order, then
      `other`.
  - **Segments:** one per tuple of values, the first selector varying
    slowest, with ids `s1`, `s2`, and so on.
- **A segment's texts:**
  - **Lookup:** a segment shows the variant that the tuple selects: an
    exact match of the case values first, else the first variant whose
    keys are each the value or `*`. `*` counts as `other`.
  - **The source:** the source variant. This seeds a category the
    source lacks with its fallback text, so Czech few starts from
    English's `*`.
  - **The target:** the existing translation's variant, when the
    translation has the same selectors. A translation with other
    selectors makes the message one segment, so its text is not lost.
- **Codes:** a variant's pattern takes codes as in record mloc-0060, the
  pattern's quotes not among them. Code ids are unique across the unit's
  segments, and the original data is shared by all of them.
- **Notes:** each plural segment has a note with
  `appliesTo="target"`, `ref` its segment and
  `category="plural_examples"`. The note lists the target's numbers in
  its category, computed from the library's own plural rules:
  - the integers from 0 to 1,000 as runs (`3~10`), at most six items,
    then `…`;
  - the powers of ten from 10,000 to 1,000,000;
  - for cardinals, the decimals 0.0 to 2.5.
  - A tuple of several plurals names each variable.
- **Import:**
  - **Fold:** a unit with `pgs:switch` becomes one message: the
    source's declarations and `.match`, then a variant per segment in
    segment order. Each key is its case value: `other` becomes `*`; a
    value MF2 cannot write bare is quoted.
  - **Refused:** a switch other than the source's now; a segment
    without a case; a case of another count of values; a unit
    translated in part.
  - **Record:** the import records the source's hash when the segments'
    sources fold to the same canonical message as the source's own
    expansion now. Otherwise it records the hash of what they fold to,
    so the translation shows as stale.
- **Compiling means the full compile.** The PO, XLIFF and pseudo-locale
  paths now share `MessageCompiles` (`tools/mloccat/message_text.c`).
  The size query alone had passed messages whose data model is wrong,
  such as a variable declared twice, which a catalog cannot hold.

## Proof

- `test_xliff`:
  - **An English source into Arabic:**
    - An exact 0, then zero, one, two, few, many and other in that
      order.
    - Notes of CLDR's form:
      - `0, 0.0` for zero;
      - `3~10, 103~110, …` for few;
      - the decimals for other.
    - The many segment seeded from `*`, in both source and target.
    - Markup paired within a segment, code ids unique across the unit.
    - An ordinal with a select: `other fem` and `other other`.
    - A select key with a space left as one segment.
  - **Import:** the segments fold back in order, with `other` as `*`,
    a retranslated many segment kept, and the source's hash recorded.
  - **Refusals:** each of the four, writing nothing.
- `test_pseudo`, `test_po`, `test_xliff`: a message declaring a
  variable twice is refused. Each test fails when the full compile is
  replaced by the size query.
- **Run here:**
  - **Schemas:** the OASIS core schema rejects `pgs:case` on
    `<segment>`. Section 3.2.2.6 lists "attributes from the namespace
    urn:oasis:names:tc:xliff:pgs:1.0" there, so the core schema omits
    an attribute that the specification's text allows. With that one
    attribute added to a local copy, the following validate under
    xmllint 2.9.14:
    - the test's Arabic file;
    - a Czech file;
    - the suite set.
  - **The suite set** now holds the MF2 test suite's 293 messages that
    compile fully, 6 of them as PGS units. The 20 others are wrong in
    their data model, and every import now refuses them.
  - **Round trips:**
    - XLIFF export, import and export again gives the same bytes, with
      every translation recorded and none stale.
    - PO export, import and export again gives the same bytes, and
      `msgfmt -c` accepts it.

## Consequences

- An imported selection message is the expanded one: it has a variant
  per target category, and is spelled with bare or quoted keys. It
  selects the same.
- A source whose selectors change after export refuses the old file's
  units. The translator exports again.
- Gender as PGS names it has no MF2 function. A gender chosen by
  `:string` is a select.
