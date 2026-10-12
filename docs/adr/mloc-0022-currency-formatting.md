# mloc-0022. Currency formatting: symbols field by field, spacing as ICU

Status: Accepted

## Context

CLDR 49 gives currencies per locale: symbols (standard, narrow, formal,
variant), names, a handful of currencies' own patterns and separators
(Portugal's escudo writes its cifrão as the decimal separator), and per
numbering system the standard and accounting patterns, each with an
`alphaNextToNumber` variant and a `noCurrency` variant. The
supplemental `currencyData` gives each currency's digits and its cash
rounding. Of 166,185 symbol and name entries, 82,427 differ from what a
locale's parent gives; the symbols among them are 2,957. UTS #35
prefers the `alphaNextToNumber` pattern, "when the currency symbol
character closest to the numeric value has General Category L", to
root's `currencySpacing`, which ICU4C still applies: a no-break space
where a character that is neither a symbol nor a space meets a digit.
CLDR's test data has no currency formatting cases.

## Decision

- **Data:** three units of the number part (`src/currency_tables.h`).
  The fractions, a unit of no locale: each currency's digits and
  rounding, standard and cash, and a default entry. The currency
  pattern sets, a unit of no locale, which each numbers entry names:
  the standard and accounting patterns and their `alphaNextToNumber`
  and `noCurrency` variants. The currency symbols, a unit per locale key
  that gives any of its own, in a sparse shape (sorted codes, a row per
  code with a mask of the fields present): the fields the key itself
  gives, each kept only where it differs from what its parent's chain
  gives. A currency's fields are inherited one by one, as UTS #35
  resolves them, so a formatter walks the locale's chain once when it
  is made (`mlocVisitLocaleUnits`), each key in every source; a
  currency's own pattern names a pattern of its own unit's source.
- **Spacing:** the `alphaNextToNumber` pattern is used when the
  currency's character closest to the digits is neither a symbol nor a
  space (General_Category S or Z): letters, as UTS #35 says, and the
  rest of what root's `currencySpacing` spaces ("Fr. 1,234.50", "USD
  1,234.50", "$1,234.50"). The letter rule alone would write
  "Fr.1,234.50". The generator stores, per symbol, whether its first
  and its last character are of that kind, from Maul Unicode's
  General_Category, which the generator links and the library does not.
  A variant written for another pattern is not used: an
  `alphaNextToNumber` pattern that differs from its pattern in more than
  spaces (Indian English inherits English's grouping, and where a
  locale's own one is below the draft threshold root's stands), or a
  `noCurrency` pattern whose digits differ, gives way to the pattern
  spaced as `currencySpacing` says, or without its currency.
- **Formatter:** `mlocCurrencyFormatter` embeds the number formatter's
  core and options (`number_format.h`, shared). Displays: symbol,
  narrow, code, formal, variant (a variant the locale lacks is the
  symbol, a symbol it lacks the code) and hidden, which takes the
  `noCurrency` pattern (ICU's hidden display keeps the standard pattern
  without its symbol, and its spaces). Accounting comes from the
  locale's `-u-cf-account` alone, as the numbering system from `-u-nu`
  (mloc-0021). A currency's own pattern replaces the standard pattern;
  its own separators, or else the locale's currency separators, replace
  the decimal and group separators. Negative affixes without a minus
  sign's place (accounting's parentheses) stand for the sign.
- **Digits:** the zero precision of a currency formatter rounds to the
  currency's digits and rounding increment (cash ones with cash
  usage), in the options' rounding mode, as ECMA-402 and ICU do: a
  second default rounding beside compact notation's. A precision the
  options give replaces it.
- **Compact notation:** each pattern set also names the short compact
  currency table and its `alphaNextToNumber` table (both lengths take
  them: CLDR has no long currency patterns, and ICU falls back to the
  short ones); hidden, compact notation takes the short decimal table,
  as UTS #35 says when there is no `noCurrency` compact pattern. A
  compact pattern switches to its `alphaNextToNumber` variant only where
  its currency meets its zeros (UTS #35; Bengali's "0 লা¤" keeps its
  pattern), and a variant written for another pattern gives way as
  above. The compact rounding applies unless the options give a
  precision, as ICU does.
- **Names:** a fourth unit per locale key, in the same sparse shape:
  the currency's name and its name for each plural category. The
  generator writes them, 2.1 MB, into a data file of their own,
  `src/generated/currency-names.mloc`, beside the compiled-in data (267
  KB), which a program adds to its data set to show names: the library
  stays small, and data without names is a choice (L20). Fields are
  inherited one by one with the inheritance marker counting as absent,
  then the lateral fallback from the locale asked for (a category's name
  to other's, then to the name; an alternative symbol to the symbol):
  CLDR's own resolution, which agrees with CLDR's JSON on 481,409 of
  481,455 currency values (all but 46 Norwegian Nynorsk names, where the
  JSON takes Norwegian's).
- **Long names:** the name display writes the decimal pattern's number
  (the currency's digits, the currency separators) inside the unit
  pattern of the number's plural category, with that category's name
  (UTS #35, ICU); the unit patterns and the ISO append pattern are latn's
  (root gives other systems none, ICU reads latn's too) and live as texts
  of the pattern sets. Compact notation with a name takes the decimal
  compact table of its length ("1.2 million US dollars"). A name the
  data does not give is the code, and showing names needs the names data
  file in the set (`mloc_errorNotFound` otherwise).
- **ISO append:** an option of the symbol displays writes the amount
  inside `currencyPatternAppendISO` ("$1,432.00 USD").
- **Lookups:** `mlocGetCurrencySymbol`, `mlocGetCurrencyName` (ECMA-402
  DisplayNames of type currency) and `mlocGetCurrencyDigits`; the code
  stands in, with `mloc_fellBack`, where the data has no text.
- **Ranges** are the number formatter's (`mlocFormatCurrencyRange`). A
  name or the appended code is written once around both ends ("1.00–5.00
  US dollars"), in the category CLDR's plural ranges give the ends'
  categories, as ICU collapses its outer modifier.
- **Proof:** ICU4C 79.1rc, built on CLDR 49, as oracle: 105 locales,
  12 currencies (one the data does not know), every display, accounting
  and cash, four amounts, 65,520 cases: every case agrees but the
  hidden display's, which differ in the spaces and bidi marks where the
  symbol stood; and 13,230 range cases agree. In compact notation
  (40,320 cases: 6 currencies, four displays, both lengths, eight
  amounts) every case agrees but Afrikaans's, whose own
  `alphaNextToNumber` compact patterns write no space ("USD1,2 m."; ICU
  reads no such pattern and spaces by `currencySpacing`), and the hidden
  display's, whose short decimal table has other words than the
  currency table in Macedonian, Mongolian and Pashto. Long names (11,340
  cases, 105 locales, 12 currencies, nine amounts) agree but Burmese's,
  whose unit pattern CLDR 49 gives as "{1} {0}" (XML and JSON; ICU
  writes "{0} {1}"), and the 46 Nynorsk names above; name ranges (2,100
  cases) agree but those and the locales without plural ranges, where
  UTS #35 says a pair without a value takes the end's category and ICU
  takes other. The tests take their expected text from it.

## Consequences

The currency units cost about 65 KB of the compiled-in data: the
symbols units 44 KB, the compact currency tables about 20 KB; the names
are the data file of 2.1 MB that a program adds when it shows names. The names and their plural forms, ten times
the symbols, are a kind of their own, so data without them is a choice.
Building the generator fetches Maul Unicode.
