# Maul Locale API reference

Generated from the public headers by `tools/gen_api.py`. The headers
are the source of truth; this file mirrors them.

## `base.h`

The base of the Maul Locale API: the library and CLDR versions, the export and attribute macros, and the result codes every fallible function returns.

```c
mlocVersion mlocGetVersion(void);
```
Returns the version of the library that was linked, which may differ from the MLOC_VERSION macros a program was compiled with.  @return The library version. @par Thread safety Safe from any thread.

```c
mlocVersion mlocGetCldrVersion(void);
```
Returns the CLDR release the library's code and data come from.  @return The CLDR version. @par Thread safety Safe from any thread.

```c
const char* mlocResultName(mlocResult result);
```
Returns the name of a result code, for diagnostics.  @param result  Any value; an unknown one is named as such. @return A static, NUL-terminated string such as "mloc_errorCapacity". @par Thread safety Safe from any thread.

## `calendar.h`

Calendars (UTS #35 Part 4, record mloc-0040): dates as values in CLDR's calendars, as Temporal models them. A date is a day, counted from January 1 of year 1 in the proleptic Gregorian calendar (day 1), read in a calendar; its fields (year, era, month code, day) are computed from it. Calendars without a built implementation yet refuse with mloc_errorNotFound.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeDate(mlocCalendar calendar, const mlocDateInput* input, mlocOverflow overflow, mlocDate* dateOut);
```
Makes a date from its fields in a calendar, as Temporal's PlainDate.from does: a month or day past the calendar's is clamped to the last (mloc_overflowConstrain) or refused (mloc_overflowReject); a month code no year of the calendar has, an unknown era, or a month or day below 1 is refused either way.  @param calendar  The calendar. @param input     The fields. @param overflow  What a month or day past the calendar's does. @param dateOut   Receives the date; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a value out of range or refused, or a date outside Temporal's range; mloc_errorSyntax for an era or month code not of the calendar; mloc_errorNotFound for a calendar not built yet. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeDateFromDay(mlocCalendar calendar, int32_t day, mlocDate* dateOut);
```
Makes the date of a day in a calendar.  @param calendar  The calendar. @param day       The day, 1 for January 1 of year 1 in the proleptic Gregorian calendar. @param dateOut   Receives the date; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a calendar out of range, or a day outside Temporal's range; mloc_errorNotFound for a calendar not built yet. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetDateFields(const mlocDate* date, mlocDateFields* fieldsOut);
```
Computes a date's fields in its calendar.  @param date       The date. @param fieldsOut  Receives the fields. @return mloc_success; mloc_errorInvalid for a null pointer or a date the library did not make (a day out of range, an unknown calendar, reserved bytes not zero); mloc_errorNotFound for a calendar not built yet. @par Thread safety Safe from any thread.

## `currency.h`

Currency formatting (UTS #35 Part 3, record mloc-0022): an amount of an ISO 4217 currency in a locale's currency pattern, with its symbol or code, rounded to the currency's digits unless the options say otherwise. The options follow ECMA-402's NumberFormat with the currency style; their zero value is the default. Lookups give a currency's symbol, name and digits, and the currency a locale or a region uses on a day.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitCurrencyFormatter( mlocCurrencyFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const char* code, size_t codeLength, const mlocCurrencyOptions* options);
```
Makes a locale's formatter of a currency. The numbering system is the locale's, as for numbers; the locale's -u-cf-account keyword asks for its accounting pattern ("($3.00)"). The symbol meets the digits as UTS #35 prefers: a locale's alphaNextToNumber pattern spaces a symbol whose character next to the digits is a letter ("USD 3.00", "$3.00"). A code the data does not know shows as its code with the default digits.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number and currency units, the parents and the likely subtags. @param locale        The locale, canonicalized. @param code          The currency's ISO 4217 code, three ASCII letters in any case. @param codeLength    Its length in bytes, 3. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorNotFound for data without the number or currency units (or, for names, the currency names); mloc_errorInvalid for a null pointer, a code that is not three letters, or options out of range or in conflict (a percent style, the code appended to a display that is no symbol). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatCurrency(const mlocCurrencyFormatter* formatter, const mlocDecimal* decimal, mlocOutput* output);
```
Formats an amount of the formatter's currency.  @param formatter  A formatter from mlocInitCurrencyFormatter. @param decimal    The amount. @param output     Receives the text; its part function, when set, receives mlocNumberPart values, the currency as mloc_numberPartCurrency. @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding passes the decimal's limits; mloc_errorInvalid for a null pointer, or a formatter or decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatCurrencyRange(const mlocCurrencyFormatter* formatter, const mlocDecimal* start, const mlocDecimal* end, mlocOutput* output);
```
Formats a range of two amounts, as mlocFormatNumberRange does ("$3.00 – $5.00", "3,00–5,00 €"; ends that write alike give "~$5.00"). A name, or the appended code, is written once when both ends take the same plural category ("3.00–5.00 US dollars"), else for each.  @param formatter  A formatter from mlocInitCurrencyFormatter. @param start      The range's start. @param end        Its end. @param output     Receives the text; its part function, when set, receives mlocNumberPart values with the end each belongs to (mloc_numberRangeStart, mloc_numberRangeEnd, or 0 for both). @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding passes the decimal's limits; mloc_errorInvalid for a null pointer, a NaN end, or a formatter or decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseCurrency(const mlocCurrencyFormatter* formatter, const char* text, size_t length, mlocParseMode mode, uint8_t anyCurrency, mlocDecimal* decimalOut, char* codeOut, size_t* endOut);
```
Reads an amount of money as a person types it (ICU's DecimalFormat::parse on a currency formatter, or with anyCurrency its parseCurrency), as mlocParseNumber reads a number: the formatter's separators and pattern, and the currency where the pattern or the person puts it: the formatter's symbol as written (or the text it shows, a narrow symbol), its code in any case, its long names case folded ("12.50 US dollars"). With anyCurrency, any currency the locale's chain names: its symbol (with root's equivalent signs, "＄" for "$"), its code, its names uppercased as the locale uppercases; a currency is then required. A formatter of long names or of the ISO append reads the text around its number as it writes it ("12.50 US dollars" in strict mode too).  @param formatter    A formatter from mlocInitCurrencyFormatter, not of compact notation. Its long names are read when its data set had the names data file. @param text         UTF-8 text; it need not end in a 0 byte. @param length       Its length in bytes. @param mode         An mlocParseMode. @param anyCurrency  1 to read any currency, 0 for the formatter's alone. @param decimalOut   Receives the amount. @param codeOut      Receives the currency's code, three letters (the formatter's when none was typed). @param endOut       Receives the offset after the amount, or on mloc_errorSyntax the offset reading stopped at. @return mloc_success; mloc_errorSyntax when no amount is read (or no currency, with anyCurrency); mloc_errorCapacity as for mlocParseNumber; mloc_errorNotFound for data without the parse sets, or with anyCurrency a chain of more than eight currency units of a kind; mloc_errorInvalid for a null pointer, a mode out of range, or a formatter the library did not make or of compact notation, or anyCurrency past 1. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetCurrencySymbol(const mlocDataSet* data, const mlocLocale* locale, const char* code, size_t codeLength, mlocCurrencyDisplay display, mlocOutput* output);
```
Writes a currency's symbol, or a variant of it, as a locale shows it (the ECMA-402 DisplayNames of type currency take names, below).  @param data        The data with the currency symbols. @param locale      The locale, canonicalized. @param code        The currency's ISO 4217 code, three ASCII letters. @param codeLength  Its length in bytes, 3. @param display     A symbol display, or the code. @param output      Receives the text. @return mloc_success; mloc_fellBack when the locale has no symbol and the code is written instead; mloc_errorCapacity when the text does not fit (output->length is then the length needed); mloc_errorNotFound when the chain cannot be walked; mloc_errorInvalid for a null pointer, a code that is not three letters, or a display that is no symbol. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetCurrencyName(const mlocDataSet* data, const mlocLocale* locale, const char* code, size_t codeLength, mlocOutput* output);
```
Writes a currency's name in a locale ("US Dollar", "Dollar américain"), as a menu of currencies shows it.  @param data        The data with the currency names data file. @param locale      The locale, canonicalized. @param code        The currency's ISO 4217 code, three ASCII letters. @param codeLength  Its length in bytes, 3. @param output      Receives the text. @return mloc_success; mloc_fellBack when the data names it in no locale of the chain and the code is written instead; mloc_errorCapacity when the text does not fit (output->length is then the length needed); mloc_errorNotFound when the chain cannot be walked; mloc_errorInvalid for a null pointer or a code that is not three letters. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetCurrencyDigits(const mlocDataSet* data, const char* code, size_t codeLength, mlocCurrencyUsage usage, uint8_t* digitsOut, uint16_t* incrementOut);
```
Gives a currency's fraction digits and rounding increment (supplementalData.xml): JPY 0, BHD 3, CHF cash 2 by 5.  @param data          The data with the currency fractions. @param code          The currency's ISO 4217 code, three ASCII letters; a code the data does not list has the default, 2 digits. @param codeLength    Its length in bytes, 3. @param usage         Standard or cash. @param digitsOut     Receives the fraction digits. @param incrementOut  Receives the rounding increment in units of the last digit, 1 for none. @return mloc_success; mloc_errorNotFound for data without the fractions; mloc_errorInvalid for a null pointer, a code that is not three letters or a usage out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFindCurrency(const mlocDataSet* data, const mlocLocale* locale, int32_t day, char* codeOut);
```
Finds the currency a locale uses on a day: its -u-cu key's ("en-US-u-cu-eur" takes EUR), else the first legal tender in use that day of its region, the one mlocGetLocalePreferences takes ("de" takes Germany's: DEM in 1990, EUR now), else the first code in use that is none (Antarctica's code of no currency). The region's currencies are in CLDR's order, which puts the primary one first.  @param data     The data with the currency regions and the likely subtags. @param locale   The locale, canonicalized. @param day      The day (mlocRegionCurrency); INT32_MAX asks for the currencies still in use. @param codeOut  Receives the ISO 4217 code, NUL-padded, 4 bytes; left unchanged on failure. @return mloc_success; mloc_errorNotFound for data without the currency regions, or a region with no currency in use on the day; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetRegionCurrencies(const mlocDataSet* data, const char* region, size_t regionLength, mlocRegionCurrency* currenciesOut, size_t capacity, size_t* countOut);
```
Lists the currencies a region uses and used, in CLDR's order: on any day the first legal tender in use is the region's primary one.  @param data           The data with the currency regions. @param region         The region, two ASCII letters in any case or three digits. @param regionLength   Its length in bytes. @param currenciesOut  Receives the currencies; may be null when capacity is 0. @param capacity       The room in currenciesOut. @param countOut       Receives the count, 0 for a region the data does not list; with mloc_errorCapacity the count needed. @return mloc_success; mloc_errorCapacity when the currencies do not fit; mloc_errorNotFound for data without the currency regions; mloc_errorInvalid for a null pointer or a region that is not two letters or three digits. @par Thread safety Safe from any thread.

## `data.h`

The library's data: the compiled-in data and data files the program holds, each opened once into an mlocData view and read in place (record mloc-0014). The library reads no file and allocates nothing; the bytes a view was opened from must outlive it and everything made from it.

```c
void mlocGetDataVersion(mlocDataVersion* versionOut);
```
Returns the data version this library reads.  @param versionOut  Receives the version. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocReadDataVersion(const void* bytes, size_t size, mlocDataVersion* versionOut);
```
Reads the version of a data file from its header alone, so that a program can tell which file to fetch before it opens one.  @param bytes       The file's first bytes; at least 64. @param size        Their size. @param versionOut  Receives the version. @return mloc_success; mloc_errorInvalid for a null pointer; mloc_errorData when the bytes are no data file header of a format this library knows. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocOpenData(mlocData* dataOut, const void* bytes, size_t size);
```
Opens a data file held in memory. The whole file is validated here, once; a view that opened is never checked again.  @param dataOut  Receives the view; left unchanged on failure. @param bytes    The file, 8-byte aligned (any allocator's memory is). @param size     The file's size. @return mloc_success; mloc_errorInvalid for a null pointer or bytes that are not 8-byte aligned; mloc_errorDataVersion for a file of another data version (mlocReadDataVersion names it); mloc_errorData for bytes that are not a valid data file. @par Thread safety Safe from any thread.

```c
void mlocOpenBuiltinData(mlocData* dataOut);
```
Opens the data compiled into the library.  @param dataOut  Receives the view. @par Thread safety Safe from any thread.

```c
size_t mlocGetDataLocaleCount(const mlocData* data);
```
Returns the number of locales a source has data for.  @param data  An opened source. @return The count. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetDataLocale(const mlocData* data, size_t index, const char** idOut, size_t* lengthOut);
```
Returns one locale a source has data for, as CLDR names it ("de_AT", "sr_Latn"), in the order of their bytes.  @param data       An opened source. @param index      Below mlocGetDataLocaleCount. @param idOut      Receives the id, which is not NUL-terminated and lives as long as the source's bytes. @param lengthOut  Receives its length in bytes. @return mloc_success; mloc_errorInvalid for a null pointer or an index out of range. @par Thread safety Safe from any thread.

## `decimal.h`

Decimal values and their plural operands (record mloc-0019): a number is formatted from its decimal digits, never from a binary value, so the digits a user typed are the digits shown.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeDecimal(int64_t value, mlocDecimal* decimalOut);
```
Makes the decimal of a signed integer, with no fraction digits.  @param value       The integer. @param decimalOut  Receives the decimal. @return mloc_success; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeUnsignedDecimal(uint64_t value, mlocDecimal* decimalOut);
```
Makes the decimal of an unsigned integer, with no fraction digits.  @param value       The integer. @param decimalOut  Receives the decimal. @return mloc_success; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeShortestDecimal(double value, mlocDecimal* decimalOut);
```
Makes the shortest decimal that reads back as a double: of the decimals with the fewest digits that round to it, the nearest, ties to even (0.1 is "0.1", not "0.1000000000000000055511151231257827"). Its fraction digits shown are those its digits need. Infinity and NaN give decimals of those kinds, and -0.0 a negative zero. The digits come from integer arithmetic, the same on every platform.  @param value       The double. @param decimalOut  Receives the decimal. @return mloc_success; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocTextResult mlocParseDecimal(const char* text, size_t length, mlocDecimal* decimalOut);
```
Parses a decimal: an optional "+" or "-", digits with an optional "." and fraction (at least one digit in all), and an optional exponent, "e" or "E" with an optional sign and digits. The fraction digits written are the ones shown, the exponent moving the point: "1.50" shows two, "1.50e1" is 15.0 and shows one.  @param text        The text, without spaces. @param length      Its length in bytes. @param decimalOut  Receives the decimal; left unchanged on failure. @return status mloc_success with offset the length; mloc_errorSyntax with the offset of the byte at fault; mloc_errorCapacity with offset 0 for more than 40 significant digits, or digits or fraction digits shown past 10^999999 either way; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDecimal(const mlocDecimal* decimal, mlocOutput* output);
```
Writes a decimal as plain text with every digit and the fraction digits it shows, such as "-1234.50"; "Infinity", "-Infinity" or "NaN" for the others. mlocParseDecimal reads a finite one back to the same value. It reports no parts.  @param decimal  The decimal. @param output   Where the text goes. @return mloc_success; mloc_errorCapacity with the length needed; mloc_errorInvalid for a null pointer or a decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocRoundDecimal(mlocDecimal* decimal, int32_t position, uint32_t increment, mlocRoundingMode mode);
```
Rounds a decimal to a multiple of increment times 10^position: at position -2 with increment 5, 1.234 becomes 1.25 and 1.2 stays. It then shows the fewer of its fraction digits and -position (at least those its digits need), so 1.999 at -2 is 2.00 and 1.5 at -2 stays 1.5. A value that rounds to zero keeps its sign. Infinity and NaN are left as they are.  @param decimal    The decimal, rounded in place; left unchanged on failure. @param position   The power of ten of the increment's unit, within 10^±999999. @param increment  1, 2, 5 or 25 (ECMA-402's 10, 20, 50, 250 and larger are these at a higher position); 0 means 1. @param mode       How a value between two multiples rounds. @return mloc_success; mloc_errorCapacity when the rounded value passes 10^999999; mloc_errorInvalid for a null pointer, a decimal the library did not make, or another position, increment or mode. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocShowFractionDigits(mlocDecimal* decimal, int32_t count);
```
Sets the fraction digits a decimal shows, never fewer than its digits need: 1.5 shows 1.500 with three and 1.50 shows 1.5 with zero. The value is unchanged; mlocRoundDecimal removes digits.  @param decimal  The decimal, changed in place; left unchanged on failure. @param count    The fraction digits to show, 0 to 999999. @return mloc_success, also for infinity and NaN, which show none; mloc_errorInvalid for a null pointer, a decimal the library did not make, or a count out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetPluralOperands(const mlocDecimal* decimal, mlocPluralOperands* operandsOut);
```
Computes the plural operands of a finite decimal; c is 0.  @param decimal      The decimal. @param operandsOut  Receives the operands. @return mloc_success; mloc_errorInvalid for a null pointer, a decimal that is not finite, or one the library did not make. @par Thread safety Safe from any thread.

## `list.h`

Lists (UTS #35 Part 2, records mloc-0026 and mloc-0035): items joined by a locale's list patterns ("a, b, and c", "a, b o c"), as ICU4C's ListFormatter and ECMA-402's Intl.ListFormat join them, with ICU's contextual changes (Spanish "e" and "u", Hebrew's vav before a word of another script). The patterns are compiled in.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitListFormatter(mlocListFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocListOptions* options);
```
Makes a locale's list formatter of a type and width. The patterns are the first the locale's chain has, root's last.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the list units, the parents and the likely subtags. @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return mloc_success; mloc_errorNotFound for data without the list units; mloc_errorInvalid for a null pointer or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatList(const mlocListFormatter* formatter, const mlocListItem* items, size_t count, mlocOutput* output);
```
Writes items joined by a formatter's patterns: none as no text, one as itself, two by the locale's pattern for two, more by its start, middle and end patterns. Parts are mlocListPart values.  @param formatter  A formatter mlocInitListFormatter made. @param items      The items, count of them; may be null when count is 0. @param count      The number of items, at most UINT32_MAX. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a formatter the library did not make, a null pointer, an item with a null text and a length, or a count past UINT32_MAX. @par Thread safety Safe from any thread.

## `locale.h`

Locale identifiers (UTS #35 Part 1): a parsed locale is a fixed-size value in canonical syntax (record mloc-0015).

```c
MLOC_NODISCARD MLOC_API mlocTextResult mlocParseLocale(const char* text, size_t length, mlocLocale* localeOut);
```
Parses a locale id strictly and puts it in canonical syntax: case, variants and attributes in order, extensions by their singleton with private use last, keys in order, a "u" value "true" removed. Aliases are not replaced.  @param text       The id, with "-" or "_" between subtags. @param length     Its length in bytes. @param localeOut  Receives the locale; left unchanged on failure. @return status mloc_success with offset the length; mloc_errorSyntax with the offset of the subtag at fault for text that is no locale id, a tag that begins with a script, and duplicate variants, extensions, keys or attributes; mloc_errorCapacity when the variants and extensions pass 112 bytes or the tag has more than 64 subtags; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatLocale(const mlocLocale* locale, mlocOutput* output);
```
Writes a locale as its BCP 47 tag in canonical syntax, such as "sr-Latn-RS" or "de-DE-u-co-phonebk". It reports no parts.  @param locale  A locale from mlocParseLocale. @param output  Where the tag goes. @return mloc_success; mloc_errorCapacity with the length needed; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocTextResult mlocCanonicalizeLocale(const mlocDataSet* data, const char* text, size_t length, mlocLocale* localeOut, mlocLocaleChanges* changesOut);
```
Parses a BCP 47 tag or CLDR locale id and canonicalizes it as UTS #35 Annex C does: a legacy tag replaced whole, an extlang removed, "und-" put before a tag that begins with "x" or a script, then CLDR's language, script, region and variant aliases applied until none matches ("iw" gives "he", "sh" gives "sr-Latn", "SU" gives the region of the language's likely one), then the extensions: a t extension's language the same way, u and t values by the bcp47 data's aliases ("ms-imperial" gives "ms-uksystem"), and the subdivisions of "sd" and "rg" by theirs ("rg-fi01" gives "rg-axzzzz"). The syntax is made canonical as mlocParseLocale does.  @param data        The data with the aliases and likely subtags. @param text        The tag. @param length      Its length in bytes, below 256. @param localeOut   Receives the locale; left unchanged on failure. @param changesOut  Receives what was replaced; may be null. @return status mloc_success; mloc_errorSyntax with an offset into the text as mlocParseLocale; mloc_errorCapacity for a text of 256 bytes or more, or a result that does not fit; mloc_errorNotFound for data without the aliases (or, for a tag with a u or t extension, the extension aliases); mloc_errorData for aliases that never stop replacing, which only a crafted data file has; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMaximizeLocale(const mlocDataSet* data, const mlocLocale* locale, mlocLocale* localeOut);
```
Adds likely subtags (UTS #35): "zh-TW" gives "zh-Hant-TW", "und" gives "en-Latn-US". A script "Zzzz" and a region "ZZ" are dropped first; the variants and extensions are kept. Aliases are not replaced, so give a canonicalized locale.  @param data       The data to read the likely subtags from. @param locale     The locale. @param localeOut  Receives the result; may be locale itself; left unchanged on failure. @return mloc_success; mloc_errorNotFound when the data has no likely subtags for the locale or has no likely subtags table; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMinimizeLocale(const mlocDataSet* data, const mlocLocale* locale, mlocFavor favor, mlocLocale* localeOut);
```
Removes likely subtags (UTS #35): the shortest of the language alone, then with its region or script (in the order favor gives), that adds back to the same locale; "zh-Hant-TW" gives "zh-TW", or "zh-Hant" when the script is favored. The variants and extensions are kept.  @param data       The data to read the likely subtags from. @param locale     The locale. @param favor      mloc_favorRegion or mloc_favorScript. @param localeOut  Receives the result; may be locale itself; left unchanged on failure. @return mloc_success; mloc_errorNotFound as mlocMaximizeLocale; mloc_errorInvalid for a null pointer or another favor. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetParentLocale(const mlocDataSet* data, mlocComponent component, const mlocLocale* locale, mlocLocale* parentOut);
```
Returns the next locale of a locale's fallback chain in a component (record mloc-0016): the locale is first put in key form, with the script likely subtags give it and left out when it is its language's own ("zh-TW" is zh-Hant-TW, "sr-Cyrl-BA" is sr-BA); then its next key is CLDR's explicit parent ("en-AU" gives "en-001", "es-MX" "es-419"), else the last variant, the region or the script dropped, where in the main component a script gives root ("zh-Hant" gives "und", never "zh"). The extensions are kept. Root is "und".  @param data       The data with the parents and likely subtags. @param component  The component whose parents apply. @param locale     The locale. @param parentOut  Receives the parent; left unchanged on failure. @return mloc_success; mloc_errorNotFound for root, which has no parent, or data without the parents table; mloc_errorCapacity when the extensions do not fit after the parent's variants; mloc_errorInvalid for a null pointer or another component. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocPrepareMatch(const mlocDataSet* data, const mlocLocale* supported, size_t count, const mlocLocale* defaultLocale, mlocMatchEntry* entriesOut);
```
Prepares supported locales for mlocMatchLocales (record mloc-0017): maximizes each as matching does and orders them as they are tried, those equivalent to the default locale first, then CLDR's paradigm locales (en, en-GB, es, es-419, pt-BR, pt-PT), then the rest, each group in the order given. Give canonicalized locales (mlocCanonicalizeLocale): matching replaces no aliases.  @param data           The data with the likely subtags and the matching table. @param supported      The supported locales. @param count          Their number, below 2^31. @param defaultLocale  The locale whose equivalents are tried first, as ICU's default locale: usually supported[0]; null for none. @param entriesOut     Receives count entries; left unchanged on failure. @return mloc_success; mloc_errorNotFound for data without the likely subtags or the matching table; mloc_errorInvalid for a null pointer with a count, or a count too large. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMatchLocales(const mlocDataSet* data, const mlocLocale* supported, const mlocMatchEntry* entries, size_t count, const mlocLocale* desired, size_t desiredCount, const mlocMatchOptions* options, mlocMatch* matchOut);
```
Finds the supported locale that best matches the desired locales in their order of preference, as ICU's LocaleMatcher does (record mloc-0017). Each desired locale is maximized and each supported one gets the distance CLDR's rules give its language, script and region (regions by groups: "es-MX" is nearer "es-419" than "es", "en-IN" nearer "en-GB" than "en-US"); the nearest wins, a tie going to the one tried first unless a later one has its language's likely script or region. Each later desired locale is demoted by the distance of two English regions, so that it wins only with a closer match.  @param data          The data mlocPrepareMatch was given. @param supported     The supported locales mlocPrepareMatch was given; private use tags are compared with them. @param entries       What mlocPrepareMatch made of them. @param count         Their number. @param desired       The desired locales, the most wanted first, canonicalized. @param desiredCount  Their number, below 2^31. @param options       The options; null for none. @param matchOut      Receives the outcome; left unchanged on failure. @return mloc_success, also when nothing matches; mloc_errorNotFound as mlocPrepareMatch; mloc_errorInvalid for a null pointer with a count, a count too large, another flag, a threshold above 100, or an entry the search reads that names no locale of the count (entries prepared for another list). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocCombineMatch(const mlocLocale* desired, const mlocLocale* supported, mlocLocale* localeOut);
```
Makes the locale to use from a match, as ICU's resolved locale: the supported locale with the desired one's region, its variants (which replace the supported ones) and its extensions (each replacing the supported one of its letter): "ar-SA-u-nu-latn" matched with "ar-EG" gives "ar-SA-u-nu-latn".  @param desired    The desired locale of the match. @param supported  The supported locale of the match. @param localeOut  Receives the locale; may be either input; left unchanged on failure. @return mloc_success; mloc_errorCapacity when the variants and extensions pass 112 bytes; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetLocalePreferences( const mlocDataSet* data, const mlocLocale* locale, mlocLocalePreferences* preferencesOut);
```
Returns a locale's regional preferences (UTS #35), the region's values from CLDR's supplemental data. The region is the -u-rg key's ("en-GB-u-rg-uszzzz" takes the US's), else the locale's region, else the -u-sd key's, else the likely one ("en" takes the US's), else 001; a region without data takes 001's. Keys override: -u-hc the hour cycle (c12 and c24 resolve to the cycle the allowed formats list first), -u-fw the first day, -u-ms the measurement system; -u-ca-iso8601 begins weeks on Monday with four days in the first unless -u-fw or -u-rg give the first day. The hour data is looked up for the language in the region first (ca-ES has its own). A key with a value it does not take is left out.  @param data          The data with the preferences and likely subtags. @param locale        The locale, canonicalized. @param preferencesOut Receives the preferences; left unchanged on failure. @return mloc_success; mloc_errorNotFound for data without the preferences; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

## `names.h`

Display names (UTS #35 Part 2, records mloc-0036 to mloc-0039): a locale's name for a language, script, region, variant, key, key's value or measurement system, as ICU4C's LocaleDisplayNames and ECMA-402's Intl.DisplayNames give them, for a subdivision, and for a whole locale. The names are in a data file of their own, names.mloc, and the subdivision names in another, subdivisions.mloc, which a program adds to its data set.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetName(const mlocDataSet* data, const mlocLocale* locale, mlocNameType type, const char* code, size_t length, const mlocNameOptions* options, mlocOutput* output);
```
Writes a locale's name for a code: the form's along the locale's chain, else the standard name along the chain, as ICU4C's LocaleDisplayNames looks it up.  @param data     The data with the names (names.mloc, or subdivisions.mloc for a subdivision) and the parents. @param locale   The locale, canonicalized. @param type     What the code names. @param code     The code, in any case. @param length   Its length in bytes. @param options  The options, or null for the defaults. @param output   Where the name goes (record mloc-0015); no parts. @return mloc_success; mloc_fellBack when the code, in canonical case, is written for a name the locale's chain lacks; mloc_errorNotFound for a missing name with mloc_nameFallbackNone, or data without the names; mloc_errorSyntax for a code not of the type's syntax; mloc_errorCapacity when the name does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a part function, or a type or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetLocaleName(const mlocDataSet* data, const mlocLocale* displayLocale, const mlocLocale* locale, const mlocLocaleNameOptions* options, mlocOutput* output);
```
Writes a locale's name in another locale by UTS #35's Locale Display Name Algorithm, as CLDR's own tools build it: the language's name (with dialect names the longest match of its script, region and variants), then by the locale's pattern the names of the parts it left out ("Latin", "Canada"), of the u keys ("Buddhist Calendar", "Currency: €"), of the t extension ("Transform: Russian, Cyrillic") and its fields, and the other extensions and private use as written ("x: abc"), joined by the locale's separator, brackets inside a name nested ("Myanmar [Burma]"). The sd and rg values are named as subdivisions ("Region Subdivision: Scotland"). A part without a name stands as written; time zones stand as written until their names are in the data.  @param data           The data with the names (names.mloc), the parents and, for -u-cu, the currency symbols; for -u-sd and -u-rg, subdivisions.mloc too. @param displayLocale  The locale the name is in, canonicalized. @param locale         The locale named, canonicalized. @param options        The options, or null for the defaults. @param output         Where the name goes (record mloc-0015); no parts. @return mloc_success; mloc_fellBack when the language has no name and its code stands; mloc_errorNotFound for data without the names; mloc_errorCapacity when the name does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a part function, or options out of range. @par Thread safety Safe from any thread.

## `number.h`

Number formatting (UTS #35 Part 3, record mloc-0021): a decimal in a locale's digits, symbols and grouping, rounded as the options say. The options follow ECMA-402's NumberFormat; their zero value is the default, and the zero precision rounds nothing. A formatter also reads a number a person types (record mloc-0032).

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitNumberFormatter(mlocNumberFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocNumberOptions* options);
```
Makes a locale's number formatter. The numbering system is the locale's -u-nu keyword ("native", "traditio" and "finance" as the locale names them), else the locale's default; one without digits (Roman, for one) takes the default, which the spellout part formats instead.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number units, the parents and the likely subtags. @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead (the locale's data is always found along its chain, root's last, so whether a locale is supported is a question for language matching); mloc_errorNotFound for data without numbers; mloc_errorInvalid for a null pointer or options out of range or in conflict. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatNumber(const mlocNumberFormatter* formatter, const mlocDecimal* decimal, mlocOutput* output);
```
Formats a decimal.  @param formatter  A formatter from mlocInitNumberFormatter. @param decimal    The number. @param output     Receives the text; its part function, when set, receives mlocNumberPart values. @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding passes the decimal's limits; mloc_errorInvalid for a null pointer, or a formatter or decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatNumberRange(const mlocNumberFormatter* formatter, const mlocDecimal* start, const mlocDecimal* end, mlocOutput* output);
```
Formats a range of two decimals (ECMA-402's formatRange): both ends by the formatter, around the locale's range separator, spaced when the ends have affixes of their own ("3–5", "-5 – -3", "3K – 5K"), an affix both share written once when it is longer than one character ("3–5 thousand"); ends that write alike give one number with the approximately sign ("~5"). The start may pass the end.  @param formatter  A formatter from mlocInitNumberFormatter. @param start      The range's start. @param end        Its end. @param output     Receives the text; its part function, when set, receives mlocNumberPart values with the end each belongs to (mloc_numberRangeStart, mloc_numberRangeEnd, or 0 for both). @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding passes the decimal's limits; mloc_errorInvalid for a null pointer, a NaN end, or a formatter or decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseNumber(const mlocNumberFormatter* formatter, const char* text, size_t length, mlocParseMode mode, mlocDecimal* decimalOut, size_t* endOut);
```
Reads a number as a person types it in the formatter's locale (ICU's DecimalFormat::parse): its digits, any Unicode decimal digits, its separators, signs, pattern affixes, percent or per mille sign (the value then divided by 100 or 1000, as the style multiplies it), NaN, infinity, and an exponent. Reading starts at the first byte and stops where no matcher reads more: "12 apples" gives 12, ending at 2; compare the end with the length to require the whole text. The decimal is exact, its fraction digits those typed (English "1.50" gives 1.50).  @param formatter   A formatter from mlocInitNumberFormatter, of standard, scientific or engineering notation. @param text        UTF-8 text; it need not end in a 0 byte. @param length      Its length in bytes. @param mode        An mlocParseMode. @param decimalOut  Receives the number read. @param endOut      Receives the offset after the number read, or on mloc_errorSyntax the offset reading stopped at. @return mloc_success; mloc_errorSyntax when no number is read (or, in strict mode, the affixes or grouping do not match); mloc_errorCapacity when the digits or the exponent pass the decimal's limits; mloc_errorNotFound for data without the parse sets; mloc_errorInvalid for a null pointer, a mode out of range, a formatter the library did not make or one of compact notation. @par Thread safety Safe from any thread.

## `plural.h`

Plural rules (UTS #35 Part 3, record mloc-0020): the category a number takes in a locale, cardinal ("1 file", "2 files") or ordinal ("1st", "2nd"), and that of a range of two numbers.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitPluralRules(mlocPluralRules* rulesOut, const mlocDataSet* data, const mlocLocale* locale, mlocPluralType type);
```
Finds a locale's plural rules along its chain in the plurals component: "pt-PT" has its own, "en-GB" takes "en"'s, and a language CLDR does not name takes root's (other for everything).  @param rulesOut  Receives the rules; left unchanged on failure. @param data      The data with the plural units and the parents. @param locale    The locale, canonicalized. @param type      Cardinal or ordinal. @return mloc_success; mloc_fellBack when the rules are those of a locale it falls back to; mloc_errorNotFound for data without them; mloc_errorInvalid for a null pointer or another type. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocSelectPlural(const mlocPluralRules* rules, const mlocDecimal* decimal, mlocPluralCategory* categoryOut);
```
Selects the category of a decimal, by the digits it shows: in English 1 is one and 1.0 is other. Infinity and NaN are other.  @param rules        Rules from mlocInitPluralRules. @param decimal      The number. @param categoryOut  Receives the category. @return mloc_success; mloc_errorInvalid for a null pointer, or rules or a decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocSelectPluralInteger(const mlocPluralRules* rules, int64_t value, mlocPluralCategory* categoryOut);
```
Selects the category of an integer, as that of its decimal with no fraction digits, without making the decimal.  @param rules        Rules from mlocInitPluralRules. @param value        The integer. @param categoryOut  Receives the category. @return mloc_success; mloc_errorInvalid for a null pointer or rules the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocSelectPluralOperands(const mlocPluralRules* rules, const mlocPluralOperands* operands, mlocPluralCategory* categoryOut);
```
Selects the category of plural operands, for a compact number whose exponent (c) the caller sets: in French 1c6 ("1 M") is many.  @param rules        Rules from mlocInitPluralRules. @param operands     The operands, as mlocGetPluralOperands gives them for the whole value, with c set. @param categoryOut  Receives the category. @return mloc_success; mloc_errorInvalid for a null pointer or rules the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocSelectPluralRange(const mlocPluralRules* rules, mlocPluralCategory start, mlocPluralCategory end, mlocPluralCategory* categoryOut);
```
Selects the category of a range from its ends' categories, by the locale's table (English days from 1 to 2 are other); where CLDR has no entry, the end's category.  @param rules        Cardinal rules from mlocInitPluralRules. @param start        The category of the range's start. @param end          The category of its end. @param categoryOut  Receives the category. @return mloc_success; mloc_errorInvalid for a null pointer, ordinal rules, rules the library did not make, or another category. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetPluralCategories(const mlocPluralRules* rules, uint32_t* categoriesOut);
```
Gives the categories a locale's rules use, as bits 1 << category: one and other for English cardinals, all six for Arabic's.  @param rules          Rules from mlocInitPluralRules. @param categoriesOut  Receives the bits. @return mloc_success; mloc_errorInvalid for a null pointer or rules the library did not make. @par Thread safety Safe from any thread.

## `spellout.h`

Numbers in words, ordinal suffixes and the algorithmic numbering systems (UTS #35 Part 3, record mloc-0034): CLDR's rule-based number formats, as ICU4C reads and runs them, on the exact decimal. The rules are in a data file of their own, spellout.mloc, which a program adds to its data set.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitSpelloutFormatter( mlocSpelloutFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const char* ruleSet, size_t length, const mlocSpelloutOptions* options);
```
Makes a locale's formatter of a rule set: words ("%spellout-cardinal", "%spellout-ordinal-feminine", "%spellout-numbering-year"), ordinal suffixes ("%digits-ordinal") or a numbering system's rules ("%roman-upper"). The rules are the first the locale's chain has of each group (spell-out, ordinal suffixes, numbering systems), a group taken whole as ICU takes it.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number units, the plural rules and the spell-out rules (spellout.mloc). @param locale        The locale, canonicalized. @param ruleSet       A public rule set's name, with its "%" as CLDR writes it, or null for the locale's default (ICU's: "%spellout-numbering"). @param length        The name's length in bytes, 0 with null. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the locale has no rules of its own and root's, which write digits, stand instead, or when the numbering system asked for has no digits and the default writes the digits of patterns; mloc_errorNotFound for a rule set the locale's rules do not have, or data without the spell-out rules or the number units; mloc_errorInvalid for a null pointer, a length with a null name, a private rule set's name ("%%..."), or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitNumberingSystemFormatter( mlocSpelloutFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const char* system, size_t length, const mlocSpelloutOptions* options);
```
Makes a formatter of an algorithmic numbering system by its CLDR id ("roman", "hebr", "hans", "jpanyear"), as ICU's NumberFormat makes one: the rules of the locale the system names, with that locale's plural rules and symbols ("hans" is zh's "%spellout-cardinal"), or for a system of root's rules ("roman", "armn", "geor") the locale asked for.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number units, the plural rules and the spell-out rules (spellout.mloc). @param locale        The locale, canonicalized. @param system        The numbering system's id. @param length        Its length in bytes. @param options       The options, or null for the defaults. @return mloc_success; mloc_errorNotFound for an id not of an algorithmic system (the numeric ones, "latn" or "arab", are the number formatter's), or data without the spell-out rules or the number units; mloc_errorInvalid for a null pointer or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatSpellout(const mlocSpelloutFormatter* formatter, const mlocDecimal* decimal, mlocOutput* output);
```
Formats a decimal by the formatter's rules: its value, not the digits it shows ("1.10" is written as 1.1, "-0" as 0), exactly (ICU4C's results wherever its int64_t or double holds the value; long fractions and values past int64_t are written exactly where ICU loses digits). Nothing is rounded: round the decimal first (mlocRoundDecimal).  @param formatter  A formatter from mlocInitSpelloutFormatter or mlocInitNumberingSystemFormatter. @param decimal    The value. @param output     Receives the text; on mloc_errorCapacity its length is the size needed. The text has no parts. @return mloc_success; mloc_errorCapacity for a buffer too small, or rules that nest past 64 levels for a value (a value far past the rules' largest base) or would divide it to more digits than a decimal holds; mloc_errorInvalid for a null pointer, an invalid decimal, an output asking for parts, or a formatter the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseSpellout(const mlocSpelloutFormatter* formatter, const char* text, size_t length, mlocDecimal* decimalOut, size_t* endOut);
```
Reads a number written in words (or ordinal suffixes, or a numbering system's numerals) back, as ICU4C's RuleBasedNumberFormat parses it, strictly: every public rule set of the formatter's group is tried and the longest reading kept, the first of equal ones, so a cardinal formatter also reads ordinals and years. The text must be written as the rules write it (case, spaces and hyphens as they are); digits in a rule's pattern read as the strict number parser reads them. Reading starts at the first byte and stops where no rule reads more: compare the end with the length to require the whole text. The value is exact (ICU's is a double).  @param formatter   A formatter from mlocInitSpelloutFormatter or mlocInitNumberingSystemFormatter. @param text        UTF-8 text; it need not end in a 0 byte. @param length      Its length in bytes. @param decimalOut  Receives the value read: finite, NaN or an infinity; zero is never negative. @param endOut      Receives the offset after the text read, 0 when none is. @return mloc_success; mloc_errorSyntax when no rule reads anything (or the rules are of a kind ICU refuses to parse); mloc_errorCapacity when a value read passes a decimal's digits, or text crafted against the search makes it take more than 4,194,304 steps; mloc_errorInvalid for a null pointer or a formatter the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API size_t mlocGetSpelloutRuleSetCount(const mlocDataSet* data, const mlocLocale* locale);
```
The number of public rule sets a locale's spell-out and ordinal suffix rules have, the names mlocGetSpelloutRuleSet lists.  @param data    The data with the spell-out rules. @param locale  The locale, canonicalized. @return The count; 0 for a null pointer or data without the rules. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetSpelloutRuleSet(const mlocDataSet* data, const mlocLocale* locale, size_t index, const char** nameOut, size_t* lengthOut);
```
A public rule set's name, with its "%": the spell-out rules' in their order, then the ordinal suffix rules'.  @param data       The data with the spell-out rules. @param locale     The locale, canonicalized. @param index      The rule set's index, below mlocGetSpelloutRuleSetCount. @param nameOut    Receives the name, in the data, not NUL-terminated. @param lengthOut  Receives its length in bytes. @return mloc_success; mloc_errorNotFound for an index past the last or data without the rules; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

## `unit.h`

Units of measurement (UTS #35 Parts 2 and 6, records mloc-0023 to mloc-0031): CLDR's unit identifiers ("kilometer-per-hour", "square-foot", "foot-and-inch") and exact conversion between them by CLDR's units.xml, the units a locale prefers for a usage, and amounts of a unit formatted with its name in a locale. The names are in a data file of their own, units.mloc, which a program adds to its data set.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocConvertUnit(const mlocDataSet* data, const char* from, size_t fromLength, const char* to, size_t toLength, const mlocPrecision* precision, const mlocDecimal* value, mlocDecimal* valueOut);
```
Converts a value from one unit to another of the same dimensions ("mile" to "kilometer", "foot-per-minute" to "mile-per-hour") or of the opposite ones ("mile-per-gallon" to "liter-per-100-kilometer", as UTS #35's reciprocals). The units are CLDR core unit identifiers: simple units with SI and binary prefixes, powers, products, "per" and unit constants ("per-100-kilometer"); deprecated ones are replaced ("metric-ton" is "tonne"). The conversion is exact, by CLDR's factors as fractions, and the value is rounded once, by the precision. A temperature alone converts with its offset ("celsius" to "fahrenheit"); "beaufort" by UTS #35's force ranges.  @param data        The data with the unit table. @param from        The unit of the value. @param fromLength  Its length in bytes. @param to          The unit wanted. @param toLength    Its length in bytes. @param precision   How to round, or null: the zero precision gives an exact result as it is and any other rounded half even to 38 significant digits. A precision that keeps more than 38 significant digits of an inexact result shows digits past the 38th that may be one unit off. @param value       The value. @param valueOut    Receives the converted value; left unchanged on failure. @return mloc_success; mloc_errorSyntax for an identifier that is not well-formed; mloc_errorNotFound for a simple unit the data does not know, a private-use unit, or data without the unit table; mloc_errorInvalid for a null pointer, an invalid precision or decimal, a mixed unit, or units that do not convert to each other (a currency unit converts to none); mloc_errorCapacity when a unit's size or the value passes what the exact integers (2,048 bits) or a decimal hold. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetUnitPreference( const mlocDataSet* data, const mlocLocale* locale, const char* unit, size_t unitLength, const char* usage, size_t usageLength, const mlocDecimal* value, const mlocPrecision* precision, mlocOutput* unitOut, mlocUnitAmounts* amountsOut);
```
Chooses the unit a locale prefers for a value of a usage ("road", "person-height"; UTS #35 Part 6, Unit Preferences) and converts the value to it. A valid -u-mu key ("celsius", "fahrenhe", "kelvin") that converts from the unit decides alone. Otherwise the unit's quantity (by its base unit: its quantity, the reciprocal's, then the same with numerator and denominator cancelled) selects CLDR's preferences for the usage, falling back by dropping the usage's last part, then to "default"; the region is the one mlocGetLocalePreferences takes, its preferences or else 001's, and -u-ms ("metric", "ussystem", "uksystem") moves to 001, the US or Great Britain when a unit there is of another system. The first unit whose threshold the value reaches, in that unit, is chosen (NaN reaches every one). A quantity without preferences takes its base unit; a unit of no quantity, its base units ("candela-per-bit" for "candela-per-byte"). A mixed unit's value is split exactly: whole larger parts, the rest in the last, which alone is rounded; a part that rounds up to the next part's size carries into it.  @param data        The data with the unit table, the unit preferences and the likely subtags. @param locale      The locale, canonicalized. @param unit        The value's unit, a core unit identifier. @param unitLength  Its length in bytes. @param usage       The usage, or null with length 0 for "default". @param usageLength Its length in bytes. @param value       The value. @param precision   How to round the last amount, or null not to round (as mlocConvertUnit's zero precision: exact, or 38 significant digits). The zero precision takes the preference's increment when it has one, else an integer of at least two significant digits, as ICU does. @param unitOut     Receives the chosen unit's identifier. @param amountsOut  Receives the amounts; left unchanged on failure. @return mloc_success; mloc_errorCapacity when the identifier does not fit unitOut (its length is then the length needed) or a value passes the exact integers; mloc_errorSyntax for a unit or usage that is not well-formed; mloc_errorNotFound for a simple unit the data does not know or data without the tables; mloc_errorInvalid for a null pointer, an invalid decimal or precision, or a unit that is mixed or does not convert. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitUnitFormatter(mlocUnitFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const char* unit, size_t unitLength, const mlocUnitOptions* options);
```
Makes a locale's formatter of a unit: the number as for numbers, in the unit's pattern for the plural category of the number as rounded ("1 meter", "2 meters"), a category the locale lacks taking the other category's. A pattern may leave the number out (Arabic's dual names), as CLDR's data does. A unit CLDR does not name ("kilometer-per-second", "square-millimeter") takes a name built from the locale's compound patterns as ICU builds it, its identifier normalized first, as ICU normalizes it. A mixed unit ("foot-and-inch") takes an amount of its largest part and writes each part ("5 ft, 6 in"), joined by the locale's unit list of the width: the larger parts whole, as the locale's default number format writes them, the smallest with the options, and the first part given with the amount's sign. With a usage, the amount, of the unit given, is written in the unit the locale prefers for its value, as mlocGetUnitPreference chooses and rounds it (a precision in the options wins over the preference's), mixed outputs as above. With a coordinate, an angle is written with the locale's direction for its sign and the width ("12.5°N"; numeric takes narrow's), the amount unsigned whatever the sign display. With a grammatical case and the long width, each name is the case's where the locale's chain has it, as ICU's unitDisplayCase reads it, a built name taking it through CLDR's case derivations.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number units, the plural rules and the unit names (units.mloc). @param locale        The locale, canonicalized. @param unit          A core unit identifier ("meter", "kilometer-per-hour", "square-millimeter") or a mixed unit ("foot-and-inch"). @param unitLength    Its length in bytes. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorSyntax for an identifier UTS #35 does not allow; mloc_errorNotFound for a unit the names do not know (a simple unit without names, a power past three) or data without the number units, the unit table, the names, the unit patterns, the lists or, for a case other than the nominative, the case patterns; mloc_errorCapacity for a unit whose patterns pass the formatter's room; mloc_errorInvalid for a null pointer, an empty unit, a currency, a constant in the numerator, a name ICU refuses to build (the number inside a part), a mixed unit whose parts do not convert to each other alone (other dimensions, a temperature) or are of one size, or with a notation other than standard, a usage with a mixed unit, one that does not convert or a notation other than standard, a coordinate of a unit not an angle or with a usage, or options out of range (a case past mloc_caseVocative) or in conflict (a percent style, a null usage of a length). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatUnit(const mlocUnitFormatter* formatter, const mlocDecimal* decimal, mlocOutput* output);
```
Formats an amount of the formatter's unit (of a mixed unit's largest part, split exactly: whole larger parts, the rest in the smallest, rounded, a part that rounds up to the next carrying; NaN and the infinities in the largest part, the others 0).  @param formatter  A formatter from mlocInitUnitFormatter. @param decimal    The amount. @param output     Receives the text; its part function, when set, receives mlocNumberPart values, each unit name as mloc_numberPartUnit and a unit list's text as mloc_numberPartLiteral. @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding or a part passes the decimal's limits; mloc_errorInvalid for a null pointer, or a formatter or decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatUnitRange(const mlocUnitFormatter* formatter, const mlocDecimal* start, const mlocDecimal* end, mlocOutput* output);
```
Formats a range of two amounts, as mlocFormatNumberRange does, the unit written once in the category CLDR's plural ranges give the ends' ("3–5 meters"), as ICU collapses a unit. With a usage, ends whose values choose one output unit, not mixed, collapse so ("30–70 ft"); others are each written whole, joined by the spaced range separator ("30 ft – 1.2 mi").  @param formatter  A formatter from mlocInitUnitFormatter. @param start      The range's start. @param end        Its end. @param output     Receives the text; its part function, when set, receives mlocNumberPart values with the end each belongs to (mloc_numberRangeStart, mloc_numberRangeEnd, or 0 for both). @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding passes the decimal's limits; mloc_errorInvalid for a null pointer, a NaN end, a formatter of a mixed unit without a usage (which ICU does not range either), a formatter of a coordinate, or a formatter or decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetUnitDisplayName(const mlocDataSet* data, const mlocLocale* locale, const char* unit, size_t unitLength, mlocUnitWidth width, mlocOutput* output);
```
Writes a unit's display name in a locale ("meters", "m"), as a menu of units shows it.  @param data        The data with the unit names. @param locale      The locale, canonicalized. @param unit        A unit CLDR names, by any identifier of it ("metric-ton" for "tonne"). @param unitLength  Its length in bytes. @param width       The width; numeric writes narrow's name. @param output      Receives the text. @return mloc_success; mloc_fellBack when no locale of the chain names it and the identifier is written instead; mloc_errorCapacity when the text does not fit (output->length is then the length needed); mloc_errorSyntax for an identifier UTS #35 does not allow; mloc_errorNotFound for a unit the names do not know (a compound CLDR does not name), data without them, or a chain that cannot be walked; mloc_errorInvalid for a null pointer, an empty unit, a mixed unit, a currency, a constant in the numerator or a width out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetUnitGender(const mlocDataSet* data, const mlocLocale* locale, const char* unit, size_t unitLength, mlocGender* genderOut);
```
Gives a unit's grammatical gender in a locale, the one a message agreeing with a formatted amount needs, as ICU's FormattedNumber::getNounClass gives it: a unit CLDR names has its long name's ("Meter", masculine in German), else in a language whose units have genders what CLDR's gender derivations make of its parts ("kilometer-per-hour" takes kilometer's, a prefix and a power keeping meter's). A language without genders, and a mixed unit, give mloc_genderNone.  @param data        The data with the unit table and the unit names. @param locale      The locale, canonicalized. @param unit        A core unit identifier or a mixed unit. @param unitLength  Its length in bytes. @param genderOut   Receives the gender; left unchanged on failure. @return mloc_success; mloc_errorSyntax for an identifier UTS #35 does not allow; mloc_errorNotFound for a simple unit the table does not know, or data without the unit table or the names; mloc_errorCapacity for an identifier whose normal form passes 255 bytes; mloc_errorInvalid for a null pointer, an empty unit, a currency or a constant in the numerator. @par Thread safety Safe from any thread.

---

66 functions across 12 headers.
