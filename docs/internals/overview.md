# Overview

Maul Locale is the locale layer of a text stack: identifiers, numbers, dates, time zones, messages, grammar, catalogs, collation and segmentation for every locale CLDR describes, from one CLDR release. This page states the library's profile within the [Maul conventions](maul-conventions.md), how its parts fit, and where data comes from. The other pages of this folder explain each part.

## Profile

- **Determinism: bit-exact.** Identical input gives identical output on every platform, compiler and architecture. A `double` given as input becomes its shortest decimal digits by integer arithmetic ([numbers.md](numbers.md#decimals)); unit conversion uses exact fractions; a `double` returned (a Temporal duration's fields and totals) is the nearest to an exact value, its bits assembled by integer arithmetic. Nothing else touches floating point, and the library calls no `<math.h>` function.
- **Threads: none started.** Every function runs on the caller's thread. Data and every object made from it are read-only once made, so any function is safe from any thread. The one object that writes after it is made is a catalog chain that still binds lazily ([messages-and-catalogs.md](messages-and-catalogs.md#chains)).
- **Owner objects: none.** Nothing is allocated and there is no root object. Formatters, collators, inflectors and bound messages are fixed-size structs or caller buffers with nothing to destroy, so the family's def, cookie and ABI stamp rules have nothing to apply to.
- **Stored formats.** Data files are generated and open only with a library of their data version. Catalogs and compiled messages are the asset formats ([data.md](data.md#asset-formats)): each opens with any library of its own format version. The library has no state formats: an opaque struct, a bound message, an alphabetic index and a catalog chain's cache live in the memory of the process that made them and are never stored.
- **Callbacks.** A part function (`mlocOutput.part`) or a program's message function may call any function of the library, the call it is inside of included, since every object is read-only and a call keeps its state on its stack. It must not make again an object the call is using, or write the output or faults the call is writing.
- **Operating system: none.** No file, clock, environment variable, time zone or locale of the process is read. The caller passes the data, the instant, the zone and the user's preferred languages (which a windowing library such as Maul Window reports).
- **Dependencies:** the C standard library's `<stddef.h>`, `<stdint.h>`, `<stdbool.h>` and `<string.h>`, `snprintf` for a few integers (which no locale setting changes), and [Maul Unicode](https://github.com/Rawframe-Project/maul-unicode) for UTF-8 decoding, normalization, case folding, combining classes and General_Category. No Unicode table is duplicated here.

## Parts

The library is one static or shared library over these parts; `tools/modules.txt` holds their internal dependencies, which point one way:

| Part | Public header | Built on |
|---|---|---|
| data | `data.h` | |
| locale | `locale.h`, `names.h` | data |
| decimal, plural | `decimal.h`, `plural.h` | data, locale |
| number | `number.h`, `currency.h`, `unit.h`, `spellout.h` | decimal, plural |
| list | `list.h` | locale |
| calendar, zone | `calendar.h`, `zone.h`, `ixdtf.h` | data |
| datetime | `datetime.h`, `relative.h`, `duration.h` | calendar, zone, number |
| message | `message.h` | number, datetime, list, grammar |
| grammar | `grammar.h` | number, spellout |
| catalog | `catalog.h` | message |
| collate | `collate.h`, `search.h`, `index.h` | data, Maul Unicode's NFD |
| segment | `segment.h` | data, Maul Unicode's iterators |

Word segmentation by dictionaries, transliteration and person names are in CLDR's scope and not built yet.

## Where data comes from

```
CLDR 49 XML ─┐
IANA tzdb ───┼─ tools/mlocgen ─► src/generated/builtin_data.c  (compiled in)
FractionalUCA┘                ─► src/generated/*.mloc          (data files)
                              ─► src/generated/*.c, *.h        (code tables: plural rules, calendar years, ...)
Zemberek, Kotus, Unihan ─ tools/gen_*.py ─► src/generated/*.h  (grammar lexicons)
```

The generator ([generator.md](generator.md)) reads CLDR's XML release itself and resolves its inheritance; it depends on no ICU data. CLDR's JSON, ICU4C and ICU4X serve only as test oracles ([conformance.md](conformance.md)). Every generated file is committed, and CI regenerates them from the pinned sources and fails on any byte of difference.

Algorithms over a closed set are compiled into the library rather than interpreted from data: the plural rule sets are C functions, the calendars and grammar engines are code, and locale data refers to them by index. The data version ([data.md](data.md#versions)) includes a hash of those code tables, so a data file can never be read by a library whose code it does not match.

## What it never compromises on

- **Conformance.** Each part passes CLDR's conformance data for it and agrees with ICU4C (and ICU4X where it covers the part) on the same inputs. Where it departs, the departure is listed with its reason; there are no silent differences.
- **Hostile data and text.** Every data file is validated completely when it is opened, and every text is validated as it is read. No input makes the library read out of bounds, loop without bound or allocate.
- **Speed.** Formatters are made once and format many times with no lock, allocation or system call. Each part is benchmarked against ICU4C and ICU4X on the same inputs, and the target is to be no slower than the faster of the two ([performance.md](performance.md)).
