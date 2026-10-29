# Grammar

In agglutinative languages a name or a number in a message takes suffixes that depend on how its end is spoken: Turkish writes "Ayşe'ye" but "Ali'ye", "1985'te" but "1990'da", "TDK'nin" by the letters' names. A translator cannot write these into a message for values only known at run time. `maul-locale/grammar.h` inflects runtime text and numbers for case, number, possessor and copula, and Korean particles, by the rules of the language.

## Languages

| Language | What it writes |
|---|---|
| Turkish, Azerbaijani, Kazakh, Kyrgyz, Turkmen, Uzbek | Case, plural, possessive and copula suffixes by vowel harmony and consonant assimilation, and the question particle. |
| Hungarian | Case suffixes by vowel harmony and assimilation ("Kodállyal", "5-tel"). |
| Finnish | Case endings by the word's inflection type and consonant gradation ("Helsingissä", "EU:hun"). |
| Korean | Particles by whether the spoken end has a final consonant ("책은", "URL로"). |

Another language's inflector refuses with `mloc_errorNotFound`.

## Inflecting text and numbers

`mlocInitInflector` makes a locale's inflector. `mlocInflector_InflectText` writes a text with the suffixes an `mlocInflection` asks for; `mlocInflector_InflectNumber` writes a number as a number formatter writes it, inflected by how that number is spoken, which the spell-out rules give (`spellout.mloc`; Uzbek and Turkmen have their number words in code).

```c
mlocInflector inflector;
mlocInflection dative = {.grammaticalCase = mloc_caseDative};
char text[64];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitInflector(&set, &locale, &inflector) == mloc_success &&
    mlocInflector_InflectText(&inflector, "Ayşe", 5, &dative, &output) == mloc_success)
{
    // tr: "Ayşe'ye"
}
```

The end of a text is read by its look unless the request says otherwise: a number by how it is spoken, a word in capitals by its letters' names ("ABD'ye"), anything else as written. `reading` forces a word or letters (NATO is read as a word), and `hint` gives the spoken form where spelling misleads (a hint "jak" for "Jacques"). A text whose end cannot be read is written as best it can be, or refused when the request asks to be exact. A common noun takes no apostrophe and softens its final stop (kitap, kitabı); names, numbers and abbreviations are the default.

Parts mark the stem, the boundary mark (the apostrophe, hyphen or colon), the suffixes and a question particle, so a program can style the value apart from its suffix.

## In messages

In a [message](messages.md), `mloc:case` and `mloc:particle` inflect a placeholder, and `mloc:gender` selects on the gender of a list of persons:

```
{$name :mloc:case case=dative}
{$year :integer useGrouping=never} → {$year :mloc:case case=locative}
{$company :mloc:particle particle=topic}
```

`mloc:case` takes `case` (CLDR's names: nominative, dative, locative, inessive and the others), `number` (singular, plural), `possessor` (none, 1, 2, 3, 1p, 2p, 3p), `copula`, `question`, `noun` (proper or common), `reading` and `hint`; `mloc:particle` takes `particle`, `reading` and `hint`. A number placeholder is inflected as its own function formats it: "1.985'te" after `:integer`, "1985'ten" without grouping. A request the language refuses gives the placeholder's fallback.
