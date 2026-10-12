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

---

32 functions across 5 headers.
