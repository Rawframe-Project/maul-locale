# mloc-0001. Library profile

Status: Accepted

## Context

Every Maul library states in one record what its domain adds to the
family rulebook (family record 0005). Maul Locale formats numbers,
dates, lists, names and messages, sorts text and segments words for
every locale CLDR describes, and it must be as fast as the fastest
existing implementation at each task while running anywhere a C
compiler does.

## Decision

- **Determinism:** bit-exact. Identical input gives identical output on
  every platform, compiler and architecture. A `double` given as input
  is converted to its shortest decimal digits by integer arithmetic;
  unit conversion uses exact rationals. A `double` returned (a Temporal
  duration's fields and totals) is the nearest to an exact value, its
  bits assembled by integer arithmetic (amended 2026-10-15, mloc-0041).
  Nothing else touches floating point.
- **Threads:** none. Every function runs on the caller's thread; data
  is immutable and every function is safe from any thread.
- **Callbacks:** a part function (`mlocOutput`) or a program function
  (`mlocMessageFunction`) may call any function of the library, the
  call it is inside of included (conventions section 10): every object
  is read only once it is made and a call keeps its state on its stack.
  It must not make again an object the call is using, or write the
  output or faults the call is writing (amended 2026-10-18).
- **Memory:** there are no owner objects and nothing is allocated. Data
  is compiled into the library or held by the caller as the bytes of a
  data file, which is validated once when opened. Formatters,
  collators, segmenters and compiled messages are fixed-size structs
  the caller declares, with nothing to destroy. Results go into caller
  buffers, and a buffer that is too small is reported with the size
  needed.
- **Operating system:** the library makes no operating system call: no
  file, clock, time zone or locale of the process is read. The caller
  passes everything, including the user's preferred languages, which
  Maul Window reports.
- **Platform dependencies:** none beyond the C standard library's
  `<stddef.h>`, `<stdint.h>` and `<string.h>`, and Maul Unicode.
- **Commit areas:** `api`, `bench`, `build`, `calendar`, `catalog`,
  `ci`, `collate`, `data`, `datetime`, `decimal`, `docs`, `generator`,
  `grammar`, `list`, `locale`, `message`, `names`, `number`, `person`,
  `plural`, `segment`, `spellout`, `tables`, `tests`, `tools`,
  `transform`, `zone`.

## Consequences

Consumers call any function from any thread with no setup and no
cleanup, results do not depend on where the program runs, and the
caller decides where every byte lives.
