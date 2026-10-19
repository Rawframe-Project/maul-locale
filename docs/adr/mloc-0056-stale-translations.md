# mloc-0056. Stale translations: the hash of the source message answered

Status: Accepted

## Context

A translation is made from one version of its source message. When the
source changes later, the translation may say something else. It is
still better than none in the program (a slightly old French text
beats English in a French UI). But a translator must see it again, and
a report must count it. That needs a record, in the translation, of
which source text it answers.

A text comparison would also flag changes that do not matter: a source
re-indented, a literal quoted differently, a placeholder spaced
another way.

## Decision

- **The source hash:** XXH64, seed 0, of the source message's canonical
  text. The canonical text is the message compiled and written back by
  `mlocWriteMessageText`, so whitespace and quoting do not change the
  hash; text, options, variants and declarations do. It is written as
  16 lowercase hex digits, and the check refuses any other form.
- **The record:** a translation entry's `@mloc-source` metadata holds the
  hash of the source message it answers.
- **`mloccat accept SOURCE TRANSLATION [KEY...]`** writes that record:
  - It covers the keys given, or with none every translation entry the
    source has.
  - It leaves alone the entries the source keeps (`@translate no`) and
    keys the source lacks. Naming one of them is an error, and nothing
    is written.
  - It replaces an earlier record rather than adding a second.
  - It writes the file again in the resource writer's form (record
    mloc-0053), to a temporary file that then replaces the original at
    once. On Windows that is `MoveFileExA`, since `rename` does not
    replace a file there.
- **Stale:** an entry whose record differs from the source's hash now.
  An entry without a record is not stale, just unrecorded; the report
  (catalog slice (d), next) lists those apart. `mloccat compile` still
  compiles a stale translation and counts the stale ones in each
  catalog's line.
- **Tool structure:** the compiler and the commands that follow read a
  set the same way (`tools/mloccat/catalog_set.c`). The hash is
  `tools/mloccat/stale.c`, and the command is `tools/mloccat/accept.c`.

## Proof

- `test_accept`:
  - **The hash:**
    - Two values against the reference XXH64 over the canonical text.
      They are pinned, since they are written into translators' files.
    - Whitespace, a placeholder's spacing, the outer braces and quoting
      do not change it. A word, an option's value and a variant's text
      do.
    - A message that does not compile has no hash.
    - The hex form is read back; capitals, 15 or 17 digits and letters
      past f are refused.
  - **Accepting:**
    - A recorded hash of no source message is stale.
    - A key the source lacks, or keeps, is refused, and the file is
      left byte for byte.
    - One key is accepted, then all.
    - Each hash is written once and replaces the old one.
    - The kept entry, the orphan and the comments are untouched.
    - Nothing is stale afterwards.
    - A source whose placeholder was only re-spaced keeps its
      translation fresh. One whose text changed makes it stale.

## Consequences

- A translation tool that rewrites files keeps `@mloc-source` as plain
  metadata. Exchange formats carry it (PO's `fuzzy`, XLIFF's state)
  from catalog slices (f) and (g).
- A library version whose canonical text changes would make every
  record stale. The pinned values in `test_accept` catch that before
  release.
