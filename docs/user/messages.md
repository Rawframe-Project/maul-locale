# Messages

A program's user-visible text is written as messages in Unicode MessageFormat 2 (UTS #35 Part 9), which choose a variant by plural category or any other value and format numbers, dates and lists in the locale. `maul-locale/message.h` compiles, binds and formats them; `maul-locale/list.h` joins lists. [Catalogs](catalogs.md) hold a program's messages per locale.

## Compile, bind, format

A message goes through three steps, each into caller memory:

1. `mlocCompileMessage` reads the MF2 text and checks its syntax and data model once. The compiled form is locale-free and is what a catalog stores.
2. `mlocMessageFormatter_Bind` binds a compiled message to a formatter of a locale: functions are found and the formatters and plural rules the message needs are made once. The bound message points into the compiled message and the formatter, so its buffer lies apart from both; a buffer that overlaps either is refused. The bind checks the message once, so the compiled message, the formatter and the bound message all stay as they were while the bound message is used; a formatter made again in the same variable is another one.
3. `mlocFormatMessage` formats a bound message with named arguments, as often as needed and from any thread.

Each step measures with a null buffer and a capacity of 0.

```c
static const char text[] = ".input {$count :integer}\n"
                           ".match $count\n"
                           "one {{You have {$count} new message.}}\n"
                           "* {{You have {$count} new messages.}}";
uint8_t compiled[1024];
uint8_t bound[2048];
size_t compiledSize = 0;
size_t boundSize = 0;
mlocMessageError error = mloc_messageErrorNone;
mlocMessageFormatter formatter;
mlocTextResult result = mlocCompileMessage(text, sizeof(text) - 1, compiled, sizeof(compiled),
                                           &compiledSize, &error);
if (result.status == mloc_success &&
    mlocInitMessageFormatter(&set, &locale, nullptr, &formatter) == mloc_success &&
    mlocMessageFormatter_Bind(&formatter, compiled, compiledSize, bound, sizeof(bound),
                              &boundSize) == mloc_success)
{
    mlocMessageArgument count = {
        "count", 5, {.kind = mloc_messageValueInteger, .as.integer = 3}};
    char out[128];
    mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
    mlocResult formatted = mlocFormatMessage(bound, &count, 1, &output, nullptr);
    // formatted is mloc_success; out holds "You have 3 new messages."
    (void)formatted;
}
```

A message that does not compile reports `mloc_errorSyntax` with the offset and the kind of error. A syntax error is found while measuring and leaves the buffer as it was; a data model error, or a message past the chain and resolution bounds, is found only once the compiled form is written, and the bytes written are cleared, so the buffer holds no compiled message. `mlocWriteMessageText` writes a compiled message back as canonical MF2 text.

## Arguments and functions

An argument is a string, an integer, a double, a decimal, a currency amount, a unit amount, a date, a time, a date-time, an instant, a zoned date-time, a duration, a list, or an object of the program's own. The functions of the specification are all there: `:string`, `:number`, `:integer`, `:offset`, `:percent`, `:currency`, `:unit`, `:date`, `:time` and `:datetime`, with their options. The library adds its own in the `mloc:` namespace:

| Function | Writes |
|---|---|
| `mloc:spellout` | A number in words or by a numbering system's rules (`spellout.mloc`). |
| `mloc:relative` | A relative time, "in 3 days" (`relative.mloc`). |
| `mloc:duration` | A duration, "1 hr, 2 min" (`units.mloc`). |
| `mloc:list` | A list, "Ana, Bo and Cy". |
| `mloc:case`, `mloc:particle`, `mloc:gender` | A name or number with a case suffix or particle, or a value's gender for selection; see [Grammar](grammar.md). |

A program adds functions of its own in the formatter's options, each under a namespace of its own (`app:icon`). Its resolve entry reads the call's operand and options (`mlocMessageCall_GetOption`, `mlocMessageCall_FindOption`) and writes the value; it may format, match and compare like the built-in ones. Its entries must give the same text for the same value within one call, which formats a message again to report its parts and one over 512 bytes to write it; a text whose length differs the second time refuses the call with `mloc_errorInvalid`, the buffer cleared, and any parts already reported are then void; no part reported ends past the text first written. The functions array and each entry outlive the formatter and stay as they were, and a function changes none of the call's arguments: the call checks them once, before it formats. A call refuses with `mloc_errorInvalid` a function past the formatter's own, without a resolve entry or selecting without the match entry it had, and an argument a function changed before the text is formatted again, which clears the buffer.

## Faults and fallbacks

When part of a message fails (an argument is missing, a function refuses its operand), the format call writes the rest with the specification's fallback for that part, such as `{$name}`, and returns `mloc_withFallbacks`. The faults, with the expression each comes from, go to an `mlocMessageFaults` when one is given. A formatter made with the strict option then returns `mloc_errorFallback` and leaves the output, its buffer and length, as it was, so text written there earlier stays.

## Direction and parts

A placeholder's text is isolated by the specification's default bidi strategy: a right-to-left name inside a left-to-right message keeps its own order. The formatter's options can turn isolation off where the program isolates itself. With the output's part function set, a format call reports each literal text, placeholder, fallback and markup with its byte range, and `mlocGetMessagePartInfo` tells what each one names; markup writes nothing, so a program can style or link a range.

## Lists

`mlocListFormatter_Format` joins items with a locale's patterns: and, or or unit lists, wide, short or narrow ("Ana, Bo, and Cy", "Ana, Bo, or Cy", "5 ft, 6 in"), with the contextual changes some languages make (Spanish "y" before "i" becomes "e"). `mlocGetListGender` gives the gender of a list of persons by the locale's rule, for selecting a message's variant.

```c
mlocListItem items[3] = {{"Ana", 3}, {"Bo", 2}, {"Cy", 2}};
mlocListFormatter formatter;
char text[64];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitListFormatter(&set, &locale, nullptr, &formatter) == mloc_success &&
    mlocListFormatter_Format(&formatter, items, 3, &output) == mloc_success)
{
    // en: "Ana, Bo, and Cy"; de: "Ana, Bo und Cy"
}
```
