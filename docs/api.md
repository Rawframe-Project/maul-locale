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

---

11 functions across 3 headers.
