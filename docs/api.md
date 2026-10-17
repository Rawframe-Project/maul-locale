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

Calendars (UTS #35 Part 4, record mloc-0040): dates as values in CLDR's calendars, as Temporal models them. A date is a day, counted from January 1 of year 1 in the proleptic Gregorian calendar (day 1), read in a calendar; its fields (year, era, month code, day) are computed from it.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeDate(mlocCalendar calendar, const mlocDateInput* input, mlocOverflow overflow, mlocDate* dateOut);
```
Makes a date from its fields in a calendar, as Temporal's PlainDate.from does: a month or day past the calendar's is clamped to the last (mloc_overflowConstrain) or refused (mloc_overflowReject), and so is a leap month the year lacks (made the month after it); a month code no year of the calendar has, an unknown era, or a month or day below 1 is refused either way.  @param calendar  The calendar. @param input     The fields. @param overflow  What a month or day past the calendar's does. @param dateOut   Receives the date; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a value out of range or refused, or a date outside Temporal's range; mloc_errorSyntax for an era or month code not of the calendar. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeDateFromDay(mlocCalendar calendar, int32_t day, mlocDate* dateOut);
```
Makes the date of a day in a calendar.  @param calendar  The calendar. @param day       The day, 1 for January 1 of year 1 in the proleptic Gregorian calendar. @param dateOut   Receives the date; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a calendar out of range, or a day outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetDateFields(const mlocDate* date, mlocDateFields* fieldsOut);
```
Computes a date's fields in its calendar.  @param date       The date. @param fieldsOut  Receives the fields. @return mloc_success; mloc_errorInvalid for a null pointer or a date the library did not make (a day out of range, an unknown calendar, reserved bytes not zero). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocAddToDate(const mlocDate* date, const mlocDateDuration* duration, mlocOverflow overflow, mlocDate* dateOut);
```
Adds a duration to a date in its calendar, as Temporal's PlainDate.add does: the years first, keeping the month code (a leap month the new year lacks is clamped as mlocMakeDate clamps it, or refused), then the months, then the day clamped to its month or refused, then weeks and days as days. Subtracting is adding the negated duration.  @param date      The date. @param duration  The duration. @param overflow  What a month code or day the result lacks does. @param dateOut   Receives the date, in the date's calendar; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date the library did not make, a duration out of its limits or of two signs, a month code or day refused, or a result outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocDateUntil(const mlocDate* one, const mlocDate* two, mlocDateUnit largestUnit, mlocDateDuration* durationOut);
```
Computes the duration from one date to another of the same calendar, as Temporal's PlainDate.until does: the most years whose addition does not pass the second date (judged by year, month code and day), then the most months, then weeks if they are the largest unit, then days; none larger than the largest unit. Temporal's since of the first date and the second is this duration negated (not the duration from the second to the first, which may differ by the clamping of days).  @param one          The date from. @param two          The date to. @param largestUnit  The largest unit to give. @param durationOut  Receives the duration, of one sign; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date the library did not make, dates of two calendars, or an unknown unit. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocDateUntilRounded(const mlocDate* one, const mlocDate* two, const mlocDateRounding* rounding, mlocDateDuration* durationOut);
```
Computes the duration from one date to another of the same calendar and rounds it, as Temporal's PlainDate.until does with rounding options: the difference of mlocDateUntil, then for a smallest unit of years, months or weeks the multiples of the increment around it, added to the first date, with the second date's place between them deciding by mode; for days, the days rounded to the increment. A rounding that reaches a larger unit's next value carries into it, up to the largest unit. Temporal's since is this from the same first date with the mode negated (ceil for floor, half-ceil for half-floor, and back), and the result negated.  @param one          The date from. @param two          The date to. @param rounding     The units, increment and mode. @param durationOut  Receives the duration; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date the library did not make, dates of two calendars, an unknown unit or mode, a smallest unit larger than the largest, an increment out of range, or a multiple whose date falls outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocWithDate(const mlocDate* date, const mlocDateChanges* changes, mlocOverflow overflow, mlocDate* dateOut);
```
Makes a date with some of a date's fields changed, as Temporal's PlainDate.with does: the date's year, month code and day, less the month code where a month or month code is given and less the year where a year, era or era year is given, with the given fields over them, made as mlocMakeDate makes a date.  @param date      The date. @param changes   The fields to change. @param overflow  What a month or day past the calendar's does. @param dateOut   Receives the date; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date the library did not make, an era without its era year or the reverse, a year, month or day refused or disagreeing, or a date outside Temporal's range; mloc_errorSyntax for an era or month code not of the calendar. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeMonthDay(mlocCalendar calendar, const mlocDateChanges* fields, mlocOverflow overflow, mlocDate* referenceOut);
```
Finds a month and day's reference date, as Temporal's PlainMonthDay.from does in the Intl Era and Month Code proposal: a month code (or, with a year, a month or month code of that year) and a day, clamped to the month's longest length (30 in Chinese and Dangi, the given year's month with a year) or refused; then the latest date from 1900 to 1972 in the ISO calendar with that month code and day, else the earliest from 1973 to 2035. Chinese and Dangi take the proposal's table of reference years, a month and day it lacks becoming the month of the same number (or refused).  @param calendar      The calendar. @param fields        A day, and a month code, or a year (or era and era year) and a month or month code. @param overflow      What a month or day past the calendar's does. @param referenceOut  Receives the reference date, in the calendar; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, missing or disagreeing fields (a month's place without a year among them), or a value refused; mloc_errorSyntax for an era or month code not of the calendar. @par Thread safety Safe from any thread.

## `currency.h`

Currency formatting (UTS #35 Part 3, record mloc-0022): an amount of an ISO 4217 currency in a locale's currency pattern, with its symbol or code, rounded to the currency's digits unless the options say otherwise. The options follow ECMA-402's NumberFormat with the currency style; their zero value is the default. Lookups give a currency's symbol, name and digits, and the currency a locale or a region uses on a day.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitCurrencyFormatter( mlocCurrencyFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const char* code, size_t codeLength, const mlocCurrencyOptions* options);
```
Makes a locale's formatter of a currency. The numbering system is the locale's, as for numbers; the locale's -u-cf-account keyword asks for its accounting pattern ("($3.00)") unless the options' sign says otherwise. The symbol meets the digits as UTS #35 prefers: a locale's alphaNextToNumber pattern spaces a symbol whose character next to the digits is a letter ("USD 3.00", "$3.00"). A code the data does not know shows as its code with the default digits.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number and currency units, the parents and the likely subtags. @param locale        The locale, canonicalized. @param code          The currency's ISO 4217 code, three ASCII letters in any case. @param codeLength    Its length in bytes, 3. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorNotFound for data without the number or currency units (or, for names, the currency names); mloc_errorInvalid for a null pointer, a code that is not three letters, or options out of range or in conflict (a percent style, the code appended to a display that is no symbol). @par Thread safety Safe from any thread.

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
Opens a data file held in memory. The whole file is validated here, once; a view that opened is never checked again.  @param dataOut  Receives the view; left unchanged on failure. @param bytes    The file, 8-byte aligned (any allocator's memory is). @param size     The file's size. @return mloc_success; mloc_errorInvalid for a null pointer or bytes that are not 8-byte aligned; mloc_errorDataVersion for a file of another data version (mlocReadDataVersion names it), a zones file's release aside; mloc_errorData for bytes that are not a valid data file. @par Thread safety Safe from any thread.

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

## `datetime.h`

Dates and times formatted (UTS #35 Part 4, record mloc-0042): a date written in a locale by what it should show, a semantic field set at a length ("Jan 5, 2026" for year, month and day, medium), as ICU4X's semantic skeletons and CLDR's datetime conformance data name them, or by the locale's four standard formats; a time of day by its hour cycle and precision ("4:03 PM"), or by a standard format. The patterns were matched to the locale's data by the generator as ICU4C's DateTimePatternGenerator matches them; they and the calendar names are in a data file of their own, dates.mloc, which a program adds to its data set.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitDateFormatter(mlocDateFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocDateOptions* options);
```
Makes a locale's date formatter. The calendar is the locale's (mlocLocalePreferences: -u-ca, else its region's first); the numbering system its -u-nu keyword's, else its default, with the systems a pattern names for its fields (Japanese years by jpanyear, Hebrew dates by hebr).  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the dates (dates.mloc), the number units, the preferences, the parents and, for a pattern naming an algorithmic numbering system, the spell-out rules; with a zone, what mlocInitZoneFormatter reads too. @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorNotFound for data without the dates, the number units or the preferences, or a numbering system the spell-out rules lack, or with a zone without the zone names; mloc_errorData for a pattern the formatter cannot write; mloc_errorInvalid for a null pointer, options out of range, or a zone with a calendar period (a month, a year). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDate(const mlocDateFormatter* formatter, const mlocDate* date, mlocOutput* output);
```
Writes a date by the formatter, read in the formatter's calendar (the same day). Parts: mlocDatePart.  @param formatter  A formatter from mlocInitDateFormatter. @param date       The date. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a date outside Temporal's range, or a formatter the library did not make or made with a zone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateInZone(const mlocDateFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* instant, mlocOutput* output);
```
Writes the date in a zone at an instant, and the zone, by a formatter made with a zone (mlocDateOptions' zone): the zone's local date in the formatter's calendar, the zone as mlocFormatZone writes it. Parts: mlocDatePart.  @param formatter  A formatter from mlocInitDateFormatter, with a zone. @param zone       The zone. @param instant    The instant, in Temporal's range. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a zone the library did not make, an instant out of range, or a formatter the library did not make or made without a zone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateRange(const mlocDateFormatter* formatter, const mlocDate* start, const mlocDate* end, mlocOutput* output);
```
Writes a range of two dates by the formatter (ECMA-402's formatRange), as ICU4C's DateIntervalFormat writes the formatter's skeleton: the locale's interval pattern for the greatest field the dates differ in ("Jan 5 – 8, 2026", "Jan 5 – Feb 8, 2026"), else the locale's fallback around both dates ("1/5/2026 – 1/8/2026"); one date when they differ in no field shown. Both are read in the formatter's calendar and written in the year style the one that needs more asks; the start may pass the end.  @param formatter  A formatter from mlocInitDateFormatter, without a zone. @param start      The range's start. @param end        Its end. @param output     Where the text goes (record mloc-0015); its part function, when set, receives mlocDatePart values with the date each belongs to (mloc_dateRangeStart, mloc_dateRangeEnd, or 0 for both). @return As mlocFormatDate. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateRangeInZone(const mlocDateFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* start, const mlocInstant* end, mlocOutput* output);
```
Writes a range of two instants' dates in a zone, each with the zone, by a formatter made with one: on one local day the start's date and zone; on two, each whole around the fallback, the date by the pattern with the greatest difference's field prefixed where the date lacks it ("Jan 5 at PST – Feb 8 at PST" as "Jan 5, 2026 at PST – Feb 8, 2027 at PST" across years), as ICU4C's DateIntervalFormat writes the date's skeleton with the zone's.  @param formatter  A formatter from mlocInitDateFormatter, with a zone. @param zone       The zone. @param start      The range's start, in Temporal's range. @param end        Its end, likewise. @param output     Where the text goes (record mloc-0015); parts as mlocFormatDateRange's. @return As mlocFormatDateInZone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitTimeFormatter(mlocTimeFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocTimeOptions* options);
```
Makes a locale's time formatter. The patterns are those of the locale's calendar (mlocLocalePreferences), the digits and the decimal separator its numbering system's (-u-nu, else its default).  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the dates (dates.mloc), the number units, the preferences and the parents; with a zone (a zone style, or the full or long format), what mlocInitZoneFormatter reads too. @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorNotFound for data without the dates, the number units or the preferences, or with a zone without the zone names; mloc_errorData for a pattern the formatter cannot write; mloc_errorInvalid for a null pointer, options out of range, or a zone style with a standard format. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTime(const mlocTimeFormatter* formatter, const mlocTime* time, mlocOutput* output);
```
Writes a time of day by the formatter. Parts: mlocDatePart.  @param formatter  A formatter from mlocInitTimeFormatter. @param time       The time. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a time out of range, or a formatter the library did not make or made with a zone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeInZone(const mlocTimeFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* instant, mlocOutput* output);
```
Writes the time of day in a zone at an instant, and the zone, by a formatter made with one (a zone style, or the full or long format): the zone's local time, the zone as mlocFormatZone writes it in the form the pattern names. Parts: mlocDatePart.  @param formatter  A formatter from mlocInitTimeFormatter, with a zone. @param zone       The zone. @param instant    The instant, in Temporal's range. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a zone the library did not make, an instant out of range, or a formatter the library did not make or made without a zone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeRange(const mlocTimeFormatter* formatter, const mlocTime* start, const mlocTime* end, mlocOutput* output);
```
Writes a range of two times of day by the formatter, as ICU4C's DateIntervalFormat writes the formatter's skeleton: the locale's interval pattern for the greatest field they differ in (am or pm, the hour, the minute: "10:00 – 11:30 AM"), else the fallback around both; one time when they differ in no field shown. Under minute-optional precision both take the hour's pattern only when both minutes are zero. The start may pass the end.  @param formatter  A formatter from mlocInitTimeFormatter, without a zone. @param start      The range's start. @param end        Its end. @param output     Where the text goes (record mloc-0015); parts as mlocFormatDateRange's. @return As mlocFormatTime. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeRangeInZone(const mlocTimeFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* start, const mlocInstant* end, mlocOutput* output);
```
Writes a range of two instants' times of day in a zone by a formatter made with one, each with its zone as the pattern names it. Instants on two local dates are written whole around the fallback, each as the short date with the full year (with the era across eras) and the time joined by the short atTime glue, as ICU4C writes them ("1/5/2026, 10:00 PM PST – 1/6/2026, 2:00 AM PST"); the dates take a date formatter's state in the locale's calendar, made then from the data and locale the formatter was made with.  @param formatter  A formatter from mlocInitTimeFormatter, with a zone. @param zone       The zone. @param start      The range's start, in Temporal's range. @param end        Its end, likewise. @param output     Where the text goes (record mloc-0015); parts as mlocFormatDateRange's. @return As mlocFormatTimeInZone; across dates, also as mlocInitDateFormatter for the dates (mloc_errorNotFound for data without a numbering system's spell-out rules the dates name). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitDateTimeFormatter(mlocDateTimeFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocDateTimeOptions* options);
```
Makes a locale's date-time formatter: the date as mlocInitDateFormatter writes it, the time as mlocInitTimeFormatter does, joined by the calendar's glue ("October 15, 2026 at 4:03 PM") of the date's length as ICU4C chooses it (a field set by its month: written out with a weekday full, written out long, abbreviated medium, else short; a standard date format by its own length). A weekday with a time is matched whole where the locale has a pattern for both ("Thursday 4:03 PM"), unless a zone follows ("Thu, 4:03 PM PT", as ICU4C composes it).  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          As mlocInitDateFormatter's. @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return As mlocInitDateFormatter and mlocInitTimeFormatter; mloc_errorInvalid also for a calendar period (a month, a year), a standard format on one side only, or a zone in the date's options. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateTime(const mlocDateTimeFormatter* formatter, const mlocDateTime* dateTime, mlocOutput* output);
```
Writes a date and time of day by the formatter, the date read in the formatter's calendar. Parts: mlocDatePart.  @param formatter  A formatter from mlocInitDateTimeFormatter. @param dateTime   The date and time. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a date-time out of range, or a formatter the library did not make or made with a zone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateTimeInZone(const mlocDateTimeFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* instant, mlocOutput* output);
```
Writes the date and time of day in a zone at an instant, and the zone, by a formatter made with one: the zone's local date-time, the date read in the formatter's calendar. Parts: mlocDatePart.  @param formatter  A formatter from mlocInitDateTimeFormatter, with a zone. @param zone       The zone. @param instant    The instant, in Temporal's range. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a zone the library did not make, an instant out of range, or a formatter the library did not make or made without a zone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateTimeRange(const mlocDateTimeFormatter* formatter, const mlocDateTime* start, const mlocDateTime* end, mlocOutput* output);
```
Writes a range of two date-times by the formatter, as ICU4C's DateIntervalFormat writes the formatter's skeleton: on one day the date once and the time's range inside the Gregorian medium date-time glue ("Jan 5, 2026, 10:00 – 11:30 AM"), or one date-time where the time shows no difference; on two days each whole around the fallback, the date by the pattern with the greatest difference's field prefixed where the date lacks it, joined by the formatter's glue of that pattern's length. The start may pass the end.  @param formatter  A formatter from mlocInitDateTimeFormatter, without a zone. @param start      The range's start. @param end        Its end. @param output     Where the text goes (record mloc-0015); parts as mlocFormatDateRange's. @return As mlocFormatDateTime. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateTimeRangeInZone( const mlocDateTimeFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* start, const mlocInstant* end, mlocOutput* output);
```
Writes a range of two instants' date-times in a zone by a formatter made with one, as mlocFormatDateTimeRange writes the zone's local date-times, each time with its zone as the pattern names it.  @param formatter  A formatter from mlocInitDateTimeFormatter, with a zone. @param zone       The zone. @param start      The range's start, in Temporal's range. @param end        Its end, likewise. @param output     Where the text goes (record mloc-0015); parts as mlocFormatDateRange's. @return As mlocFormatDateTimeInZone. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitZoneFormatter(mlocZoneFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, mlocZoneStyle style);
```
Makes a locale's zone formatter. The names are its zone names (zone-names.mloc) along its chain, the countries' its region names (names.mloc, else the region code), the offset's digits its numbering system's (-u-nu, else its default; an algorithmic one writes ASCII digits). A generic name is qualified where the metazone's preferred zone for the locale's region (its likely region when it names none) is another zone of another offset.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the zone names (zone-names.mloc), the zone codes, the number units, the likely subtags and the parents; the region names (names.mloc) for the location formats. @param locale        The locale, canonicalized. @param style         How zones are written. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorNotFound for data without the zone names, the zone codes or the number units; mloc_errorInvalid for a null pointer or a style out of range or none. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatZone(const mlocZoneFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* instant, mlocOutput* output);
```
Writes a time zone at an instant by the formatter (the instant picks the metazone, standard or daylight time and the offset). A zone CLDR does not know, an offset zone and a zone without a region fall back to the offset. Parts: mlocDatePart.  @param formatter  A formatter from mlocInitZoneFormatter. @param zone       The zone. @param instant    The instant, in Temporal's range. @param output     Where the text goes (record mloc-0015). @return mloc_success; mloc_errorCapacity when the text does not fit (output's length then the size needed); mloc_errorInvalid for a null pointer, a zone the library did not make, an instant out of range, or a formatter the library did not make. @par Thread safety Safe from any thread.

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

## `duration.h`

Durations (record mloc-0044): a Temporal duration's fields written as a locale says them ("1 hr, 2 min, 3 sec", "1:02:03", "2 days and 3 hours"), as ECMA-402's Intl.DurationFormat writes them. The fields are written as given, never balanced ("90 min"); the zone part's arithmetic balances and rounds them (mlocGetDurationValues). The unit names, lists and duration patterns are those of the unit and list parts.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitDurationFormatter(mlocDurationFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocDurationOptions* options);
```
Makes a locale's duration formatter.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number units, the plural rules, the unit names and the lists. @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorNotFound for data without those parts; mloc_errorData for a locale whose duration pattern is not hours, minutes and seconds between two separators; mloc_errorCapacity when the names do not fit the formatter; mloc_errorInvalid for a null pointer or options out of range or in conflict, as ECMA-402 refuses them (a field's style it does not take, display always on a fraction, a field not numeric after a numeric one, a field not a fraction after one). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetDurationOptions(const mlocDurationFormatter* formatter, mlocDurationOptions* optionsOut);
```
Gives a formatter's options as resolved: every field's style and display (numeric for a fraction), never the defaults.  @param formatter   A formatter from mlocInitDurationFormatter. @param optionsOut  Receives the options. @return mloc_success; mloc_errorInvalid for a null pointer or a formatter the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatDuration(const mlocDurationFormatter* formatter, const mlocDurationValues* duration, mlocOutput* output);
```
Writes a duration as ECMA-402's PartitionDurationFormatPattern does: the fields in order up to the first numeric one, each shown by its display as a number with the unit's name in its plural category, the numeric fields as one item joined by the locale's separators, the items joined by the locale's unit list of the style (short for digital). The first field shown carries the sign (-0 for a field of 0 in a negative duration). Each field is its exact value, and the fractions' sum is exact.  @param formatter  A formatter from mlocInitDurationFormatter. @param duration   The fields: integers of one sign, valid as mlocMakeDuration takes them. @param output     Receives the text; its part function, when set, receives the parts (mloc_durationPartField). @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed); mloc_errorInvalid for a null pointer, a duration mlocMakeDuration refuses, or a formatter the library did not make. @par Thread safety Safe from any thread.

## `ixdtf.h`

Temporal's strings (record mloc-0041): ISO 8601 date-times with RFC 9557's annotations, durations, UTC offsets, and time zone and calendar strings, read as ECMA-262's grammar reads them. The text is UTF-8; every terminal of the grammar is ASCII.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseTemporalString(const char* text, size_t length, uint32_t formats, mlocTemporalParse* parseOut);
```
Reads a date-time string (ECMA-262's ParseISODateTime) as one of the goals allowed: dates in basic or extended form, times to the nanosecond, offsets, a time zone annotation and annotations with their rules for u-ca and for critical flags.  @param text     The string, UTF-8; it need not end in a zero byte. @param length   Its length in bytes. @param formats  The goals allowed, mlocTemporalFormat bits. @param parseOut Receives what the string holds. @return mloc_success; mloc_errorSyntax for a string no allowed goal matches, or that the grammar's early errors or the annotation rules refuse; mloc_errorInvalid for a null pointer or no goal allowed. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseUTCOffset(const char* text, size_t length, int64_t* nanosecondsOut, bool* hasSecondsOut);
```
Reads a UTC offset (ECMA-262's ParseDateTimeUTCOffset): ±HH, ±HH:MM or ±HHMM, with seconds and up to nine digits of a fraction.  @param text               The string, UTF-8. @param length             Its length in bytes. @param nanosecondsOut     Receives the offset. @param hasSecondsOut      Receives whether it gave seconds; may be null. @return mloc_success; mloc_errorSyntax for a string that is no UTC offset; mloc_errorInvalid for a null text or nanosecondsOut. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseTimeZoneString(const char* text, size_t length, mlocTimeZoneParse* parseOut);
```
Reads a time zone string (ECMA-262's ParseTemporalTimeZoneString): a time zone identifier, or a date-time string whose annotation, Z or offset names the zone.  @param text      The string, UTF-8. @param length    Its length in bytes. @param parseOut  Receives the zone read. @return mloc_success; mloc_errorSyntax for a string that names no zone by the grammar; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseCalendarString(const char* text, size_t length, mlocTextSpan* calendarOut);
```
Reads a calendar string (ECMA-262's ParseTemporalCalendarString): a date-time string's calendar annotation, an empty span when it has none (the ISO calendar), or the string itself when it is a calendar id by the grammar.  @param text        The string, UTF-8. @param length      Its length in bytes. @param calendarOut Receives the calendar id's span. @return mloc_success; mloc_errorSyntax for a string that is neither; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocParseDuration(const char* text, size_t length, mlocDurationValues* valuesOut);
```
Reads a duration string (ECMA-262's ParseTemporalDurationString): P with years, months, weeks and days, then T with hours, minutes and seconds, the last of which may have a fraction that carries down exactly; the fields, as float64 values, pass Temporal's limits.  @param text       The string, UTF-8. @param length     Its length in bytes. @param valuesOut  Receives the fields. @return mloc_success; mloc_errorSyntax for a string that is no duration; mloc_errorInvalid for a null pointer or fields past the limits. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTemporalDate( const mlocDate* date, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes a date (ECMA-262's TemporalDateToString): its ISO year, month and day and the calendar annotation.  @param date     The date. @param options  The calendar and its annotation; may be null. @param output   Receives the string. @return mloc_success; mloc_errorCapacity when the buffer is too short; mloc_errorInvalid for a null date or output, a date the library did not make, or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatYearMonth(const mlocDate* date, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes a year-month (ECMA-262's TemporalYearMonthToString): its ISO year and month, and the reference day with the annotation when the calendar is not ISO or is shown.  @param date     The year-month's reference date. @param options  The calendar and its annotation; may be null. @param output   Receives the string. @return As mlocFormatTemporalDate. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatMonthDay(const mlocDate* date, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes a month-day (ECMA-262's TemporalMonthDayToString): its ISO month and day, and the reference year with the annotation when the calendar is not ISO or is shown.  @param date     The month-day's reference date. @param options  The calendar and its annotation; may be null. @param output   Receives the string. @return As mlocFormatTemporalDate. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTemporalTime( const mlocTime* time, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes a time of day (ECMA-262's TimeRecordToString) to the options' precision, truncated.  @param time     The time. @param options  The precision; may be null. @param output   Receives the string. @return mloc_success; mloc_errorCapacity when the buffer is too short; mloc_errorInvalid for a null time or output, a time the library did not make, or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTemporalDateTime( const mlocDateTime* dateTime, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes a date-time (ECMA-262's FormatISODateTime): the date, T, the time to the options' precision, and the calendar annotation.  @param dateTime  The date-time. @param options   The precision, calendar and annotation; may be null. @param output    Receives the string. @return As mlocFormatTemporalDate. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatInstant(const mlocInstant* instant, const mlocTimeZone* zone, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes an instant (ECMA-262's TemporalInstantToString): its date-time in universal time and Z, or with a zone its local date-time and offset rounded to the minute.  @param instant  The instant. @param zone     The zone; null for universal time. @param options  The precision; may be null. @param output   Receives the string. @return mloc_success; mloc_errorCapacity when the buffer is too short; mloc_errorInvalid for a null instant or output, an instant out of range, a zone the library did not make, or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatZonedDateTime(const mlocTimeZone* zone, const mlocInstant* instant, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes a zoned date-time (ECMA-262's TemporalZonedDateTimeToString): its local date-time, the offset rounded to the minute, the zone's identifier and the calendar annotation, each as the options show them.  @param zone     The zone. @param instant  The instant. @param options  The precision, calendar and what is shown; may be null. @param output   Receives the string. @return As mlocFormatInstant. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTemporalDuration(const mlocDurationValues* values, const mlocTemporalStringOptions* options, mlocOutput* output);
```
Writes a duration (ECMA-262's TemporalDurationToString): each field not zero with its designator, the seconds from the exact sum of seconds and the sub-second fields to the options' precision, truncated (mloc_secondsMinute is refused).  @param values   The fields, which pass Temporal's limits. @param options  The precision; may be null. @param output   Receives the string. @return mloc_success; mloc_errorCapacity when the buffer is too short; mloc_errorInvalid for a null pointer, fields past the limits or of mixed signs, or options out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatUTCOffset(int64_t offsetNanoseconds, mlocOutput* output);
```
Writes a UTC offset (ECMA-262's FormatUTCOffsetNanoseconds): ±HH:MM, with seconds and as many digits of a fraction as it needs.  @param offsetNanoseconds  The offset, less than a day either way. @param output             Receives the string. @return mloc_success; mloc_errorCapacity when the buffer is too short; mloc_errorInvalid for a null output or an offset of a day or more. @par Thread safety Safe from any thread.

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
MLOC_NODISCARD MLOC_API mlocResult mlocGetLocaleDirection(const mlocDataSet* data, const mlocLocale* locale, mlocDirection* directionOut);
```
A locale's direction, CLDR's character order: right to left when its script, or its likely script when it names none (or "Zzzz"), is written right to left (Arabic, Hebrew, Thaana, N'Ko, Adlam and the other scripts CLDR's script metadata marks so), else left to right. This agrees with every CLDR locale's own character order (Mongolian's, top to bottom, is left to right in a horizontal line, its letters being of bidi class L).  @param data          The data to read the likely subtags from. @param locale        The locale. @param directionOut  Receives mloc_directionLeftToRight or mloc_directionRightToLeft; left unchanged on failure. @return mloc_success; mloc_errorNotFound when the locale names no script and the data has no likely script for it; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

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
Returns a locale's regional preferences (UTS #35), the region's values from CLDR's supplemental data. The region is the -u-rg key's ("en-GB-u-rg-uszzzz" takes the US's), else the locale's region, else the -u-sd key's, else the likely one ("en" takes the US's), else 001; a region without data takes 001's. Keys override: -u-hc the hour cycle (c12 and c24 resolve to the cycle the allowed formats list first), -u-fw the first day, -u-ms the measurement system, -u-ca the calendar; -u-ca-iso8601 also begins weeks on Monday with four days in the first unless -u-fw or -u-rg give the first day. The hour data is looked up for the language in the region first (ca-ES has its own). A key with a value it does not take is left out.  @param data          The data with the preferences and likely subtags. @param locale        The locale, canonicalized. @param preferencesOut Receives the preferences; left unchanged on failure. @return mloc_success; mloc_errorNotFound for data without the preferences; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

## `message.h`

Messages (record mloc-0045): Unicode MessageFormat 2 (UTS #35 Part 9) compiled once into caller memory, bound once to a locale, and formatted many times. A compiled message is locale-free: it holds the message's declarations, selectors, variants and patterns with their names resolved, keys and names in NFC, and every syntax and data model check done. It is the form a catalog stores; it is valid only for this library's version.

```c
MLOC_NODISCARD MLOC_API mlocTextResult mlocCompileMessage(const char* text, size_t length, void* buffer, size_t capacity, size_t* sizeOut, mlocMessageError* errorOut);
```
Compiles a message from its MF2 text into caller memory. Call it with a null buffer and a capacity of 0 to learn the capacity needed. Syntax errors, a variant's key count and a missing fallback variant are found while measuring; the data model errors that compare names or keys need the compiled form, so they are found only once the buffer is large enough.  @param text      The message, UTF-8. May be null when length is 0. @param length    The number of bytes. @param buffer    Receives the compiled message; any alignment. @param capacity  The buffer's size in bytes. @param sizeOut   Receives the compiled message's size on success, or the capacity needed with mloc_errorCapacity, which may be a little more than the size. @param errorOut  Receives what failed with mloc_errorSyntax, else mloc_messageErrorNone; may be null. @return mloc_success and the length read; mloc_errorSyntax and the byte offset where the error is found (also for a name or key with a run of more than 30 combining marks, which UAX #15's stream-safe form bounds and NFC here does not take, and for a name whose NFC form is no name: U+037E and U+1FEF become ";" and "`"); mloc_errorCapacity when the buffer is too small, nothing then being written that the caller can use; mloc_errorInvalid for a null pointer, or a text of 1 GiB or more. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocWriteMessageText(const void* message, size_t size, mlocOutput* output);
```
Writes a compiled message back as MF2 text, in a canonical form: a simple message where its pattern can stand alone; else each declaration, the selectors and each variant on a line of their own; one space between an expression's parts; literals quoted only where they must be. Compiling the text gives the same records, and writing them again the same text. The message is checked first, so bytes from a file are safe to give: every count, index and string must hold, the strings well-formed UTF-8 and the names MF2 names.  @param message  A compiled message from mlocCompileMessage. @param size     Its size in bytes. @param output   Receives the text. @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed); mloc_errorData when the bytes are not a compiled message, or one whose text would be 1 GiB or more, which could not compile again; mloc_errorDataVersion for one of another library or Unicode version; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitMessageFormatter(mlocMessageFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocMessageOptions* options);
```
Makes a locale's message formatter.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data its functions read. @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return mloc_success; mloc_errorInvalid for a null pointer, an unknown isolation, or a function without a resolve entry, a name that is not an MF2 identifier with a namespace, a namespace of u: or mloc:, or two functions of one name. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocBindMessage(const mlocMessageFormatter* formatter, const void* message, size_t size, void* buffer, size_t capacity, size_t* sizeOut);
```
Binds a compiled message to a formatter in caller memory: the message is read as mlocWriteMessageText reads it, each function found, each chain of declarations measured, and each number function's formatter and plural rules made once where its options do not come from arguments (a format call makes its own when its options differ). No program function is called. The bound message points into the compiled one and the formatter, which must outlive it; it is read-only, so threads may format with it at once. Call it with a null buffer and a capacity of 0 to learn the capacity. A bind that fails leaves the buffer holding no bound message, even one bound there before.  @param formatter  A formatter from mlocInitMessageFormatter. @param message    A compiled message from mlocCompileMessage. @param size       Its size in bytes. @param buffer     Receives the bound message; any alignment. @param capacity   The buffer's size in bytes. @param sizeOut    Receives the size written, or needed with mloc_errorCapacity. @return mloc_success; mloc_errorCapacity when the buffer is too small; mloc_errorData and mloc_errorDataVersion as mlocWriteMessageText; mloc_errorInvalid for a null pointer, a formatter the library did not make, more than 32 selectors, a chain of more than 64 expressions each naming the next through a declaration, or a message whose format call would resolve more than 65536 expressions (a local is resolved again at each reference, an option's twice; these two found only when binding, not when measuring). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatMessage(const void* bound, const mlocMessageArgument* arguments, size_t argumentCount, mlocOutput* output, mlocMessageFaults* faults);
```
Formats a bound message with its arguments: the pattern selected, each text written, each placeholder resolved where it stands and written, markup writing nothing. An expression that fails is written as its fallback ("{$name}", "{|literal|}", "{:function}") and reported as a fault. A numeric argument without a function is written plainly ("1234.5"), a currency amount as its amount and code ("42.5 EUR").  @param bound          A bound message from mlocBindMessage. @param arguments      The arguments; may be null when count is 0. @param argumentCount  Their number. @param output         Receives the text. @param faults         Receives the faults; may be null. @return mloc_success; mloc_withFallbacks when the text holds fallbacks; in strict mode mloc_errorFormat at the first fault, nothing written; mloc_errorCapacity when the text does not fit (output->length is then the length needed); mloc_errorInvalid for a null pointer (an argument's name, string, decimal or amount among them), a decimal that is not one the library makes, a currency code that is not three ASCII letters, an unknown kind, or a bound message the library did not make. @par Thread safety Safe from any thread.

```c
size_t mlocGetMessageOptionCount(const mlocMessageCall* call);
```
The number of options a function's call has (its attributes and its u: options aside, which are the formatter's), for mlocGetMessageOption.  @param call  The call a function's resolve entry received, read only while that entry runs. @return The count; 0 for a null pointer. @par Thread safety Safe from any thread; the call is used by one thread at a time.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetMessageOption(const mlocMessageCall* call, size_t index, const char** nameOut, size_t* lengthOut, mlocMessageOperand* valueOut, mlocMessageResolved* storage);
```
Reads one of a call's options: its name and its value, resolved now (a variable's value resolved through its declaration).  @param call        The call a function's resolve entry received. @param index       Below mlocGetMessageOptionCount. @param nameOut     Receives the name (UTF-8, NFC, not terminated). @param lengthOut   Receives its length. @param valueOut    Receives the value; mloc_messageOperandFallback for one that failed (the formatter has reported a bad option), which the function ignores. @param storage     Holds a resolved value valueOut points to; it must live as long as valueOut is read. @return mloc_success; mloc_errorInvalid for a null pointer or an index past the count. @par Thread safety Safe from any thread; the call is used by one thread at a time.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFindMessageOption(const mlocMessageCall* call, const char* name, size_t length, mlocMessageOperand* valueOut, mlocMessageResolved* storage);
```
Finds a call's option by name and reads its value as mlocGetMessageOption does.  @param call      The call a function's resolve entry received. @param name      The option's name, UTF-8 in NFC. @param length    Its length. @param valueOut  Receives the value. @param storage   As for mlocGetMessageOption. @return mloc_success; mloc_errorNotFound when the call has no such option; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread; the call is used by one thread at a time.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetMessageLocale(const mlocMessageCall* call, mlocLocale* localeOut);
```
The locale a call formats for: the formatter's.  @param call        The call a function's resolve entry received. @param localeOut   Receives the locale. @return mloc_success; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread; the call is used by one thread at a time.

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

## `relative.h`

Relative time (record mloc-0043): a signed amount of a unit written as a locale says it ("in 3 days", "2 hr. ago", "yesterday", "next Monday"), as ECMA-402's Intl.RelativeTimeFormat and ICU4C's RelativeDateTimeFormatter write it. The amount is the caller's: the library never reads a clock. The names and patterns are a data file of their own, src/generated/relative.mloc.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInitRelativeFormatter(mlocRelativeFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale, const mlocRelativeOptions* options);
```
Makes a locale's relative time formatter.  @param formatterOut  Receives the formatter; left unchanged on failure. @param data          The data with the number units, the plural rules and relative time (relative.mloc). @param locale        The locale, canonicalized. @param options       The options, or null for the defaults. @return mloc_success; mloc_fellBack when the numbering system asked for has no digits and the default stands instead; mloc_errorNotFound for data without the number units or relative time; mloc_errorData for a chain of more relative units than the formatter holds; mloc_errorInvalid for a null pointer or options out of range or in conflict (a style other than decimal, a sign display other than auto). @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatRelative(const mlocRelativeFormatter* formatter, const mlocDecimal* amount, mlocRelativeUnit unit, mlocOutput* output);
```
Writes an amount of a unit relative to now: negative amounts (-0 included) in the past, the others in the future; the amount's magnitude by the formatter's number options in the pattern of its plural category as written ("in 1 day", "in 1.5 days"), a category the locale lacks taking the other category's, a pattern without the number writing none (Arabic's "in one year"); under auto, an amount equal to a named one writes the name ("yesterday", "now").  @param formatter  A formatter from mlocInitRelativeFormatter. @param amount     The amount, finite. @param unit       The unit. @param output     Receives the text; its part function, when set, receives mlocNumberPart values: the number's, and literal for the pattern's text and for a name. @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding passes the decimal's limits; mloc_errorNotFound for a unit whose patterns the locale's chain lacks; mloc_errorInvalid for a null pointer, an amount not finite, a unit out of range, or a formatter or decimal the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatRelativeDayTime( const mlocRelativeFormatter* relative, const mlocTimeFormatter* time, const mlocDecimal* days, const mlocTime* timeOfDay, mlocOutput* output);
```
Writes a day relative to today and a time of day joined by the locale's relative date-time glue ("tomorrow at 3:00 PM", "in 3 days, 10:00"): the day as mlocFormatRelative writes an amount of days, the time as the time formatter writes it, joined by the glue of the time formatter's calendar at the length the relative width gives (long the long glue, short the medium, narrow the short; ICU4C takes the medium atTime glue whatever the width). The day is capitalized as the relative formatter asks only where the glue writes it first.  @param relative   A formatter from mlocInitRelativeFormatter. @param time       A formatter from mlocInitTimeFormatter, without a zone; its data holds the glue. @param days       The day's distance from today, finite. @param timeOfDay  The time. @param output     Receives the text; its part function, when set, receives mlocDatePart values: the day whole as mloc_datePartRelativeDay, the time's fields, and literal for the glue's text. @return mloc_success; mloc_errorCapacity when the text does not fit (output->length is then the length needed) or rounding passes the decimal's limits; mloc_errorNotFound for a day whose patterns the locale's chain lacks; mloc_errorInvalid for a null pointer, an amount not finite, a time out of range, or a formatter the library did not make (a time formatter with a zone among them). @par Thread safety Safe from any thread.

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

## `zone.h`

Time zones (record mloc-0041): the IANA time zone database's zones as ECMA-262's Temporal and ECMA-402 read them. A zone is found by an identifier, a Zone or Link name of the database in any letter case ("America/New_York", "asia/calcutta") or a fixed offset ("+05:30"), and keeps the identifier as given; two zones are equal when their identifiers resolve to the same primary identifier. Its offset at any instant comes from the data's zones unit, compiled from the pinned database release (mlocGetDataVersion names it), and runs over Temporal's whole range. A date and a time of day make a date-time, which a zone maps to the instants it names (none in a gap, two in a fold) and back.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFindTimeZone(const mlocDataSet* data, const char* id, size_t length, mlocTimeZone* zoneOut);
```
Finds a time zone by its identifier: an available Zone or Link name compared with ASCII letters folded, or an offset as ECMA-262's UTCOffset without seconds: a sign ("+", "-" or U+2212) and hours 00 to 23, then optionally minutes, after a colon or not ("+05", "+0530", "-05:30").  @param data     The data with the zones unit; may be null for an offset. @param id       The identifier, UTF-8. @param length   Its length in bytes. @param zoneOut  Receives the zone; left unchanged on failure. @return mloc_success; mloc_errorSyntax for an offset that is not one of those forms; mloc_errorNotFound for a name the data does not have, or data without the zones unit; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeZoneId(const mlocTimeZone* zone, mlocOutput* output);
```
Writes a zone's identifier in its normalized form: a name in the database's letter case, as given otherwise ("Asia/Calcutta" stays itself), or an offset as "+HH:MM" ("+00:00" for zero). It reports no parts.  @param zone    The zone. @param output  Where the identifier goes. @return mloc_success; mloc_errorCapacity with the length needed; mloc_errorInvalid for a null pointer or a zone the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatPrimaryTimeZoneId(const mlocTimeZone* zone, mlocOutput* output);
```
Writes a zone's primary identifier, as ECMA-402's AvailableNamedTimeZoneIdentifiers resolves it ("Asia/Calcutta" gives "Asia/Kolkata", "Etc/GMT" gives "UTC"); an offset zone's is its identifier. It reports no parts.  @param zone    The zone. @param output  Where the identifier goes. @return As mlocFormatTimeZoneId. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocTimeZonesEqual(const mlocTimeZone* one, const mlocTimeZone* two, bool* equalOut);
```
Tells whether two zones are equal as ECMA-262's TimeZoneEquals says: named zones whose primary identifiers are the same, or offset zones of the same offset. A named zone never equals an offset zone, even "UTC" and "+00:00".  @param one       A zone. @param two       Another. @param equalOut  Receives whether they are equal. @return mloc_success; mloc_errorInvalid for a null pointer or a zone the library did not make. @par Thread safety Safe from any thread.

```c
size_t mlocGetTimeZoneCount(const mlocDataSet* data);
```
Returns the number of primary time zone identifiers the data has, the list ECMA-402's Intl.supportedValuesOf("timeZone") gives.  @param data  The data with the zones unit. @return The count; 0 for data without the zones unit or a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZone(const mlocDataSet* data, size_t index, mlocTimeZone* zoneOut);
```
Returns the zone of a primary identifier, in the order of the identifiers' bytes ("Etc/UTC" before "Etc/Universal" were both primary).  @param data     The data with the zones unit. @param index    Below mlocGetTimeZoneCount. @param zoneOut  Receives the zone. @return mloc_success; mloc_errorInvalid for a null pointer or an index out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZoneRelease(const mlocDataSet* data, mlocOutput* output);
```
Writes the time zone database release a data set's zones are compiled from, such as "2026e": that of the first source with zones, which every zone lookup in the set reads. A zones file (mlocgen zonesfile) put before the compiled-in data gives a release other than the library's.  @param data    The data with the zones unit. @param output  Receives the release's name. @return mloc_success; mloc_errorCapacity when the buffer is too short; mloc_errorInvalid for a null pointer; mloc_errorNotFound for data without the zones unit. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFindTimeZoneBcp47(const mlocDataSet* data, const char* id, size_t length, mlocTimeZone* zoneOut);
```
Finds a time zone by its BCP 47 id, the value of a locale's "tz" key ("usnyc" for America/New_York), as CLDR lists them: the zone is the id's identifier, found in the set's zones. A deprecated id ("camtr") finds its replacement's zone; "unk" is refused.  @param data     The data with the zone codes and the zones units. @param id       The id, ASCII letters in any case. @param length   Its length in bytes. @param zoneOut  Receives the zone; left unchanged on failure. @return mloc_success; mloc_errorNotFound for an id CLDR does not list, a zone the set's zones lack, or data without either unit; mloc_errorInvalid for a null pointer. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeZoneBcp47(const mlocDataSet* data, const mlocTimeZone* zone, mlocOutput* output);
```
Writes a zone's BCP 47 id: the id whose CLDR aliases hold the zone's identifier, or else its primary identifier; for an offset zone of whole hours from -12 to +14, the id of its Etc/GMT zone ("utcw05" for -05:00, "utc" for +00:00).  @param data    The data with the zone codes unit. @param zone    The zone. @param output  Receives the id, lower-case. @return mloc_success; mloc_errorCapacity with the length needed; mloc_errorNotFound for a zone CLDR gives no id, or data without the zone codes unit; mloc_errorInvalid for a null pointer or a zone the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFindTimeZoneWindows(const mlocDataSet* data, const char* name, size_t length, const char* region, size_t regionLength, mlocTimeZone* zoneOut);
```
Finds a time zone by its Windows name ("Eastern Standard Time"), as CLDR's windowsZones maps it: the zone of the region's mapping, or of the 001 mapping when the region has none or none is given, as ICU's TimeZone::getIDForWindowsID does.  @param data          The data with the zone codes and the zones units. @param name          The Windows name, ASCII letters in any case. @param length        Its length in bytes. @param region        The region, two ASCII letters in any case or three digits; may be null when regionLength is 0. @param regionLength  Its length in bytes, or 0 for none. @param zoneOut       Receives the zone; left unchanged on failure. @return mloc_success; mloc_errorNotFound for a name CLDR does not map, a zone the set's zones lack, or data without either unit; mloc_errorInvalid for a null pointer or a region that is not two letters or three digits. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeZoneWindows(const mlocDataSet* data, const mlocTimeZone* zone, mlocOutput* output);
```
Writes a zone's Windows name: the name CLDR's windowsZones maps the zone's BCP 47 id to (mlocFormatTimeZoneBcp47), as ICU's TimeZone::getWindowsID gives it.  @param data    The data with the zone codes unit. @param zone    The zone. @param output  Receives the name. @return mloc_success; mloc_errorCapacity with the length needed; mloc_errorNotFound for a zone with no Windows name, or data without the zone codes unit; mloc_errorInvalid for a null pointer or a zone the library did not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZoneOffset(const mlocTimeZone* zone, const mlocInstant* instant, int32_t* offsetSecondsOut);
```
Returns a zone's offset from universal time at an instant, in seconds: local time is the instant plus the offset. The database's offsets before 1900 or so are local mean time, to the second.  @param zone              The zone. @param instant           An instant in Temporal's range. @param offsetSecondsOut  Receives the offset. @return mloc_success; mloc_errorInvalid for a null pointer, a zone the library did not make or an instant out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZoneTransition(const mlocTimeZone* zone, const mlocInstant* instant, mlocTransitionDirection direction, mlocInstant* transitionOut);
```
Finds a zone's first transition strictly after an instant, or its last strictly before, as Temporal's getTimeZoneTransition: the first instant of a new offset (one that changes only whether daylight saving time is in effect is not a transition).  @param zone           The zone. @param instant        An instant in Temporal's range. @param direction      Next or previous. @param transitionOut  Receives the transition, whose nanoseconds are 0. @return mloc_success; mloc_errorNotFound when there is none within Temporal's range (an offset zone has none); mloc_errorInvalid for a null pointer, a zone the library did not make, an instant out of range or another direction. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeTime(const mlocTimeInput* input, mlocOverflow overflow, mlocTime* timeOut);
```
Makes a time of day from its fields, as Temporal's RegulateTime does: a field out of its range is clamped into it (mloc_overflowConstrain; a leap second's 60 becomes 59) or refused (mloc_overflowReject). Milliseconds, microseconds and nanoseconds range 0 to 999 each.  @param input     The fields. @param overflow  What a field out of range does. @param timeOut   Receives the time; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, another overflow, or a field refused. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeDateTime(const mlocDate* date, const mlocTime* time, mlocDateTime* dateTimeOut);
```
Makes a date-time of a date and a time of day, within Temporal's limits for one: every date and time but midnight at the start of the first day (April 19 of year -271821 in the ISO calendar), which lies a whole day before the first instant.  @param date         The date. @param time         The time of day. @param dateTimeOut  Receives the date-time; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date or time the library did not make, or that midnight. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeTimeDuration(const mlocTimeDurationFields* fields, mlocTimeDuration* durationOut);
```
Makes a time duration of fields (ECMA-262's TimeDurationFromComponents, with days of 24 hours as Add24HourDaysToTimeDuration counts them). A field too large for int64_t is the host's to fold in.  @param fields       The fields, of any signs. @param durationOut  Receives the duration; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, or a field or the total of 2^53 seconds or more in size. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocBalanceTimeDuration(const mlocTimeDuration* duration, mlocTimeUnit largestUnit, mlocTimeDurationFields* fieldsOut);
```
Gives a time duration in fields up to a largest unit, each of the duration's sign (ECMA-262's TemporalDurationFromInternal for its time part): with days, hours below 24; with hours, minutes below 60; and so on down to the largest unit, which takes the rest.  @param duration     The duration. @param largestUnit  The largest unit to give. @param fieldsOut    Receives the fields. @return mloc_success; mloc_errorInvalid for a null pointer, a duration the library would not make, an unknown unit, or a largest field past int64_t: nanoseconds of a duration of more than about 292 years, microseconds of more than about 292,000 years. The host then forms that field, which Temporal keeps as a float64, from the duration. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocAddTimeDurations(const mlocTimeDuration* one, const mlocTimeDuration* two, mlocTimeDuration* durationOut);
```
Adds two time durations (ECMA-262's AddTimeDuration), as Temporal's Duration.add reads them with days of 24 hours.  @param one          The first duration. @param two          The second. @param durationOut  Receives the sum, of one sign. @return mloc_success; mloc_errorInvalid for a null pointer, a duration the library would not make, or a sum of 2^53 seconds or more in size. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocRoundTimeDuration(const mlocTimeDuration* duration, mlocTimeUnit unit, uint32_t increment, mlocRoundingMode mode, mlocTimeDuration* durationOut);
```
Rounds a time duration to a multiple of an increment of a unit (ECMA-262's RoundTimeDuration), as Temporal's Duration.round reads it without a date to start from, days being 24 hours. A day takes an increment of 1 to 10^9; hours to nanoseconds one dividing the next unit (24, 60, 60, 1000, 1000, 1000) into more than one part.  @param duration     The duration. @param unit         The unit. @param increment    The increment. @param mode         The rounding mode. @param durationOut  Receives the rounded duration. @return mloc_success; mloc_errorInvalid for a null pointer, a duration the library would not make, a unit, increment or mode refused, or a result of 2^53 seconds or more in size. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocTotalTimeDuration(const mlocTimeDuration* duration, mlocTimeUnit unit, double* totalOut);
```
Gives the total of a unit in a time duration (ECMA-262's TotalTimeDuration), days being 24 hours, as Temporal's Duration.total reads it without a date to start from: the float64 nearest the exact quotient, which the library finds with integers alone.  @param duration  The duration. @param unit      The unit. @param totalOut  Receives the total. @return mloc_success; mloc_errorInvalid for a null pointer, a duration the library would not make or an unknown unit. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocAddToTime(const mlocTime* time, const mlocTimeDuration* duration, mlocTime* timeOut, int64_t* daysOut);
```
Adds a time duration to a time of day (ECMA-262's AddTime), giving the time and the days it passes, as Temporal's PlainTime.add (which drops the days) and PlainDateTime.add read it. Subtracting is adding the negated duration.  @param time      The time of day. @param duration  The duration. @param timeOut   Receives the time. @param daysOut   Receives the days passed, negative going back; may be null. @return mloc_success; mloc_errorInvalid for a null pointer, or a time or duration the library would not make. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocTimeUntil(const mlocTime* one, const mlocTime* two, const mlocTimeRounding* rounding, mlocTimeDurationFields* fieldsOut);
```
Computes the duration from one time of day to another, rounded and given in fields, as Temporal's PlainTime.until does: units hour to nanosecond, the smallest no larger than the largest, an increment dividing the unit's next (24 for hours, 60 for minutes and seconds, 1000 for the rest) into more than one part. Temporal's since is this with the mode negated (ceil for floor, half-ceil for half-floor, and back) and the fields negated.  @param one        The time from. @param two        The time to. @param rounding   The units, increment and mode. @param fieldsOut  Receives the fields; days are 0. @return mloc_success; mloc_errorInvalid for a null pointer, a time the library did not make, a unit, increment or mode refused. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocRoundTime(const mlocTime* time, mlocTimeUnit unit, uint32_t increment, mlocRoundingMode mode, mlocTime* timeOut, int64_t* daysOut);
```
Rounds a time of day to a multiple of an increment of a unit (ECMA-262's RoundTime), as Temporal's PlainTime.round (units hour to nanosecond, which drops the days) and PlainDateTime.round read it. A unit of hours to nanoseconds takes an increment dividing the next unit (24, 60, 60, 1000, 1000, 1000) into more than one part; a day takes 1, giving midnight of the day or the next.  @param time       The time of day. @param unit       The unit. @param increment  The increment. @param mode       The rounding mode. @param timeOut    Receives the time. @param daysOut    Receives 1 when the rounding reaches the next midnight, else 0; may be null. @return mloc_success; mloc_errorInvalid for a null pointer, a time the library did not make, or a unit, increment or mode refused. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocMakeDuration(const mlocDurationValues* values, mlocDateTimeDuration* durationOut);
```
Makes a duration's internal record from its fields (ECMA-262's IsValidDuration and ToInternalDurationRecord): the years, months, weeks and days as they are, the hours to nanoseconds as one exact time duration. The doubles are read from their bits and summed exactly, as the specification requires of nanoseconds and microseconds past 2^53.  @param values       The fields. @param durationOut  Receives the record. @return mloc_success; mloc_errorInvalid for a null pointer, a field that is no finite integer, fields of two signs, years, months or weeks of 2^32 or more in size, or days to nanoseconds of 2^53 seconds or more. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetDurationValues(const mlocDateTimeDuration* duration, mlocUnit largestUnit, mlocDurationValues* valuesOut);
```
Gives a duration's fields from an internal record (ECMA-262's TemporalDurationFromInternal): the time part balanced up to the largest unit, and into days for a date unit, each field the float64 nearest it. The fields are refused, as CreateTemporalDuration refuses them, when their float64 values pass Temporal's limits.  @param duration     The record, of one sign. @param largestUnit  The largest unit. @param valuesOut    Receives the fields. @return mloc_success; mloc_errorInvalid for a null pointer, a record of two signs or a time part the library would not make, an unknown unit, or fields past Temporal's limits. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocAddToDateTime(const mlocDateTime* dateTime, const mlocDateTimeDuration* duration, mlocOverflow overflow, mlocDateTime* dateTimeOut);
```
Adds a duration to a date-time, as Temporal's PlainDateTime.add does (AddDurationToDateTime): the time first, its days passed added to the duration's days, then the date part in the date's calendar as mlocAddToDate adds it. Subtracting is adding the negated duration.  @param dateTime     The date-time. @param duration     The duration, its days of 24 hours. @param overflow     What a month code or day the result lacks does. @param dateTimeOut  Receives the date-time; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date-time the library did not make, a duration of two signs or out of its limits, a month code or day refused, or a result outside Temporal's limits for a date-time. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeUntil(const mlocDateTime* one, const mlocDateTime* two, const mlocDateTimeRounding* rounding, mlocDateTimeDuration* durationOut);
```
Computes the duration from one date-time to another of the same calendar, rounded, as Temporal's PlainDateTime.until does (DifferencePlainDateTimeWithRounding): the date difference to the second date, or the day before or after it when the times run the other way, the time difference beside it, then the rounding of RoundRelativeDuration. A largest time unit folds the days into the time. Increments: 1 to 10^9 for date units; for time units one that divides the next unit (24, 60, 60, 1000, 1000, 1000) into more than one part. Temporal's since is this with the mode negated and the result negated. The time part is given to mlocBalanceTimeDuration with the largest unit (or days, for a date unit, whose days the host adds to the date part's).  @param one          The date-time from. @param two          The date-time to. @param rounding     The units, increment and mode. @param durationOut  Receives the duration; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date-time the library did not make or out of the limits, date-times of two calendars, a unit, increment or mode refused, or a rounding that reaches a date out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocRoundDateTime(const mlocDateTime* dateTime, mlocTimeUnit unit, uint32_t increment, mlocRoundingMode mode, mlocDateTime* dateTimeOut);
```
Rounds a date-time's time of day as mlocRoundTime does, carrying a next midnight into the date (Temporal's PlainDateTime.round, RoundISODateTime).  @param dateTime     The date-time. @param unit         The unit, a day to a nanosecond. @param increment    The increment, as mlocRoundTime takes it. @param mode         The rounding mode. @param dateTimeOut  Receives the date-time; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, a date-time the library did not make, a unit, increment or mode refused, or a result outside Temporal's limits. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeTotal(const mlocDateTime* one, const mlocDateTime* two, mlocUnit unit, double* totalOut);
```
Gives the total of a unit in the duration from one date-time to another in the same calendar (ECMA-262's DifferencePlainDateTimeWithTotal), as Temporal's Duration.total reads it from a plain date or date-time: the float64 nearest the exact total, which the library finds with integers alone. A calendar unit counts the part of the unit reached, a day 24 hours.  @param one       The date-time from. @param two       The date-time to. @param unit      The unit. @param totalOut  Receives the total. @return mloc_success; mloc_errorInvalid for a null pointer, a date-time the library would not make or outside the limits, two calendars, an unknown unit, or a date the total reaches outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocAddToInstant(const mlocInstant* instant, const mlocTimeDuration* duration, mlocInstant* instantOut);
```
Adds a time duration to an instant, as Temporal's Instant.add does (AddDurationToInstant; the host refuses a duration with date units). Subtracting is adding the negated duration.  @param instant     The instant. @param duration    The time duration. @param instantOut  Receives the instant; left unchanged on failure. @return mloc_success; mloc_errorInvalid for a null pointer, an instant or duration the library would not make, or a result outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocInstantUntil(const mlocInstant* one, const mlocInstant* two, const mlocTimeRounding* rounding, mlocTimeDuration* durationOut);
```
Computes the time from one instant to another, rounded, as Temporal's Instant.until does (DifferenceEpochNanoseconds): units hour to nanosecond, the smallest no larger than the largest, an increment dividing the unit's next (24, 60, 60, 1000, 1000, 1000) into more than one part. The largest unit is the host's to give to mlocBalanceTimeDuration. Temporal's since is this with the mode negated and the result negated.  @param one          The instant from. @param two          The instant to. @param rounding     The units, increment and mode. @param durationOut  Receives the duration. @return mloc_success; mloc_errorInvalid for a null pointer, an instant the library would not make, or a unit, increment or mode refused. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocRoundInstant(const mlocInstant* instant, mlocTimeUnit unit, uint32_t increment, mlocRoundingMode mode, mlocInstant* instantOut);
```
Rounds an instant to a multiple of an increment of a unit, as Temporal's Instant.round does (RoundEpochNanoseconds): the mode applied as if the instant were positive, so floor is always toward the past. Units hour to nanosecond; the increment must divide a day of the unit (24 hours, 1440 minutes, 86400 seconds, and so on), a whole day included.  @param instant     The instant. @param unit        The unit. @param increment   The increment. @param mode        The rounding mode. @param instantOut  Receives the instant. @return mloc_success; mloc_errorInvalid for a null pointer, an instant the library would not make, or a unit, increment or mode refused. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetPossibleInstants(const mlocTimeZone* zone, const mlocDateTime* dateTime, mlocPossibleInstants* instantsOut);
```
Finds the instants at which a zone's clocks read a date-time (ECMA-262's GetPossibleEpochNanoseconds): none when a forward change skips it, two when a backward change repeats it. The date-time's calendar does not matter.  @param zone         The zone. @param dateTime     The local date-time. @param instantsOut  Receives the instants. @return mloc_success; mloc_errorInvalid for a null pointer, a zone or date-time the library did not make, or an instant outside Temporal's range among them. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetInstantFor(const mlocTimeZone* zone, const mlocDateTime* dateTime, mlocDisambiguation disambiguation, mlocInstant* instantOut);
```
Finds the one instant a zone's local date-time names, choosing in a gap or fold as the disambiguation says (ECMA-262's GetEpochNanosecondsFor). In a gap the date-time moves by the gap's length: for 02:30 in a gap from 02:00 to 03:00, compatible and later give 03:30, earlier 01:30.  @param zone            The zone. @param dateTime        The local date-time. @param disambiguation  How to choose. @param instantOut      Receives the instant. @return mloc_success; mloc_errorInvalid for a null pointer, a zone or date-time the library did not make, another disambiguation, a gap or fold refused by mloc_disambiguationReject, or an instant outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetStartOfDay(const mlocTimeZone* zone, const mlocDate* date, mlocInstant* instantOut);
```
Finds the first instant of a date in a zone (ECMA-262's GetStartOfDay): midnight's earlier instant, or the end of the gap when a forward change skips midnight.  @param zone        The zone. @param date        The date. @param instantOut  Receives the instant. @return mloc_success; mloc_errorInvalid for a null pointer, a zone or date the library did not make, or an instant outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetDateTimeFor(const mlocTimeZone* zone, const mlocInstant* instant, mlocCalendar calendar, mlocDateTime* dateTimeOut);
```
Finds a zone's local date-time at an instant, in a calendar (ECMA-262's GetISODateTimeFor): the instant plus the zone's offset then.  @param zone         The zone. @param instant      An instant in Temporal's range. @param calendar     The calendar the date is read in. @param dateTimeOut  Receives the date-time. @return mloc_success; mloc_errorInvalid for a null pointer, a zone the library did not make, an instant out of range or a calendar out of range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocGetInstantForOffset( const mlocTimeZone* zone, const mlocDateTime* dateTime, int64_t offsetNanoseconds, mlocOffsetOption option, mlocDisambiguation disambiguation, bool matchMinutes, mlocInstant* instantOut);
```
Finds the instant a local date-time names with an offset from universal time (ECMA-262's InterpretISODateTimeOffset), weighing the offset against the zone's rules as the option says. With matchMinutes, as for a string that gives the offset to the minute, the zone's offset rounded to the minute also matches.  @param zone                The zone. @param dateTime            The local date-time. @param offsetNanoseconds   The offset, under a day either way. @param option              How the offset is weighed. @param disambiguation      How to choose when the zone's rules decide. @param matchMinutes        Whether an offset rounded to the minute matches. @param instantOut          Receives the instant. @return mloc_success; mloc_errorInvalid for a null pointer, a zone or date-time the library did not make, an offset of a day or more, another option or disambiguation, an offset refused by mloc_offsetReject, a gap or fold refused by mloc_disambiguationReject, or an instant outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocAddToZonedDateTime( const mlocTimeZone* zone, mlocCalendar calendar, const mlocInstant* instant, const mlocDateTimeDuration* duration, mlocOverflow overflow, mlocInstant* instantOut);
```
Adds a duration to a zoned date-time (ECMA-262's AddZonedDateTime): the date part to the local date in its calendar, its time kept and found again in the zone (the earlier of a fold, past a gap by its length), then the time part as exact time.  @param zone        The zone. @param calendar    The calendar the date part is added in. @param instant     The zoned date-time's instant. @param duration    The duration, of one sign. @param overflow    Constrain or refuse a day past its month's end. @param instantOut  Receives the instant. @return mloc_success; mloc_errorInvalid for a null pointer, a zone, instant or duration the library did not make, a calendar out of range, a duration of two signs or too many days, a date refused by mloc_overflowReject, or a date-time or instant outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocZonedDateTimeUntil(const mlocTimeZone* zone, mlocCalendar calendar, const mlocInstant* one, const mlocInstant* two, const mlocDateTimeRounding* rounding, mlocDateTimeDuration* durationOut);
```
Finds the rounded duration from one zoned date-time to another in the same zone and calendar (ECMA-262's DifferenceZonedDateTimeWithRounding): with a largest time unit the exact time between them, else the date difference of their local dates and the exact time that remains, rounded relative to the first with days as long as the zone makes them. The host balances the time part to hours with mlocBalanceTimeDuration. since is until with the mode negated and the fields negated. Temporal's until and since give a zero duration for equal instants before this; Duration.round does not, and rounding a zero duration near the range's end can be refused.  @param zone         The zone. @param calendar     The calendar. @param one          The first instant. @param two          The second instant. @param rounding     The units, increment and mode, as for mlocDateTimeUntil. @param durationOut  Receives the duration. @return mloc_success; mloc_errorInvalid for a null pointer, a zone or instant the library did not make, a calendar out of range, units, an increment or a mode refused, or a date or instant the rounding reaches outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocZonedDateTimeTotal(const mlocTimeZone* zone, mlocCalendar calendar, const mlocInstant* one, const mlocInstant* two, mlocUnit unit, double* totalOut);
```
Gives the total of a unit in the duration from one zoned date-time to another in the same zone and calendar (ECMA-262's DifferenceZonedDateTimeWithTotal), as Temporal's Duration.total reads it from a zoned date-time: the float64 nearest the exact total, which the library finds with integers alone. A day, like the calendar units, is as long as the zone makes it; a time unit counts exact time.  @param zone      The zone. @param calendar  The calendar. @param one       The first instant. @param two       The second instant. @param unit      The unit. @param totalOut  Receives the total. @return mloc_success; mloc_errorInvalid for a null pointer, a zone or instant the library did not make, a calendar out of range, an unknown unit, or a date or instant the total reaches outside Temporal's range. @par Thread safety Safe from any thread.

```c
MLOC_NODISCARD MLOC_API mlocResult mlocRoundZonedDateTime(const mlocTimeZone* zone, const mlocInstant* instant, mlocTimeUnit unit, uint32_t increment, mlocRoundingMode mode, mlocInstant* instantOut);
```
Rounds a zoned date-time (Temporal.ZonedDateTime's round): to a day, the nearer start of a day as long as the zone makes it; otherwise its local date-time rounded and found again with its offset preferred.  @param zone        The zone. @param instant     The zoned date-time's instant. @param unit        The unit, day to nanosecond. @param increment   1 for a day; for another unit below and dividing the next larger. @param mode        The rounding mode. @param instantOut  Receives the instant. @return mloc_success; mloc_errorInvalid for a null pointer, a zone or instant the library did not make, a unit, increment or mode refused, or an instant outside Temporal's range. @par Thread safety Safe from any thread.

---

159 functions across 18 headers.
