# Data

Every function that needs locale data takes an `mlocDataSet`: a list of opened sources searched in order. A source is the data compiled into the library or a data file the program holds in memory. The library never reads a file itself. A message formatter, a zone formatter, and a date, time or date-time formatter made with a zone keep a pointer to the set, so the set and its array of sources outlive them and stay as they were.

## What is built in

The compiled-in data, about 460 KB, holds what most programs need for every locale CLDR has:

- locale identifiers: aliases, likely subtags, parent locales, the language matching table, regional preferences and capitalization contexts;
- plural rules;
- numbers, percents, compact notation and currencies (symbols, digits, the currencies of each region), and number parsing;
- unit identifiers, conversions and the units each region prefers;
- list patterns;
- calendars and the time zones of the pinned IANA release, with their BCP 47 ids and Windows names.

`mlocOpenBuiltinData` opens it; the call cannot fail.

## Data files

The larger data comes in files a program adds when it uses that part. They are in `src/generated/` and attached to each release.

| File | Size | Needed for |
|---|---|---|
| `dates.mloc` | 1.3 MB | Formatting dates and times, date ranges, relative times with a time of day. |
| `zone-names.mloc` | 2.9 MB | Time zone names in formatted dates, times and zones. |
| `names.mloc` | 4.3 MB | Display names of languages, scripts, regions, variants and keys; whole-locale names; the location form of zone names. |
| `subdivisions.mloc` | 0.1 MB | Display names of subdivisions (`-u-sd`, `-u-rg`). |
| `currency-names.mloc` | 2.1 MB | Currency names ("US dollars"), as formatted and as parsed. |
| `units.mloc` | 3.6 MB | Formatting units and durations, unit display names and genders. |
| `spellout.mloc` | 1.1 MB | Numbers in words, ordinal suffixes, algorithmic numbering systems, and how numbers are spoken for [grammar](grammar.md). |
| `relative.mloc` | 0.6 MB | Relative times ("in 3 days", "yesterday"). |
| `collation.mloc` | 2.5 MB | [Collation](collation.md), search and alphabetic indexes. |
| `segments.mloc` | 12 KB | [Segmentation](segmentation.md): word, sentence and line breaking as a language tailors it: Greek sentences, POSIX English words, Chinese and Japanese lines and Japanese words, and the abbreviations after which a sentence goes on. |

A call that needs data its set does not have returns `mloc_errorNotFound`. A file holds its part for every locale. When two sources have the same part, the one earlier in the set answers.

## Opening a file

A program reads the file into memory by its own means (a file, an asset bundle, a download) and opens it. The bytes must be 8-byte aligned, which any allocator's memory is, and must outlive the data and everything made from it, and not change. They are validated once, when opened: bytes changed afterwards may be read outside the file or give any answer, and a build with assertions on may stop with a trap. To replace a file, open the new bytes, make what you need from them, and only then free the old ones.

```c
mlocData sources[2];
mlocDataSet set = {sources, 2};
if (mlocOpenData(bytes, size, &sources[0]) == mloc_success)
{
    mlocOpenBuiltinData(&sources[1]);
    // set finds what the file has in it first, the rest built in
}
```

`mlocOpenData` validates the whole file once: a file that opened is never checked again, so a damaged or hostile file is refused here with `mloc_errorFormat` instead of being read later. `mlocData` is a small value that holds no resource; nothing closes it.

## Versions

A library opens only data of its own data version: the CLDR and Unicode versions, the data format version (`MLOC_DATA_FORMAT_VERSION`), the time zone release, and a hash of the code tables must all match, or `mlocOpenData` returns `mloc_errorVersion`. The data format version changes only when the format does, not with every library release, so data files and catalogs stay valid across releases that keep it. `mlocReadDataVersion` reads a file's version from its first 64 bytes, so a program can tell which file to fetch before opening one, and `mlocGetDataVersion` gives the version the library reads. A program that downloads data files stores them per data version.

## A newer time zone release

Time zone rules change several times a year. `mlocgen zonesfile <tzdb directory> <CLDR common directory> zones.mloc` compiles a zones file from any IANA release with a generator of the same data version; placed before the compiled-in data, it gives that release's rules to every zone lookup of the set. A zones file is the one kind of data whose release may differ from the library's. `mlocGetTimeZoneRelease` names the release a set uses.

## Which locales a source has

`mlocData_GetLocaleCount` and `mlocData_GetLocale` list the locales a source has data of their own for, as CLDR names them (597 in the compiled-in data, 219 in `units.mloc`); the others inherit theirs. Whether a locale is supported is a question for [language matching](locales.md#matching-a-users-languages): every lookup falls back along the locale's chain to root, so any well-formed locale gets an answer.
