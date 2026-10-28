# Messages and catalogs

MessageFormat 2 compiled, bound and formatted; lists; catalogs and their chains; the catalog tool. Sources: `src/message*.c`, `src/list*.c`, `src/catalog*.c`, `tools/mloccat/`.

## Three steps

A message is compiled once, bound once per locale and formatted many times, each into caller memory:

- **Compile** (`message_parse.c`, `message_check.c`): the whole of UTS #35 Part 9's syntax and data model, written into a locale-free, position-independent, little-endian form read byte by byte at any alignment. Names and keys are stored in NFC, so a variable is one variable however it is composed. Parsing runs twice over the text, counting then writing, so nothing is allocated; the checks that compare names (duplicate declarations, options, variant keys) use a hash table placed past the message in the caller's buffer, which is why the capacity asked may exceed the size written. A name whose NFC form is no name (U+037E becomes ";") is a syntax error at that code point, so a compiled message always writes back.
- **Bind** (`message_bind.c`, `message_prepare.c`): the compiled message is read as untrusted bytes, as a catalog's would be (versions, counts against the size, every record's indexes and strings), each function is found, and each chain of declarations is measured. Number functions whose options do not come from arguments get their formatter and plural rules made once, by resolving the expression with probe arguments; a format call uses the prepared formatter only when its options are byte for byte the prepared ones, so correctness never rests on the probe.
- **Format** (`message_format.c`, `message_resolve.c`): each placeholder and selector is resolved where it stands, a local variable through its declaration on the stack (call by name), so a call needs no memory of its own.

Call by name resolves a local again at each reference, so declarations that each name the next twice grow as a power of two. Binding refuses a message whose format would resolve more than 65,536 expressions, a chain of more than 64 declarations, or more than 32 selectors, with `mloc_errorInvalid`, since no buffer would cure them; a crafted message whose declaration names a later one, which could loop, is `mloc_errorFormat`.

## Functions and values

The default functions follow the specification and their formatters are the library's own: `:number` and `:integer` select by plural rules, `:currency`, `:unit` and the date and time functions take the argument kinds the library adds for them. The `mloc:` functions are the library's: `mloc:spellout`, `mloc:relative`, `mloc:duration`, `mloc:list`, and the grammar functions ([grammar.md](grammar.md)). A program's functions are found by a namespaced name (not `u:` or `mloc:`) with resolve, format, match and better entries, so they select and format like the built-in ones.

A fault writes the specification's fallback for the failed part and is reported with its expression, or in strict mode stops the call with `mloc_errorFallback`. Isolation follows the default bidi strategy; the message's own direction is its locale's. With parts, every literal text, placeholder, fallback and markup is reported with its range; markup writes nothing.

## Lists

List patterns are compiled in by style and width (14 KB in all) and joined as ICU joins them, with its contextual rules: Spanish "y" becomes "e" before an i sound and "o" becomes "u" before an o sound, and Hebrew's "ו" takes a dash before an item not in Hebrew script. Each pattern is validated to hold `{0}` and `{1}` once, and items are written left to right with no partial copy. The gender of a list of persons follows CLDR's `genderList.xml`, a few rules compiled in (`src/generated/list_genders.c`).

## Catalogs

A catalog is a data file holding one locale and two units: the keys and the messages, in compiled form (`src/catalog_tables.h`). Keys are found by a minimal perfect hash after PTHash's design (Pibiri and Trani): buckets of about four keys placed largest first, each with the first 16-bit pilot that places all its keys, a table 2% larger than the keys and places past the count remapped so indexes are dense. A lookup is a hash, a table read and one comparison. Opening validates every offset, every key at its own index and every message as readable, so a lookup checks nothing after that.

**One key order per set.** The tool builds the table from the source's keys, and every catalog of the set stores the same keys, table and order with a set id hashed from them. An index then means the same message in every catalog of the set, which is what lets a chain fall back by index, and the library refuses a chain of catalogs from different sets.

## Chains

A chain is up to eight catalogs of one set in fallback order, each with its locale's message formatter, and a cache in caller memory: a head, an 8-byte slot per key, and an arena for bound messages. A message is bound when it is first formatted, never on the stack (bound messages have no size bound), or all at once by `mlocBindCatalogChain`. While it still binds, a chain writes its cache and is used by one thread at a time; once bound, every slot is filled and format calls only read.

Every call writes some text and states why, so a missing translation is visible but never blank: the first catalog's message, a later one's, the source's when a translation does not bind, or the key when nothing else works. A message with fallbacks does not fall to another locale: its faults name a translator's mistake, and text in the right language with a fallback is better than another language.

## The catalog tool

The source syntax is the W3C Internationalization Working Group's draft for message resources, pinned by commit and grammar hash in `tools/resource-release.txt`. The draft may change, so the syntax lives only in `mloccat`; a change touches the tool, never the library or compiled catalogs. The reader follows the grammar rule by rule and enforces what its notes state, with two narrowings (escapes must name scalar values, identifier parts may not be empty).

**Stale translations** are found by a hash: `mloccat accept` records in each translation entry the XXH64 of the source message's canonical text (the message compiled and written back, so whitespace and quoting do not change it). An entry whose record differs from the source's hash now is stale; one without a record is unrecorded, which the report lists apart. PO and XLIFF exchange carry the same state (`fuzzy`, `initial`), and an import records the hash of the source the translator saw, so a source changed after export still shows stale.

PO and XLIFF are exchange formats only: one entry per key with the key as context, the MF2 text as the message. XLIFF splits each message into text and codes (placeholders, markup, syntax) over original data that holds their MF2 text, so a translation tool can move codes without breaking the message; the same split writes the pseudo-locales. What comes back from translators is untrusted text: the resource, PO and XLIFF readers and the reading of selection messages each have a fuzz target (`fuzz_resource`, `fuzz_po`, `fuzz_xliff`, `fuzz_pgs`). `import-xliff` reads through `XliffImport`, which takes the document in memory, so its target runs the same code.
