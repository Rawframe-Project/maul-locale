# Catalogs

A catalog holds a program's messages for one locale. The program's authors write a source catalog, translators answer it with one catalog per locale, the `mloccat` tool compiles them, and the program formats messages by key through a chain of catalogs in fallback order. `maul-locale/catalog.h` has the runtime side.

## Source files

Catalogs are written in the W3C Internationalization Working Group's draft message resource syntax: a frontmatter with the locale, optional `[section]` heads, `key = message` entries whose MF2 messages continue on indented lines, `#` comments and `@property` metadata attached to the entry that follows.

```
@locale en
---
# Shown in the toolbar.
@param $count the number of files selected
files.count =
  .input {$count :integer}
  .match $count
  one {{{$count} file}}
  *   {{{$count} files}}

[dialog]
@mloc-max-length 24
save = Save changes?
```

A translation has the same keys with its own `@locale`. The properties the tool reads are `@locale`, `@param`, `@translate no` (or `@do-not-translate`) for text that stays as the source writes it, `@obsolete`, `@allow-empty`, `@mloc-max-length` and `@mloc-example` for the length check, and `@mloc-source`, which the tool writes. Others are kept and ignored. The draft may change; a change touches the tool, never compiled catalogs or programs.

A compiled catalog names nothing in the library's [data](data.md) by index or version, so it keeps working when a program updates its data or the library to a release of another CLDR or tzdb. Before 1.0.0, a release that changes the catalog format refuses an older catalog with `mloc_errorVersion`; compile the set again with that release's `mloccat`.

## The tool

`mloccat` builds with the library (`MAUL_LOCALE_BUILD_TOOLS`).

| Command | Does |
|---|---|
| `mloccat check FILE` | Checks a source or translation: a locale the library parses, unique keys, every message compiles. |
| `mloccat compile DIR SOURCE [TRANSLATION...]` | Compiles a set into `DIR/<locale>.mlct`, one catalog per locale. |
| `mloccat accept SOURCE TRANSLATION [KEY...]` | Records in the translation which source message each entry answers, so a later change of the source shows the entry stale. |
| `mloccat report [--json] [--fail-on KINDS] SOURCE [TRANSLATION...]` | Coverage and findings per locale: `missing`, `stale`, `unrecorded`, `orphaned`, `differs`, `long`. With `--fail-on`, exits 1 when one of those kinds is found, for CI. |
| `mloccat export-po` / `import-po SOURCE TRANSLATION PO` | A translation to and from a gettext PO file, one entry per key with the key as `msgctxt`; a stale entry is `fuzzy`. |
| `mloccat export-xliff` / `import-xliff SOURCE TRANSLATION XLIFF` | The same with XLIFF 2.2: placeholders and markup as codes, the length limit as a size restriction. |
| `mloccat pseudo DIR SOURCE` | Writes the pseudo-locales `en-XA` (accented, bracketed and padded text) and `ar-XB` (mirrored text) to compile with the set, to test layouts before translation. |
| `mloccat read FILE` | Writes a file's data model as JSON. |

Compile a source with all its translations in one call: the catalogs of one set share one key order, so a key's index means the same message in each, and the library refuses a chain that mixes sets. A translation that is stale still compiles; `report` lists it.

## Formatting from catalogs

A program opens each catalog file's bytes with `mlocOpenCatalog`, which checks them all, makes a [message formatter](messages.md) for each catalog's locale, and puts them in a chain in fallback order, the source last. The chain binds each message once into a cache of the caller's memory, on first use or all at once with `mlocBindCatalogChain`; `mlocMeasureCatalogChain` gives the cache size that holds them all.

```c
// catalogs: fr-CA, fr and the source en, opened in that order;
// formatters: a message formatter of each catalog's locale
size_t cacheSize = 0;
mlocCatalogChain chain;
mlocResult measured = mlocMeasureCatalogChain(catalogs, formatters, 3, &cacheSize);
void* cache = measured == mloc_success ? malloc(cacheSize) : nullptr;
if (cache != nullptr &&
    mlocInitCatalogChain(&chain, catalogs, formatters, 3, cache, cacheSize) == mloc_success &&
    mlocBindCatalogChain(&chain) == mloc_success)
{
    mlocMessageArgument count = {
        "count", 5, {.kind = mloc_messageValueInteger, .as.integer = 2}};
    char text[128];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocCatalogResult how;
    mlocResult formatted =
        mlocFormatCatalogKey(&chain, "files.count", 11, &count, 1, &output, nullptr, &how);
    // formatted is mloc_success; text holds "2 fichiers" from fr,
    // how.reason is mloc_catalogFallback: fr-CA has no translation of its own
    (void)formatted;
}
```

Every call writes something and says why: the first catalog's message, a later one's where earlier ones lack it, the source's where a translation does not bind, or the key itself when the set has no such key or nothing else works. A program can log the reasons it did not expect. `mlocFindCatalogKey` turns a key into an index once, for `mlocFormatCatalogMessage`, which then skips the lookup.

A chain that still binds writes its cache, so it is used by one thread at a time. Once `mlocBindCatalogChain` has succeeded, any number of threads may format with it at once.
