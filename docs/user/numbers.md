# Numbers

Numbers are formatted from decimals, chosen by plural rules, written as currencies and units, spelled out in words and read back as a person types them. The headers are `maul-locale/decimal.h`, `plural.h`, `number.h`, `currency.h`, `unit.h` and `spellout.h`.

## Contents

- [Decimals](#decimals)
- [Plural rules](#plural-rules)
- [Formatting numbers](#formatting-numbers)
- [Currencies](#currencies)
- [Units](#units)
- [Numbers in words](#numbers-in-words)
- [Reading numbers](#reading-numbers)

## Decimals

Every number the library formats is an `mlocDecimal`: up to 40 significant digits, an exponent and the fraction digits it shows, so 1.50 and 1.5 are different decimals with the same value. Make one with `mlocMakeDecimal` from an integer, `mlocParseDecimal` from text such as `"-1234.50"` or `"6.02e23"`, or `mlocMakeShortestDecimal` from a `double`: the shortest digits that read back as the same double, computed with integers, so 0.1 is 0.1 on every platform.

Number and unit formatters do not round unless asked: a decimal shows its own digits. Currency formatters round to the currency's digits. `mlocRoundDecimal` rounds in any of ECMA-402's nine modes to an increment at any position, and `mlocShowFractionDigits` sets the fraction digits shown. The precision options of each formatter do the same while formatting.

## Plural rules

```c
mlocPluralRules rules;
mlocPluralCategory category = mloc_pluralOther;
if (mlocInitPluralRules(&rules, &set, &locale, mloc_pluralCardinal) == mloc_success &&
    mlocSelectPlural(&rules, &value, &category) == mloc_success)
{
    // pl, 5: mloc_pluralMany ("5 plików"); 2: mloc_pluralFew ("2 pliki")
}
```

The category depends on the digits shown: in English 1 is one and 1.0 is other, so select on the decimal as it will be formatted. Ordinal rules (`mloc_pluralOrdinal`) give "1st", "2nd", "3rd"; `mlocSelectPluralRange` gives the category of a range from its ends', and `mlocGetPluralCategories` the categories a locale uses, so a program knows which translations it needs.

## Formatting numbers

`mlocInitNumberFormatter` makes a formatter of a locale with `mlocNumberOptions`; the zero value is the default. The options follow ECMA-402's `Intl.NumberFormat`: standard, scientific, engineering and compact notation ("1.2K", "1.2 thousand"), percent and per mille, grouping, sign display, minimum integer digits, and precision by fraction or significant digits with a rounding mode and increment. The numbering system comes from the locale's `-u-nu` key or its default (Arabic-Indic digits for `ar-EG`).

```c
mlocNumberOptions options = {.notation = mloc_notationCompactShort};
mlocNumberFormatter formatter;
char text[32];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitNumberFormatter(&formatter, &set, &locale, &options) >= 0 &&
    mlocFormatNumber(&formatter, &value, &output) == mloc_success)
{
    // en, 1234567: "1.2M"; ja: "123万"
}
```

`mlocFormatNumberRange` writes ranges as ECMA-402's `formatRange` does ("3–5", "~5" when both ends write alike). With a part function set, the output reports each part: integer digits, group and decimal separators, the sign, the exponent, a compact suffix.

A formatter is a value of a few hundred bytes that points into the data it was made from. Make it once per locale and options, not per number.

## Currencies

`mlocInitCurrencyFormatter` makes a formatter of one currency in a locale, by its ISO 4217 code. Amounts round to the currency's digits (0 for JPY, 3 for BHD), or its cash rounding (CHF to 0.05) with `mloc_currencyUsageCash`. The display is the symbol, narrow symbol, code or name ("3.00 US dollars", which needs `currency-names.mloc`); the sign can be accounting ("($3.00)"), and compact notation works ("$1.2K").

```c
mlocCurrencyFormatter formatter;
char text[32];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitCurrencyFormatter(&formatter, &set, &locale, "EUR", 3, nullptr) >= 0 &&
    mlocFormatCurrency(&formatter, &value, &output) == mloc_success)
{
    // de-AT, 1234.5: "€ 1.234,50"; fr: "1 234,50 €"
}
```

`mlocFindCurrency` gives the currency a locale uses on a given day (its `-u-cu` key, else its region's legal tender then), `mlocGetRegionCurrencies` a region's currencies past and present, `mlocGetCurrencySymbol` and `mlocGetCurrencyName` the names a currency menu shows, and `mlocGetCurrencyDigits` the digits and rounding.

## Units

`mlocConvertUnit` converts between any of CLDR's units of the same dimensions, compound ones included ("foot-per-minute" to "mile-per-hour", "mile-per-gallon" to "liter-per-100-kilometer"), exactly, rounding once at the end. `mlocGetUnitPreference` chooses the unit a locale prefers for a usage and value: a road distance of 1000 meters is "0.62 mi" in the United States and "1 km" in Germany; a person's height can be feet and inches.

`mlocInitUnitFormatter` formats amounts of a unit, which needs `units.mloc`. The width is short ("5 m"), narrow ("5m"), long ("5 meters") or numeric for durations ("1:05:09"). Any core unit and any compound of them works ("kilowatt-hour-per-100-kilometer"); a name CLDR lacks is built from its parts. A mixed unit ("foot-and-inch") is split exactly ("5 ft, 6 in"). A usage in the options writes each amount in the unit the locale prefers, and a grammatical case writes the long name inflected ("2 Metern", German's dative).

```c
mlocUnitOptions options = {.width = mloc_unitWidthLong};
mlocUnitFormatter formatter;
char text[64];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitUnitFormatter(&formatter, &set, &locale, "kilometer-per-hour", 18, &options) >= 0 &&
    mlocFormatUnit(&formatter, &value, &output) == mloc_success)
{
    // en, 120: "120 kilometers per hour"
}
```

`mlocGetUnitDisplayName` names a unit for a menu ("meters"), and `mlocGetUnitGender` gives its grammatical gender in a locale, which a message that agrees with a formatted amount needs.

## Numbers in words

With `spellout.mloc` in the set, `mlocInitSpelloutFormatter` makes a formatter of one of CLDR's rule-based formats: words (`%spellout-cardinal`, `%spellout-ordinal`, `%spellout-numbering-year`, and in languages with genders `%spellout-cardinal-feminine` and the like), ordinal suffixes (`%digits-ordinal`, "21st") or a numbering system's rules (`%roman-upper`). The names differ by language; `mlocGetSpelloutRuleSet` lists the rule sets a locale has. German writes long numbers with soft hyphens between their parts, as CLDR's rules do. `mlocInitNumberingSystemFormatter` makes one by a numbering system's id instead: `roman`, `hebr`, `hans`, `jpanyear`.

```c
mlocSpelloutFormatter formatter;
char text[128];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitSpelloutFormatter(&formatter, &set, &locale, "%spellout-cardinal", 18, nullptr) >=
        0 &&
    mlocFormatSpellout(&formatter, &value, &output) == mloc_success)
{
    // en, 1234: "one thousand two hundred thirty-four"
}
```

The value is formatted exactly, whatever its size or fraction. `mlocParseSpellout` reads words back as ICU's strict parse does.

## Reading numbers

`mlocParseNumber` reads a number as a person types it in a locale: its separators and digits (any Unicode decimal digits), signs, percent and per mille signs, exponents, and the formatter's affixes. Lenient mode, the default, accepts what a person would mean (separators by their equivalence classes, grouping sizes unchecked); strict mode accepts only what the formatter would write. It reports where the number ends, so text may follow.

```c
mlocDecimal parsed;
size_t end = 0;
mlocResult read =
    mlocParseNumber(&formatter, "1.234,5 kg", 10, mloc_parseLenient, &parsed, &end);
if (read == mloc_success)
{
    // de: parsed is 1234.5; end is 7, before " kg"
}
```

`mlocParseCurrency` reads an amount of money the same way, with the formatter's currency or any currency the locale names.
