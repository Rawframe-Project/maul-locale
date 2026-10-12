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

---

3 functions across 1 headers.
