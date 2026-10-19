# mloc-0053. The catalog source: the W3C message resource syntax

Status: Accepted

## Context

A program's messages need a source file that authors keep in version
control and translators' tools read: keys, MF2 messages that span
lines, and comments and metadata attached to them. MF2 defines one
message, not a file of them. The MF2 working group's resource work
moved to the W3C Internationalization WG. On 2026-10-16 the WG
published an Editor's Draft, *Message Resources*, with an ABNF
grammar:
- a frontmatter (`@locale en-US` then `---`);
- `[section.path]` heads;
- `key = message` entries whose values continue on indented lines;
- `@property value` metadata and `#` comments, each attached to what
  follows it.

The format is a draft and may change.

## Decision

- **Source catalogs use the draft's syntax**, pinned by commit and the
  grammar's SHA-256 in `tools/resource-release.txt`. The draft is not
  copied into the repository.
- **The syntax lives only in the host tool**, `mloccat`
  (`tools/mloccat/`). A change to the draft changes the tool, never
  the library or a program's compiled catalogs.
- **The reader** (`resource.c`) follows the grammar rule by rule and
  reads the data model the draft's earlier TypeScript definition
  gives:
  - Escapes are processed, except MF2's `\\`, `\{`, `\|` and `\}`
    inside values, which pass through to MF2.
  - Each value line loses its unescaped leading and trailing
    whitespace, and the value loses its empty first and last lines.
  - Lines are joined by LF.
- **The reader enforces the rules the grammar's notes state:**
  - at most one frontmatter, and nothing but comments and metadata
    before it;
  - no comment or empty line between metadata and what it describes;
  - no metadata at the end with nothing to describe.
- **Beyond the grammar, two narrowings:**
  - A `\x`, `\u` or `\U` escape must name a Unicode scalar value. A
    UTF-8 text holds no surrogate.
  - An identifier part may not be empty, as the data model requires.
    An escaped newline alone writes nothing.
- **The writer** (`resource_write.c`) writes any model back in one form
  that reads to the same model. It escapes what the grammar does not
  allow unescaped, and whitespace that reading would drop.
- **The check as a catalog source** (`resource_check.c`):
  - A `@locale` the library parses.
  - Keys made of the section's and the entry's parts joined by dots.
    A part holding a dot is refused, since two keys would read as one.
    A key given twice is refused.
  - Every message compiles with `mlocCompileMessage`, except
    `@obsolete` ones.
  - An empty message needs `@allow-empty`.
  - The properties the tool understands take their forms, on what they
    may describe:
    - `@locale`;
    - `@param $name ...`;
    - `@translate yes|no`, and the README's `@do-not-translate`;
    - `@obsolete` and `@allow-empty`;
    - the library's own `@mloc-max-length N` and `@mloc-source HASH`.
  - Other properties are kept and ignored.
- **`mloccat read FILE`** writes the data model as JSON;
  **`mloccat check FILE`** checks a source catalog.

## Proof

- `test_resource`:
  - the draft README's example as its data model;
  - escapes, comment attachment, and each refusal with its line;
  - the catalog checks.
- **The grammar as oracle** (in the private research, never shipped):
  the ABNF run by an ABNF parser over 15,000 generated resources, a
  quarter of them mutated. No resource the grammar refuses is read.
  Every resource read and refused here is one the grammar's notes or
  the two narrowings refuse.
- **A second reader:** `mf2-resource` 3.0.0 (Rust, MIT). On the
  resources both read, the data models agree once its differences from
  the current draft are set aside: it keeps trailing whitespace, does
  not process `\x`, `\u` or `\t` in identifiers, and refuses tabs and
  empty lines inside values, all of which the pinned grammar allows.
- `fuzz_resource`: any bytes are read or refused with a position. What
  is read is checked, written, and read again to the same model. Its
  seeds are in `test/seeds/fuzz_resource`.

## Consequences

- When the draft changes, the reader and the pin change together. The
  compiled catalogs are not affected.
- The draft's vocabulary of properties is unsettled (`@translate` and
  `@do-not-translate` both appear). The tool reads both until the
  drafts agree.
