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
  A name whose NFC form is no name is a syntax error at the code point
  that breaks it (U+037E and U+1FEF, which NFC makes ";" and "`"; a
  composition never breaks a name, checked over every canonical
  composite), so a compiled message always writes back. A CI fuzz run
  aborted on a compiled message the writer refused; its input was not
  kept, and this is the one such case found by review (the job now
  keeps crash inputs).
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
  the formatter and is read-only. Binding also prepares each number
  function's expression: it resolves it once with probe arguments (an
  argument stands for a plain number as an operand; an option's value
  from an argument, or a program function reached, leaves the
  expression unprepared, and no program function is called), and keeps
  the value's options with its number or currency formatter and the
  plural rules its selection reads, 8-aligned for the buffer bound into.
  A call resolves as before and uses them only when its value's options
  are the prepared ones byte for byte, reading them in place (a bound
  message moved to an address that leaves them unaligned makes its own,
  as a copy would take a unit formatter's 3.5 KB of stack); so
  correctness never rests on the probe. A bound message keeps room for
  every number function's expression (a unit formatter's for `:unit`, a
  currency formatter's for `:currency` and `:offset`), so its size does
  not depend on the probe or the buffer's address.
  `mlocFormatMessage` resolves each placeholder and selector where it
  stands, a local through its declaration on the stack (call-by-name: no
  memory per call; a level of a chain holds its operand alone, the
  functions' work off the recursion, so a chain of 64 takes about 24 KB
  and the longest chain through options binding allows, 14, about
  26 KB), an argument by its name after NFC; it writes text,
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
- **Bidi isolation and the `u:` options:** a formatter isolates by the
  Default Bidi Strategy unless `mloc_messageIsolationNone` turns it off,
  the message's direction its locale's (unknown when the locale has
  none). Each value carries a direction and an isolate flag: literals,
  arguments and `:string` unknown, a program function's from its
  optional `direction` entry, a valid `u:dir` replacing it and
  isolating (`inherit` keeping it unisolated), an unannotated variable
  passing its declaration's on; a fallback is unknown, so in FSI and
  PDI, also where a value failed to format (what it wrote is taken
  back). `u:` options are the formatter's, hidden from a function's
  accessors; a `u:dir` or `u:id` value that is not one is a Bad Option
  and ignored, other `u:` options ignored. Markup resolves its options,
  reporting their faults, `u:dir` on it a Bad Option; binding counts
  those resolutions too. ICU4C 79 writes fallbacks bare and reports no
  bad `u:dir` value; the library follows the specification there.
- **`:number` and `:integer`:** their operands are number literals (and
  string arguments that are one), numeric arguments and number values;
  their options ECMA-402's, with ECMA-402's defaults where the
  specification leaves them open (`:number` 0 to 3 fraction digits,
  `:integer` the value rounded half away from zero, as `:number`
  rounds); a value outside an option's range, or a maximum under its
  minimum, is a Bad Option and ignored. A number value passes its
  options on (`:integer` without its fraction digits); a `select` from a
  variable or an operand is a Bad Option and the value cannot select.
  Match compares a number-literal key with the value rounded as it
  formats, written plainly (`1e3` matches `1000`, zero unsigned), and a
  keyword with its plural or ordinal category; another key is a Bad
  Variant Key (a program's match entry may return that fault too,
  without breaking the selector), and a literal beats a keyword. A
  number reads in its locale's direction; a program function receives
  a number value as `mloc_messageOperandNumber`, its decimal unrounded.
  ICU4C 79 differs on the defaults (6 digits, half-even), on `1e3`, and
  fails the call on digit sizes past its limits. Call-by-name resolves a
  declaration at each reference; its faults are reported the first
  time, a bit a declaration on the stack, so a bound message has at
  most 1,024 declarations.
- **`:offset` and `:percent`:** `:offset` takes exactly one of `add`
  and `subtract`, a digit size (none, both or a bad value is a Bad
  Option and a fallback), ignores other options, and keeps its
  operand's function and options; its value is the exact sum, and a sum
  past a decimal's 40 digits is an Unsupported Operation and a
  fallback. `:percent` drops its operand's minimum integer digits,
  increment and `select` without a fault, shows 0 fraction digits by
  default, and formats and selects (plural categories and exact keys)
  on its value times 100, the value itself kept for a later function.
- **`:currency`:** its operand is a currency amount (an argument kind of
  its own, a decimal and an ISO 4217 code, as ICU4J's `CurrencyAmount`),
  a `:currency` value, or a numeric operand with the `currency` option
  (three ASCII letters, any case; well-formed, not checked as known).
  Without a currency it is a Bad Operand; the option over an amount's
  or a value's currency is a Bad Option and the operand's stands. Its
  options are the specification's list: `currencyDisplay` maps onto
  `mlocCurrencyDisplay` (`never` is hidden), `currencySign` onto
  `mlocCurrencyOptions`' new `sign` (its zero value the locale's `-u-cf`
  choice), `fractionDigits` `auto` or a digit size; with no digit
  option the currency formatter rounds by the currency's own digits and
  increment. It drops an operand's `select` and fraction digits without
  a fault and passes the rest on; it does not select (a Bad Selector).
  The number functions read an amount or a currency value as its
  number, `:offset` keeping the currency; written with no function an
  amount is its digits and code ("42.5 EUR", as ICU4J writes one), and
  a program function receives a currency value as
  `mloc_messageOperandCurrency`.
- **`:unit` (Draft):** its operand is a unit amount (an argument kind
  of its own, a decimal and a unit identifier, as ICU4J's `Measure`), a
  `:unit` value, or a numeric operand with the `unit` option; without a
  unit it is a Bad Operand, the option over an amount's or a value's
  unit a Bad Option. A unit or usage that is not lexically an
  identifier (lower-case letters and digits joined by single hyphens)
  is a Bad Option; a well-formed unit the data cannot name, or data
  without `units.mloc`, is an Unsupported Operation when the value is
  written. Its options are the specification's list, `unitDisplay`
  mapping onto `mlocUnitWidth`, ECMA-402's 0 to 3 fraction digits, and
  `usage` converting by the unit part's preferences, whose rounding
  stands unless a digit option is given; a usage the unit's quantity
  lacks takes that quantity's default, as the unit part and ICU's
  NumberFormatter do, where the specification suggests (SHOULD) an
  Unsupported Operation. It drops an operand's `select` and does not
  select. The number functions read an amount or a `:unit` value as its
  number, `:offset` keeping the unit; an amount written with no
  function is its digits and unit ("3.5 kilometer"), and a program
  function receives a `:unit` value as `mloc_messageOperandUnit`. ICU4C
  79 has no `:unit`.
- **`:date`, `:time` and `:datetime` (Draft):** the specification's
  options over the library's date, time and date-time formatters:
  `fields`/`dateFields` onto `mlocDateFieldSet`, `length`/`dateLength`
  onto `mlocDateLength` (medium by default), `precision`/`timePrecision`
  onto `mlocTimePrecision` (the minute by default), `hour12` the
  locale's own 12- or 24-hour clock, `calendar` the locale's `-u-ca`;
  the date and time joined by the "at" glue. Options other than the
  overrides (`hour12`, `calendar`, `timeZone`) set by a variable are
  Bad Options and ignored. Operands: date/time literals and string
  arguments, read by `mlocParseTemporalString` (date-times, instants and
  zoned date-times with a year; an impossible day, a time alone, a zone
  the data lacks, an offset the zone did not have, an offset of seconds
  with no zone, or a calendar annotation other than `iso8601` a Bad
  Operand), new date, time, date-time, instant and zoned date-time
  argument kinds, and another date function's value, whose override
  options alone pass on. `:date` and `:datetime` need a date; `:time`
  over a date alone writes its midnight.
- **Zones:** a literal with Z, an offset or a zone annotation, an
  instant (its zone UTC's) and a zoned date-time argument are exact
  times; the rest are floating. `timeZone` takes an identifier or offset
  the data has (`UTC` among them) or `input`, the operand's own zone
  (over a floating value a Bad Operand, the default used);
  `mlocMessageOptions.timeZone` is the default zone, and with none an
  exact time shows in UTC. An exact time is shown at its instant in that
  zone; a floating one as it is, read in the zone only to name it.
  `timeZoneStyle` (`:time`, `:datetime`) long and short are the zone's
  specific names ("Eastern Standard Time", "EST"), named at the instant;
  over a floating value with no zone given and no default, or a time
  alone, it is a Bad Option and the zone is left out. The zone names
  are a data file of their own (`zone-names.mloc`).
- **Date values** do not select, read in their locale's direction, and
  are prepared at bind (the probe gives date functions an instant; the
  zone shown is not part of the formatter, so a timeZone literal is
  prepared too); with no function a date argument is written in ISO
  8601 (an instant with Z, a zoned date-time with its offset and zone in
  brackets, as Temporal writes them), and a program function receives a
  date value as `mloc_messageOperandDate`, `Time` or `DateTime`, an
  exact one with its instant and zone (the timeZone option's, else its
  operand's) and its date-time there. The dates are a data file of
  their own (`dates.mloc`); without it a date function is an Unsupported
  Operation. ICU4C 79's MF2 is of the older Draft (it ignores these
  options) and no oracle.
- **`mloc:spellout` and `mloc:relative`:** number values of their own
  functions. `mloc:spellout` writes the spell-out rules' text of the
  exact number: `rules` names a public rule set without its "%"
  (`spellout-ordinal`, `digits-ordinal`; MF1's `spellout` and `ordinal`),
  `spellout-numbering` by default as MF1, or `numberingSystem` an
  algorithmic system ("roman"), the later of the two standing; a rule
  set or system the locale lacks is an Unsupported Operation and a
  fallback (MF1 silently writes its default). `mloc:relative` writes an
  amount of a `unit` (year to second, the weekdays; none is a Bad
  Operand) relative to now, with `style` (long, short, narrow),
  `numeric` (always, auto) and `:number`'s digit and grouping options
  (0 to 3 fraction digits by default), no sign display. Their options
  that choose a formatter are literals only; both are prepared at bind,
  neither selects, number functions read their numbers, and they need
  `spellout.mloc` and `relative.mloc`.
- **`mloc:duration`:** a duration value: a new argument kind
  (`mloc_messageValueDuration`, `mlocDurationValues` by pointer, checked
  by `mlocMakeDuration`), an ISO 8601 duration literal or string
  (`mlocParseDuration`, as written: PT90M stays 90 minutes), a number of
  seconds (MF1's operand) balanced up to hours with its fraction
  truncated to nanoseconds, or another `mloc:duration` value, whose
  options pass on. ECMA-402's DurationFormat options: `style` (short,
  long, narrow, digital), each field's style (years to days long, short
  or narrow; hours to seconds also numeric and 2-digit; the smaller ones
  also numeric) and `...Display` (auto, always), `fractionalDigits` (0
  to 9); literals only; options ECMA-402 refuses together are an
  Unsupported Operation, found when the formatter is made. Prepared at
  bind (4 KB of room where one stands), it does not select, number
  functions refuse it, a bare duration is written as Temporal writes it
  ("P2DT3H"), program functions receive `mloc_messageOperandDuration`
  (`mlocMessageResolved` grew to 96 bytes to hold its fields), and it
  needs `units.mloc`.
- **`mloc:list`:** a list value: a new argument kind
  (`mloc_messageValueList`, the items' texts as `mlocListItem` by
  pointer and their count; the caller formats each item, as a list
  formatter takes texts), or another `mloc:list` value, whose options
  pass on. ECMA-402's ListFormat options: `type` (conjunction,
  disjunction, unit) and `style` (long, short, narrow), literals only.
  It selects by its count as `:integer` does: a number literal key
  matches the count written plainly ("2", not "2.0" or "2e0"), a plural
  keyword the count's cardinal category, a literal beating a keyword;
  another key is a Bad Variant Key. Prepared at bind (the formatter and
  the cardinal rules), its parts are the list formatter's items and
  literals, number functions refuse it, a bare list is an Unsupported
  Operation (no text stands for it), program functions receive
  `mloc_messageOperandList`, and its data is built in.
- **Parts:** with the output's part function set, a format call that
  succeeds (or falls back) writes the text a second time to report its
  parts, storing nothing: text, placeholder, fallback, markup open,
  standalone and close (no length, at their place) and isolation marks,
  `mlocMessagePart` 0x80 to 0x86 in a kind's low byte with the
  expression's or markup's index above it; they tile the text in order.
  A placeholder's range comes first (its value measured for it), then
  its function's own parts inside it, their kinds the function's
  (`mlocNumberPart`, `mlocDatePart`; a program function's passed on
  unless the low byte is a message part's). Binding refuses more than
  2^24 expressions or markups, so an index fits. Program functions must
  give the same text each time: their format entry is called once more
  to measure and once more to report.
  `mlocGetMessagePartInfo`, `mlocGetMessagePartParameter` and
  `mlocFindMessagePartOption` read what a placeholder's, fallback's or
  markup's part names from the bound message, as written: the function
  or markup name, the options (a literal's text or a variable's name)
  and attributes, `u:id` found by name.
- **A locale's direction,** which the message's own isolation needs,
  is public: `mlocGetLocaleDirection` takes the locale's script, or its
  likely script, and finds it among the scripts CLDR's
  `scriptMetadata.txt` marks right to left, a table the generator
  writes (`src/generated/script_directions.c`). For every locale of
  CLDR's `common/main` this is the character order the locale inherits
  (Mongolian's vertical order being left to right in a line); CI checks
  all of them (`tools/cldr_directions.py`, `conform_direction`).
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
- Formatting: every case of the suite the functions built so far reach
  formats to its text with its errors in order, or, where the suite
  gives no text, with its errors (299, isolated as each case says, the
  `:test:` functions defined in the test as the suite describes them);
  `test_message_words` checks the `mloc:` functions, the two over
  numbers against ICU4C 79's MF1 texts and the formatters', durations
  and lists against their formatters', a list's selection by count and
  category, their refusals, operands, parts and preparation; `test_message_parts` checks parts of every piece,
  isolation marks,
  numbers' and dates' parts at their place, a program function's,
  none on failure, faults counted once, tiling, and the accessors;
  `test_message_dates` checks the date functions' options, operands,
  refusals, zones, inheritance and preparation, with `dates.mloc`,
  `zone-names.mloc` and the locale names in the set;
  `test_message_numbers` checks `:number` and `:integer`'s defaults,
  options and their refusals, operands, inheritance and selection,
  `:offset`'s sums, refusals and overflow, `:percent`'s scaling,
  dropped options and selection, `:currency`'s operands, options,
  displays, signs, inheritance, amounts and refusals, and `:unit`'s
  widths, usage, refusals, inheritance, amounts and data, with
  `units.mloc` in the set; `test_message_functions` checks the function
  table's refusals, each operand kind, the accessors, a function's
  faults in resolve and format, `:string`'s operands, selection with and
  without better entries, the selector limit, and the resolution limit
  at bind and in a call whose function reads an option 40000 times;
  isolation in left-to-right, right-to-left and unknown messages with
  every `u:dir` value, a fallback's isolate replacing a value's, and
  markup's options. `fuzz_message` binds
  and formats each text with two arguments and two functions.
- **Speed:** the benchmark's `message-*` cases format a message of each
  function family, the plural message with its parts, a match naming
  six functions it does not reach, and the hello message on a formatter
  of 64 program functions; ICU4C 79 runs the same messages. Against its
  MessageFormat 2 every message is 25 to 200 times faster; against its
  MessageFormat 1 every family it can say is faster or level (currency
  ten times, dates two to three times, spell-out level) but
  `mloc:duration`, 1.4 times (its formatter writes each field as a
  decimal); relative times and lists are level with ICU's own
  formatters. A call does no work for a function it does not reach or
  a program function the formatter holds.

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
