# mloc-0052. Messages: mloc:case and mloc:particle

Status: Accepted

## Context

The inflector (records mloc-0047 to mloc-0050) writes a name, a word
or a number with its suffixes or particle. A message's runtime values
are names and numbers, so MessageFormat 2 needs functions that inflect
them: "{$name :mloc:case case=dative}" gives "Rodrigo'ya". Record
mloc-0045's mloc: functions (mloc:list above all) set the pattern
these follow: a value of their own, options parsed at resolution, what
binding prepares, the fallback on a fault.

## Decision

**`mloc:case` and `mloc:particle`** make a value of their own over one
of:
- A text: a literal, a string argument, or `:string` over one.
- A number value: `:number`, `:integer` or `:percent`, or a numeric
  argument read as `:number`.

Any other operand is a Bad Operand. A number is inflected as its own
formatter writes it (`mlocInflectNumber`): "1.985'te" after
`:integer`, "1985'ten" with `useGrouping=never`.

**Options of mloc:case**, literals that choose the rules:
- `case`: CLDR's names (nominative, dative, locative, inessive ...).
- `number`: singular or plural.
- `possessor`: none, 1, 2, 3, 1p, 2p or 3p.
- `copula` and `question`: true or false.
- `noun`: proper, the default since a message's values are names, or
  common.
- `reading`: auto, word or letters.

`hint` gives the spoken text and may be a variable's.

**mloc:particle** takes `particle` (topic, subject, object ...
definition, as `mlocKoreanParticle`), `reading` and `hint`. It never
asks to be exact: in a message, both forms ("Apple이(가)") are better
than a fallback.

**Faults:**
- An unknown value, or a variable where a literal is needed, is a Bad
  Option, and the option is ignored, as MF2 ignores one: "Rodrigo" for
  `case=dativ`.
- A request the language refuses (Turkish has no partitive; only
  Korean has particles) is a Bad Option at formatting, and the
  expression's fallback ("{$n}") stands in its place.
- A locale without rules is an Unsupported Operation, with the same
  fallback.

**Binding prepares** the locale's inflector once per expression. A
number's formatter is made in the call, as `mlocWriteNumberValue`
makes one when none is prepared.

## Proof

- `test_message_grammar`:
  - Turkish: cases, a possessor, the copula and question, a common
    noun, a literal, a hint by variable, numbers through `:integer`
    and bare.
  - Finnish and Hungarian.
  - Korean particles, both forms, and a hint.
  - The faults above.
- `fuzz_message` formats a message naming a mloc: function in Turkish
  and Korean as well as English, with two seeds that use the functions.

## Consequences

- A number is inflected only through `:number`, `:integer` or
  `:percent`. A currency or unit amount has no reading in the inflector
  and is a Bad Operand.
- mloc:gender, the selector on a list of persons (record mloc-0051), is
  the next amendment.
