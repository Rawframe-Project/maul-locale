# mloc-0009. Grammatical functions for messages

Status: Accepted

## Context

A translated message often attaches a suffix or particle to a value
known only at run time: Turkish `Rodrigo'ya` and `Elly'ye`, Korean
`이/가`. MessageFormat 2 selects on plural and gender but does not
inflect, and no common library does this for Turkish.

## Decision

Messages get functions, in the library's own namespace, for case
suffixes in Turkish and the other Turkic languages (vowel harmony,
consonant assimilation, buffer consonants and Turkish apostrophe
rules), Hungarian and Finnish, Korean particles by the final
consonant, and gender for one person or a list. Suffixes follow
pronunciation: numbers go through spell-out; abbreviations, foreign
names and loanwords that break harmony take a hint from the caller or
the translator. Each function documents what it cannot decide.
Languages whose tables no native speaker has reviewed are marked so in
the documentation until one does.

## Consequences

Translators write grammatical messages without code changes, and the
limits of each language are stated, not discovered.
