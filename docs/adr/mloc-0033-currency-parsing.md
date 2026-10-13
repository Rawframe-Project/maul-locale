# mloc-0033. Currency parsing: ICU's currency matcher, the chain's names without allocation

Status: Accepted

## Context

An amount of money typed into a form names its currency as the person
writes it: "$12.50", "12,50 €", "CHF 1’234.00", "12.50 US dollars",
"usd 12". ICU4C reads it with DecimalFormat::parse on a currency
formatter (its CombinedCurrencyMatcher: the formatter's symbol, its
ISO code in any case, its long names) and with parseCurrency, which
reads any currency through uprv_parseCurrency: every symbol and name
of the locale's chain, the names uppercased, root's equivalent signs
("＄" for "$"). ICU4X and ECMA-402 do not parse. ICU builds and caches
a sorted table of every currency name per locale; the library has no
cache and allocates nothing.

## Decision

- **API**: `mlocParseCurrency(formatter, text, length, mode,
  anyCurrency, decimalOut, codeOut, endOut)` on a currency formatter
  not of compact notation, the rules of `mlocParseNumber` (record
  mloc-0032) with the currency added. `codeOut` gets the code read, or
  the formatter's when none was typed; with `anyCurrency` 1 a currency
  is required (ICU's parseCurrency). Long names are read when the
  formatter's data set had the currency names file; `anyCurrency`
  needs every currency unit of the locale's chain to have fit the
  formatter (eight of a kind; `mloc_errorNotFound` otherwise, which no
  CLDR locale reaches).
- **Rules** (`src/parse_currency.c`): ICU 79's matcher list with the
  currency matcher where createParserFromProperties puts it, and the
  currency validator for parseCurrency. The currency matcher: the
  currency spacing NBSP after a number as a weak match (ICU's
  short-circuit when one UTF-16 unit follows it is kept: "706 $" ends
  at 3 in strict mode), then the formatter's symbol as written, the
  text it shows when that is another (a narrow symbol; an extension:
  ICU reads only the symbol), its code case folded, then either its
  long names (the longest, case folded, as getPluralName gives them)
  or for any currency uprv_parseCurrency: step by step along the
  chain, the currencies a step gives a symbol or a name of its own,
  not listed at an earlier step, each with its symbol and names
  resolved from that step down, and the step's own plural names; names
  uppercased as the locale uppercases (u_strToUpper's full mappings,
  Turkic i for tr and az) and compared by their own UTF-16 length,
  symbols as written, a name winning a tie; the end moved by UTF-16
  units, the input limited to 100 of them as ICU limits it. The search
  is a walk over the kept units with first-character rejects and
  ASCII fast paths; it is skipped where the text begins with a digit
  or an ignorable but a bidi control, which no currency symbol or name
  begins with (the generator refuses data where one would). A
  currency formatter's parse affixes are the standard pattern's (ICU
  parses with it, not with the alphaNextToNumber variant it formats
  with); a formatter of long names or of the ISO append parses within
  its outer pattern of the category other, with the name placeholder
  in the currency's place or the code the ISO append writes (ICU's
  currency plural patterns, "#,##0.00 ¤¤¤").
- **Data**: the parse sets gain the uppercase mappings u_strToUpper
  makes (runs, and the special mappings to several code points, from
  Maul Unicode); the currency formatter keeps its chain's currency
  units (the opaque value is 768 bytes).
- **Departures**: ICU walks truncation parents for uprv_parseCurrency
  (en_IN to en, hi_Latn to hi, es_MX to es, en_AU to en, nn to root);
  the library walks its CLDR chain, which for nn also differs in 46
  names in ICU's data. Where a long name begins with the currency's
  code ("Euro", "rubles"), the longer is read: ICU takes the code and
  leaves "o" of "12,50 Euro" unread, so its formatter cannot read its
  own long names. ICU's strict mode fails on its own currency plural
  output ("-1,234.50 US dollars" at offset 0); the library reads what
  it writes in both modes.
- **Proof**: the currency oracle (`docs/locale/research/oracle/parse`)
  over the 105 locales, twelve currencies, four displays and three
  values, the formatted text and ten variants each, parsed with the
  formatter's currency and another, both modes, with and without any
  currency: 472,010 lines, 468,298 equal to ICU4C 79's; every other
  line is a departure above (2,362 the code-led names, the rest nn,
  hi_Latn, en_IN, es_MX, es_US and en_AU), as are the differing lines
  of three random corpora of 25,200 lines each. The tests hold 88 of
  those cases, the round trip of four displays in 20 locales for six
  currencies in both modes with and without any currency, the absence
  of the names file, and the refusals.

## Consequences

Reading the formatter's currency costs about 1.5 µs a parse; reading
any currency about 30 µs where a name or a symbol has to be searched
(ICU's cached table is not built), nothing until called. The fuzz
target follows in its own step.
