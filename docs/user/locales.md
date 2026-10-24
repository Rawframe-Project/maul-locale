# Locales

A locale names a language and, as needed, its script, region, variants and options: `de-CH`, `sr-Latn`, `ar-EG-u-nu-latn`. Every formatter is made for one. Locales follow UTS #35 and BCP 47; `maul-locale/locale.h` has the functions below, `maul-locale/names.h` the display names.

## Parsing and canonical forms

`mlocParseLocale` reads a BCP 47 tag or a CLDR id (`_` works as a separator too) strictly into an `mlocLocale`, a 128-byte value in canonical syntax: two equal locales are equal byte for byte, so `memcmp` compares them. It refuses what is not a locale id with `mloc_errorSyntax` and the offset of the bad subtag. It does not replace aliases.

Text from outside the program (a user setting, an HTTP header, a stored preference) goes through `mlocCanonicalizeLocale` instead, which also replaces CLDR's aliases: `iw` becomes `he`, `sh` becomes `sr-Latn`, a legacy tag such as `i-klingon` is replaced whole, and old extension values are renamed. Formatters and matching expect canonicalized locales.

```c
mlocLocale locale;
mlocLocaleChanges changes = 0;
mlocTextResult result = mlocCanonicalizeLocale(&set, "iw_IL", 5, &locale, &changes);
if (result.status == mloc_success)
{
    // locale is he-IL; changes has mloc_changedLanguage
}
```

`mlocFormatLocale` writes a locale back as its tag.

## Likely subtags and direction

`mlocMaximizeLocale` adds the script and region a language most likely has (`zh-TW` gives `zh-Hant-TW`, `und` gives `en-Latn-US`); `mlocMinimizeLocale` removes them again, keeping the region (`zh-TW`) or, when asked, the script (`zh-Hant`). `mlocGetLocaleDirection` says whether a locale's text runs right to left, from its script or its likely script.

## Fallback

Locale data is inherited: a value missing in `de-AT` comes from `de`, then from root. The chain is not always the tag with subtags dropped: `en-AU` inherits from `en-001`, `es-MX` from `es-419`, and `zh-Hant` from root, never from `zh`. Collation, plurals, segmentation and grammar have chains of their own. Every lookup in the library follows the right chain by itself; `mlocGetParentLocale` gives the next locale of a chain for a program's own resources:

```c
mlocLocale current = locale;
mlocLocale parent;
while (mlocGetParentLocale(&set, mloc_componentMain, &current, &parent) == mloc_success)
{
    // look the program's own resource up for current, then go on
    current = parent;
}
```

The loop ends at root, `und`, which has no parent.

## Matching a user's languages

A program supports some locales; a user prefers some languages, in order, as the operating system reports them. `mlocMatchLocales` picks the best supported locale as ICU's LocaleMatcher does, by CLDR's distances between languages, scripts and regions: `es-MX` is nearer `es-419` than `es-ES`, `en-IN` nearer `en-GB` than `en-US`, and `nb` matches `no`. A later preference wins only when it matches clearly better. `mlocCombineMatch` then gives the locale to format with: the supported locale with the user's region, variants and extensions.

```c
// supported: what the program has translations for, the default first
mlocMatchEntry entries[3];
mlocMatch match = {-1, -1};
mlocLocale chosen = supported[0];
mlocLocale use = supported[0];
if (mlocPrepareMatch(&set, supported, 3, &supported[0], entries) == mloc_success &&
    mlocMatchLocales(&set, supported, entries, 3, desired, 2, nullptr, &match) ==
        mloc_success &&
    match.supported >= 0)
{
    chosen = supported[match.supported];
    if (mlocCombineMatch(&desired[match.desired], &chosen, &use) != mloc_success)
    {
        use = chosen;
    }
}
// chosen picks the translations, use the formats
```

`mlocPrepareMatch` does the work that depends only on the supported list; keep its entries while the list stays the same. When nothing is near enough, `match.supported` is -1 and the default stays. The options can weigh scripts above languages, stop demoting later preferences, or allow only matches that hold both ways.

The library reads no setting of the operating system, so the program passes the user's languages. A windowing library usually gives them as one list of tags, most preferred first; Maul Window's `mwinGetPreferredLocales`, for one, gives them comma-separated. Canonicalize each, skip the ones that are not locales, and match the rest; an empty list means the platform does not say, and the program's default stays. Match again when the platform reports a change (Maul Window's `mwin_eventLocaleChanged`).

```c
// list: the user's languages, most preferred first, comma-separated, as
// Maul Window's mwinGetPreferredLocales gives them
mlocLocale desired[8];
size_t count = 0;
for (size_t start = 0; start < length && count < 8;)
{
    const char* comma = memchr(list + start, ',', length - start);
    size_t end = comma != nullptr ? (size_t)(comma - list) : length;
    mlocTextResult parsed =
        mlocCanonicalizeLocale(&set, list + start, end - start, &desired[count], nullptr);
    if (parsed.status == mloc_success)
    {
        count++; // a tag that does not parse is skipped
    }
    start = end + 1;
}
// count 0: the platform did not say, so the program's default stays;
// else match desired[0] to desired[count - 1] as above
```

## Regional preferences

`mlocGetLocalePreferences` gives what a region prefers: the hour cycle and the hour formats it accepts, the first day of the week and the days of a year's first week, the weekend, the measurement system, the paper size and the calendar. The region is the locale's `-u-rg` key, else its region, else its likely region (`en` is the United States). Keys in the locale override: `-u-hc`, `-u-fw`, `-u-ms` and `-u-ca`.

```c
mlocLocalePreferences preferences;
mlocResult found = mlocGetLocalePreferences(&set, &locale, &preferences);
if (found == mloc_success)
{
    // en-GB: preferences.hourCycle is mloc_hourFormatH23,
    // preferences.firstDay 1 (Monday), measurementSystem mloc_measurementUk
}
```

## Display names

With `names.mloc` in the set, `mlocGetName` names a language, script, region, variant, key, key value or measurement system by its code in a locale ("Deutsch", "Latein", "Vereinigte Staaten"), with `subdivisions.mloc` a subdivision too. `mlocGetLocaleName` names a whole locale by UTS #35's algorithm: "British English", or with standard language display "English (United Kingdom)", and the extensions after it ("German (Germany, Phonebook Sort Order)").

```c
char text[128];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
mlocLocaleNameOptions options = {.capitalization = mloc_capitalizationListOrMenu};
if (mlocGetLocaleName(&set, &displayLocale, &named, &options, &output) == mloc_success)
{
    // displayLocale fr, named en-GB: "Anglais britannique"
}
```

Names come in several forms where CLDR has them: short ("US"), long, variant, the form a sorted menu lists, and the core and extension a two-level menu shows. The capitalization option titlecases the first letter where the locale's data asks for it in that position, as French does for language names in a menu but not mid-sentence. A missing name falls back to its code with `mloc_fellBack`, or to nothing.
