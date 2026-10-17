# mloc-0045. Messages: MessageFormat 2, compiled once, bound once, formatted without allocation

Status: Accepted

## Context

Unicode MessageFormat 2 (UTS #35 Part 9, as CLDR 49 pins it) is the
message language a program's translators write: simple messages with
placeholders, complex ones with `.input` and `.local` declarations and a
`.match` over selectors with keyed variants, functions with options,
attributes and markup. A message is checked for syntax and for the data
model's rules (a variant's key count, a fallback variant, a selector's
annotation, duplicate declarations, options and variants), then
formatted with arguments, best effort: a failed expression shows its
fallback (`{$x}`, `{|lit|}`, `{:fn}`) and the rest formats. Names and
keys compare after NFC.

CLDR's test suite holds 461 cases; 28 use its own `:test:` functions,
so a conforming run needs a way to add functions. ICU4C 79 passes all
the others, so it is the oracle for parsing, errors, selection, bidi
and the number functions, but it ignores the Draft date options
(`fields`, `length`), so dates are checked against this library's date
formatter. ICU4C 79's implementation makes a number formatter on every
call (28 to 54 µs a message with a number, against 0.17 to 0.50 µs for
its MessageFormat 1), so the bar is its MessageFormat 1's speed.

## Decision

- **Scope:** the whole of Part 9: syntax, the data model's checks, every
  default function (`:string`, `:number`, `:integer`, `:offset`,
  `:currency`, `:percent`, `:unit` with `usage`, the Draft `:datetime`,
  `:date` and `:time` with their override options), the default bidi
  strategy, `u:id` and `u:dir`, markup and attributes, text and parts.
- **Compile:** `mlocCompileMessage` writes a compiled message into
  caller memory, the capacity measured first. It is locale-free,
  position-independent and little-endian, read byte by byte at any
  alignment, with 32-bit counts and offsets and no small fixed limit:
  a header (magic, format, flags, size, the library and Unicode versions,
  nine counts), then declarations, expressions, markups, parameters
  (options and attributes), selectors, keys, variants, pattern pieces,
  variables and the string pool. Text and literals are stored unescaped;
  names and keys in NFC; a name is one variable however it is composed.
  Parsing runs twice over the text, counting then writing, so the
  library allocates nothing: syntax errors, a variant's key count and a
  missing fallback variant are found while measuring, the checks that
  compare names or keys once the form is written. While writing, a
  hash table past the message in the caller's buffer keeps variables
  unique in linear time; the capacity asked may therefore exceed the
  size written. The same scratch serves the checks that compare: a
  declaration binding a variable an earlier declaration named (or its
  own expression names, an input's operand aside), an option named
  twice in an expression or markup, a selector that reaches no function
  through its declarations, two variants with the same keys in NFC;
  each is linear, through the table (cleared by a stamp per group) and
  two words a variable. A selector and a variant keep their source
  offsets, so every error has its offset. The compiled form is a format of the library's version.
- **Reading and writing back:** `mlocWriteMessageText` reads a compiled
  message as a catalog would, from bytes that may come from a file: the
  magic, format and size, this library's and this Unicode's versions
  (another's is `mloc_errorDataVersion`), the counts against the size in
  64 bits, and every record's kind, indexes, ranges and strings (each in
  the pool, well-formed UTF-8, names and identifiers MF2 names), so
  nothing read leaves the bytes and every string can be written as MF2.
  The strings the records name must fit the pool together (the
  compiler never shares one; a variable's name counts once), so writing
  a message back costs no more than its size, and the length written
  saturates rather than wraps. Each expression and markup is named at
  most once, in the compiler's order (the declarations', then the
  pieces'), and the parameter and piece ranges together fit their
  records; these bounds are checked first, from the counts alone, so
  reading costs no more than the size either.
  The data model's rules stay the compiler's. It then writes the
  canonical text: a simple message when there are no declarations and
  the pattern does not start with "." past its spaces; else each
  declaration, `.match` with its selectors and each variant on a line of
  its own, `{{...}}` for a quoted body; one space between an expression's
  parts, the options before the attributes; literals bare when they are
  one or more name characters, else quoted with "\" and "|" escaped;
  text with "\", "{" and "}" escaped. Compiling it gives the same
  counts, and writing that again gives the same text.
- **Bind and format** (later slices): a message formatter (data set,
  locale, function table, bidi and error options) binds a compiled
  message in caller memory, making once every function whose options
  are literals; formatting takes typed arguments by name and writes one
  `mlocOutput`, best effort with an optional array of error records, or
  strict. Functions are one table for the defaults, the library's
  namespace `mloc:` and the program's.
- **Maul Unicode** becomes a run-time dependency (UTF-8 validation,
  NFC, bidi classes), at 0.3.0: an installed package, else the pinned
  release fetched.

## Proof

- `test_message` runs every case of the suite (`test/mf2_suite.inc`,
  generated by `tools/gen_mf2_suite.py` from CLDR's files): those the
  suite expects to compile compile, measured and written; those it
  refuses are refused with that error (syntax, a key count and a
  missing fallback while measuring; the comparing checks once written).
  Each data model rule has cases of its own with their offsets, and
  forty thousand names, options and variants compile in milliseconds.
- The writer: every case of the suite that compiles writes a text that
  compiles to the same counts and writes the same text again; canonical
  texts of each construct; a complex message with every byte changed to
  five values in turn is written back or refused, and every cut of it
  refused; measuring, null pointers and another library's version.
- `fuzz_message` compiles any text and checks the round trip's fixed
  point, and reads any bytes as a compiled message (made this library's
  version) without reading past them: 34 million runs clean. It counts the records
  of known messages, checks names composed and decomposed are one
  variable, the refusals of ill-formed UTF-8, NUL and arguments, the
  capacity measured exactly, and compiles every prefix of every case
  into a buffer of exactly the size measured.

## Consequences

- A catalog stores compiled messages and checks their version when it
  opens them; a compiled message from another library version is
  refused, never misread.
- The message part adds Maul Unicode's normalization tables to a
  program that links the library.
- The table hashes with FNV-1a, unkeyed: a text crafted so that its
  names collide makes a check quadratic in them. Messages are a
  program's and its translators' text; a program that compiles messages
  from strangers bounds their length first.
