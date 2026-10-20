# mloc-0068. The collation rule parser, in the generator, as ICU reads rules

Status: Accepted

## Context

CLDR's tailorings are rule strings in `common/collation/*.xml` (UTS #35
part 5 section 3). The tailoring builder of slice (d) needs them as a
list of resets, relations and settings. Tailorings are built by the
generator and nothing is built at load, so no runtime parses rules. ICU4X
has no runtime rules either.

ICU4C's `CollationRuleParser` is the reference: CLDR's files are written
for it. ICU's data build decodes `\uhhhh` and `\\` before parsing.
ja.xml says so in its own comment ("`\\` unescapes to `\` which is not
special between apostrophes").

## Decision

- **`tools/mlocgen/collation_rules.{h,c}`** parses a rule string,
  `CollationRulesParse`, into a `CollationRules` list. It refuses with
  "byte N: reason", naming the byte of the original string where the
  problem starts.
- **Two passes:**
  - **Escapes first:** `\uhhhh` (exactly four digits), `\Uhhhhhhhh`,
    `\x{h...}`, `\xhh` and `\\`. Any other backslash stays.
  - **Then the rules, as ICU's parser reads them:**
    - apostrophe quoting, with `''` an apostrophe;
    - a backslash escaping one code point outside quotes;
    - unquoted white space and ASCII syntax ending a string;
    - `#` comments to the end of the line;
    - `!` accepted and ignored, `@` as `[backwards 2]`.
- **The list:**
  - resets: a string, or one of ICU's 14 special positions (with `top`
    and `variable top` mapped as ICU maps them), and the strength of
    `[before n]`;
  - relations: `<` to `<<<<`, `=`, `;` and `,`, with prefix (`|`),
    string and extension (`/`). Starred relations give one relation a
    code point, their ranges expanded;
  - imports, as rules in their place, keeping their language tags for
    slice (d) to resolve;
  - settings in one record: strength, alternate, maxVariable,
    caseFirst, caseLevel, normalization, numericOrdering, backwards;
  - reorder codes, as Maul Unicode's packed scripts, the five groups and
    `others`;
  - the ranges of `[suppressContractions [...]]` and `[optimize [...]]`,
    each suppressed range with the count of rules before its setting,
    where the builder applies it (record mloc-0071). The written rules
    put the sets last, so a written string keeps their ranges but not
    their places.
- **ICU's refusals kept:**
  - a reset with no relation after it;
  - a `[before n]` chain whose first relation has another strength, or
    whose later relation is stronger;
  - starred text that is not NFD-inert, and a starred range that is
    backwards or reaches a surrogate or U+FFFD to U+FFFF;
  - a prefix or string that does not start at an NFC boundary;
  - a string holding a surrogate or U+FFFD to U+FFFF;
  - quoting or an escape left open;
  - unknown settings, values, positions and reorder codes;
  - `[hiraganaQ on]`.
- **Two refusals of our own,** where ICU reads something this list
  cannot hold exactly:
  - **An import after a setting.** ICU applies settings in order, so an
    imported setting could override an earlier local one. No CLDR
    string has a setting before an import.
  - **Sets beyond CLDR's form:** properties, nesting, negation and
    strings. A set here is code points and ranges.
- **`CollationRulesWrite`** writes the list back as rules: raw UTF-8,
  syntax and white space after a backslash, a backslash as four, and
  a surrogate, which a set may hold and UTF-8 cannot, as `\uhhhh`.
  The rules come first and the settings after, so the output parses
  into the same list (`CollationRulesSame`).

## Proof

- **`conform_collation_rules`, in the cldr CI job,** runs on all 135
  files of CLDR 49's `common/collation`. All 168 rule strings parse,
  every type and alt.
  - They hold 7,622 resets: 94, 9 and 55 `[before 1/2/3]`, and 18 at
    special positions.
  - They hold 284,274 relations: 638 with a prefix and 266 with an
    expansion.
  - They hold 51 imports, 44 settings, 65 reorders and 6 sets.
  - Each list, written back, parses the same.
- **`test_collation_rules`:**
  - every operator, reset form, position and setting;
  - quoting and escapes, in quotes and out;
  - comments;
  - starred relations and ranges;
  - imports in their place;
  - sets;
  - 25 refusals, each with the byte it names;
  - write-back of strings and sets holding syntax, backslashes, white
    space and surrogates.
- **`fuzz_collation_rules`,** in the fuzz job. Any bytes are parsed or
  refused at a byte inside them; what parses is written back and must
  parse the same. Its first run found a set beginning with `-` that
  was written back quoted, which sets do not read. It was fixed before
  this record. Six minutes since, about six million inputs, found
  nothing. A later CI run found a set holding `\ude00`, whose
  surrogate the writer dropped; it is now written as an escape. Five
  minutes after the fix, about eight million inputs, found nothing.

## Consequences

- Slice (d) builds tailorings from these lists: it resolves imports
  through the files it reads, finds each reset's position in the root,
  and allocates weights.
- Slice (b) checks reorder codes against the root's script table.
