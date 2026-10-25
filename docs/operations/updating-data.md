# Updating data

Every table in `src/generated/` comes from a pinned source by a generator, and CI regenerates it and fails on any difference. This page says how to regenerate each and how to move a pin. A change of data is a change of results: the pull request says what changes for callers.

## Contents

- [Pins](#pins)
- [Regenerating src/generated](#regenerating-srcgenerated)
- [A new CLDR release](#a-new-cldr-release)
- [A new tzdb release](#a-new-tzdb-release)
- [Grammar lexicons](#grammar-lexicons)
- [Other generated files](#other-generated-files)
- [External data](#external-data)

## Pins

| Source | Pin | Used by |
|---|---|---|
| CLDR's XML and JSON | `tools/cldr-release.txt` (tag, commit, JSON release and its SHA-256, Unicode version) | `mlocgen`, the resolution check |
| IANA tzdb | `tools/tzdb-release.txt`; `tools/tzdb-older.txt` for the zones file check | `mlocgen` |
| Unihan | `tools/unihan-release.txt` (Unicode version, SHA-256 of `Unihan.zip`) | `tools/gen_hanja_finals.py` |
| Zemberek-NLP, Kotus, ICU's matcher test | the README of each folder in `tools/external/` | `tools/gen_turkish_marks.py`, `tools/gen_finnish_types.py`, `conform_match` |
| Message resource draft | `tools/resource-release.txt` | the catalog tool's reader |
| Maul Unicode | `MAUL_LOCALE_UNICODE_VERSION` in `CMakeLists.txt` | the build |

`tools/fetch_cldr.sh CACHE` and `tools/fetch_tzdb.sh CACHE [older]` fetch the CLDR and tzdb pins into a cache directory outside the repository and check them.

## Regenerating src/generated

```sh
tools/fetch_cldr.sh ~/cldr-cache
tools/fetch_tzdb.sh ~/tzdb-cache
cmake --build out/dev --target mlocgen
tools/regenerate.sh out/dev/mlocgen ~/cldr-cache/cldr/common ~/tzdb-cache/tzdb-$(awk '$1 == "release" { print $2 }' tools/tzdb-release.txt)
```

`tools/regenerate.sh` removes mlocgen's old files from `src/generated/` and runs `mlocgen generate`, so a file the generator no longer writes shows as deleted instead of staying behind; the tables of the `tools/gen_*.py` scripts stay. mlocgen writes the compiled-in data, every data file and the code tables, then the data version (`src/generated/data_version.h`) from the pins and the code tables' hash. Its output depends only on its inputs, so a run with unchanged pins changes nothing.

## A new CLDR release

1. Move `tools/cldr-release.txt` to the release's tag and commit, the JSON release and its SHA-256, and the Unicode version it is built on. A CLDR built on a new Unicode version also needs the Maul Unicode release of that version.
2. Set `MLOC_CLDR_VERSION_*` in `include/maul-locale/base.h`.
3. Regenerate. The generator refuses data it does not understand (a new DTD element in a path it reads, a rule syntax it does not take, a set pattern with a property) with the file and the reason; extend the generator rather than skipping it.
4. Run the data checks ([development.md](development.md#data-checks)). Each check fails on a new difference and on a listed one that went away; read each, then fix the generator or list the difference with its reason in its `tools/*-exceptions.txt`.
5. Rerun the oracles of the parts whose data changed ([oracles.md](oracles.md)) against the ICU release built on the same CLDR, and move `tools/external/icu/` to that release's matcher test.
6. Regenerate `test/mf2_suite.inc` from the release's MessageFormat 2 tests: `python3 tools/gen_mf2_suite.py ~/cldr-cache/cldr <short commit> > test/mf2_suite.inc`.

A new CLDR release is a new minor version of the library: formatted text changes.

## A new tzdb release

1. Move `tools/tzdb-release.txt` to the release and the SHA-256 of its data archive, after checking the archive's signature against the tzdb maintainers' key.
2. Regenerate, and run the zone checks: offsets against `zic` and `zdump`, local times against `zoneinfo`, zoned arithmetic, and the BCP 47 and Windows mappings.

A new tzdb release is a new minor version of the library: the local time of existing instants changes.

Programs need not wait for a library release to take new rules: `mlocgen zonesfile <tzdb directory> <CLDR common directory> zones.mloc` writes a zones file for the current data version from any release ([data.md](../user/data.md#a-newer-time-zone-release)). `tools/tzdb-older.txt` names the release the CI check compiles such a file from.

## Grammar lexicons

```sh
python3 tools/gen_turkish_marks.py      # from tools/external/zemberek
python3 tools/gen_finnish_types.py      # from tools/external/kotus
python3 tools/gen_hanja_finals.py DIR   # DIR holds Unihan.zip at the pinned version
```

Each checks its sources' SHA-256 first and, with `--check`, fails when the generated header differs instead of writing it. To move Zemberek or Kotus to a new version, replace the folder's files with the new release's, exactly as published, and update its README (source, version, SHA-256); to move Unihan, update `tools/unihan-release.txt` with the Unicode version Maul Unicode follows.

## Other generated files

- `src/generated/pow5_tables.h`, the shortest-digits tables: `python3 tools/gen_pow5_tables.py` (`--check` to verify). They depend on nothing but the algorithm.
- `test/test262_*.inc`, test262's Temporal assertions: [oracles.md](oracles.md#test262-tables).
- `test/*_icu.tsv`, ICU4C's date pattern answers: [oracles.md](oracles.md#icu-date-tables).

## External data

Files from outside the project that the repository keeps are in `tools/external/<source>/`, exactly as published, each folder with a `README.md` naming the source, version, license and the SHA-256 of every file; `tools/external-dirs.txt` lists the folders, which the source checks skip. A file too large to keep (Unihan's 8 MB archive) is fetched by its pin instead, and its check runs only when the pin changes and every night.
