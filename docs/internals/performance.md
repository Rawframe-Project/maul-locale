# Performance

Speed is part of the product: a locale layer is called for every number, date and string a program shows or sorts. This page states the bar, the design rules that serve it, and how speed is measured.

## The bar

Each part is benchmarked against ICU4C and ICU4X on the same inputs, and the target is to be no slower than the faster of the two at the same work, with no more code and data than ICU4X's for the same locales. ICU4X formats numbers and dates several times faster than ICU4C and builds formatters an order of magnitude faster; ICU4C compares strings several times faster than ICU4X through its fast Latin table. The bar takes the better of each. Where a part does not meet it yet, the gap is an open `performance` issue, not a remark in the code.

Timings are never a CI gate: wall time is not repeatable on shared runners. The bar is checked on a quiet machine before a release: a part meets it when its time per call is within 10% of the faster baseline's, both measured back to back as [Measuring](#measuring) says.

## Design rules that serve it

- **Work moves to the generator.** Pattern matching, rule compilation, collation tailoring and zone compilation run once at build time; the library only selects ([generator.md](generator.md#compilers-in-the-generator)). ICU does much of this each time a formatter is made, which is why making a formatter here is cheap.
- **Make once, format many.** A formatter resolves its locale's chain, bounds its indexes and copies what it needs when it is made. Formatting reads only the formatter and the data, with no lookup, lock, allocation or system call.
- **Validated data is read with plain loads.** Validation runs once when a file opens; lookups trust it ([data.md](data.md#validation)).
- **Closed sets are code.** Plural rules compiled to C select in about 4 ns, against about 47 ns interpreted.
- **No buffering where re-reading is cheaper.** The collation compare and sort key writer read the text again per level instead of buffering elements; most compares end at the first level.

## Measuring

`bench/` holds plain C benchmarks with fixed inputs, built as `maul-locale_bench`. Each case prints nanoseconds per call, the best of five runs; `BENCH_ONLY` selects cases and `BENCH_SHOW` prints the texts written, so a speed change can be checked for a change of output. Given an earlier run's output, it prints each case's ratio to it.

The ICU4C and ICU4X baselines time the same inputs and the same work: where ICU splits a call's work between a constructor and a method, both are timed (the index build times `AlphabeticIndex`'s constructor and `addLabels` with `buildImmutableIndex`, the work `mlocBuildIndex` does). Their programs are in `tools/oracle/bench/` ([oracles](../operations/oracles.md#benchmarks) says how to build them). The machine is shared with other work, so:

- Compare against a baseline measured back to back on the same machine, never against an old figure.
- Measure on a quiet machine (load well below the core count); under load, a busy SMT sibling slows this library's code (about four instructions a cycle) more than ICU4X's.
- When the machine stays busy, compare instructions per call with `perf stat -e instructions:u` (the `tools/oracle/bench/count/` programs), which load does not change, and confirm with timings later.

A pull request meant to change speed gives the numbers before and after, measured this way.
