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
  The text is measured before it is written: a variable's name is
  written at each reference, so a crafted message could ask for far more
  than its size; a text of 1 GiB or more, which the compiler would
  refuse, is `mloc_errorData`.
  The data model's rules stay the compiler's. It then writes the
  canonical text: a simple message when there are no declarations and
  the pattern does not start with "." past its spaces; else each
  declaration, `.match` with its selectors and each variant on a line of
  its own, `{{...}}` for a quoted body; one space between an expression's
  parts, the options before the attributes; literals bare when they are
  one or more name characters, else quoted with "\" and "|" escaped;
  text with "\", "{" and "}" escaped. Compiling it gives the same
  counts, and writing that again gives the same text.
- **Bind and format:** `mlocInitMessageFormatter` keeps the data set,
  the locale and the options: strict or not, and the program's
  functions, each a namespaced name (not `u:` or `mloc:`, the
  library's) with resolve, format, match and better entries, refused
  when a name is ill-formed or given twice or resolve is missing.
  `mlocBindMessage` reads the compiled message as the writer does, finds
  each expression's function (none, `:string`, the program's by name, or
  unknown), and measures each chain of declarations, refusing
  (`mloc_errorInvalid`, which no buffer would cure) one of more than 64
  expressions each naming the next, more than 32 selectors, and a
  message whose format call would resolve more than 65536 expressions
  (call-by-name resolves a local again at each reference, an option's
  twice, so locals each named twice by the next grow as a power of two;
  a call also stops there, with an unsupported operation, whatever its
  functions read), and
  (`mloc_errorData`) one naming a later declaration (crafted bytes,
  which could loop); the bound message points into the compiled one and
  the formatter and is read-only.
  `mlocFormatMessage` resolves each placeholder and selector where it
  stands, a local through its declaration on the stack (call-by-name: no
  memory per call), an argument by its name after NFC; it writes text,
  writes markup as nothing, and writes a failed expression as its
  fallback (`{$x}`, `{|literal|}` with "\" and "|" escaped,
  `{:function}`), each fault (kind and source offset) kept in an optional
  caller array and counted, the call returning `mloc_withFallbacks`; in
  strict mode the first fault returns `mloc_errorFormat` and nothing is
  written. A function's options are each resolved first, one that fails
  a bad option; its resolve entry gets the operand (none, text, an
  argument, another function's 64-byte value, or a fallback it may
  refuse) and reads the options and locale through
  `mlocGetMessageOption`, `mlocFindMessageOption` and
  `mlocGetMessageLocale` while the call lasts; a fault it returns makes
  the expression fall back. Selection follows the specification: a
  selector that cannot select (no match entry, a fallback) or whose
  match fails is a bad selector matching only `*`; of the variants whose
  keys all match, the best by key over `*` and the better entry, at the
  first selector whose keys differ, the first of those that tie.
  `:string` takes text and number arguments, matches keys in NFC, and
  over a failed operand matches nothing without a fault of its own.
  Until the number and date functions come (the next slices), they are
  unknown, and a number argument without a function is written as its
  plain decimal.
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
- Formatting: every case of the suite without bidi isolation or the
  default number and date functions formats to its text with its errors
  in order (149, the `:test:` functions defined in the test as the
  suite describes them); `test_message_functions` checks the function
  table's refusals, each operand kind, the accessors, a function's
  faults in resolve and format, `:string`'s operands, selection with and
  without better entries, the selector limit, and the resolution limit
  at bind and in a call whose function reads an option 40000 times. `fuzz_message` binds
  and formats each text with two arguments and two functions.

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
