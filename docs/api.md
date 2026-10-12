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

---

9 functions across 2 headers.
