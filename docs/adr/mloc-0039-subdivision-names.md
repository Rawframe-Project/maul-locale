# mloc-0039. Subdivision names: a small file of their own, read through the names lookup

Status: Accepted

## Context

A locale id's `sd` and `rg` values are subdivision codes (`gbsct`,
`usca`), and CLDR's NameGetter names them by
`common/subdivisions/<locale>.xml` (record mloc-0038, whose test file
holds four such lines). mloc-0036 sent these names to a second file,
expecting about five megabytes. That figure counts every draft: at the
threshold the generator uses for all CLDR data (contributed), CLDR 49
keeps 5,425 English names and three in each of about a hundred other
locales (Scotland, England, Wales). The rest are provisional. Kept per
key as the other names are, the text is 55 KB; English's own unit is
the largest, at 51 KB.

ICU4C and ICU4X carry no subdivision names.

## Decision

- **A file of its own,** `subdivisions.mloc`: a program that never
  names an `sd` or `rg` value ships no subdivision names. The codes would
  fit in `names.mloc`'s table (1,681 codes and 5,425 more, against at
  most 8,192), but that would leave little room for later CLDR
  releases.
- **The same units:** a codes unit (kind 0x00070001), whose codes are
  `d:` and the subdivision code (`d:gbsct`), and one unit per key in the
  lean names layout (kind 0x00070009, the name field alone). The names
  lookup already passes over a source whose codes lack the one asked
  for, so one data set may hold both files and either finds its own
  codes. The generator stops if a unit's text reaches 64 KB, as for
  every names unit.
- **One titlecase table.** `names.mloc`'s table also covers the first
  letters of subdivision names. The generator builds both files in one
  run, so the table is the same for both. With `subdivisions.mloc`
  alone, ASCII and the Turkic "i" are still titlecased.
- **Lookups:** `mlocGetName` gains `mloc_nameSubdivision`. It takes
  the code in any case, with the code as the fallback as for every
  type. `mlocGetLocaleName` names `sd` and `rg` values this way, as
  NameGetter does; a value with no name stays as written. NameGetter
  would write "null" there instead.

## Proof

`test_names_data` reads the two files in one set, in each order: each
finds its own codes; a subdivision has a name in English, inherited by
British English, and in French, and none where CLDR has only a
provisional one.

## Consequences

The file is 136 KB. Outside English (whose regional locales
inherit its names), a subdivision other than the three British ones
has no name, so the code is written, as CLDR's tools do. The
provisional names would come in only with a lower draft threshold. The generator offers no such
threshold, and the 64 KB unit limit would then need wide offsets.
