# mloc-0051. The gender of a list of persons

Status: Accepted

## Context

A message that selects on gender ("{$names} is/are coming", with an
adjective agreeing) needs the gender of a list of persons. CLDR's
`genderList.xml` gives each language one of three rules for a list of
two persons or more (UTS #35 part 2, "Gender of Lists"): `neutral`
(always other), `mixedNeutral` (male or female when every person is,
else other) and `maleTaints` (female when every person is, else
male). The persons' genders are the caller's to know.

## Decision

- `mlocPersonGender`: other 0, male 1, female 2. This is a person's
  gender, kept apart from `mlocGender`, which holds CLDR's grammatical
  genders.
- `mlocGetListGender(data, locale, genders, count, genderOut)`:
  - An empty list is other and a list of one its person's gender, as
    ICU answers them.
  - A longer list follows the locale's rule.
  - A gender past female is refused.
- **The locale:**
  - It is canonicalized first with the data set's aliases, so iw takes
    he's rule, mo ro's and sh sr's.
  - It is looked up by its language and script ("zh_Hant"), then its
    language.
  - A locale CLDR does not list is neutral, as ICU's root is.
- **The rules are a table** that tools/mlocgen writes from
  `genderList.xml` into `src/generated/list_genders.c`: 55 locale IDs
  and their rules, sorted and searched by halves. The cldr CI job
  regenerates it with the other generated sources and compares.

## Proof

- `test_list_gender`:
  - Every list of up to three genders (40 lists) under each rule,
    against UTS #35's definitions written out in the test.
  - Locales of each rule, with regions, scripts, unlisted languages and
    the aliases iw, mo, sh and tl.
  - The refusals.
- `fuzz_locale` asks every parsed locale for the gender of a list made
  from the input's first bytes, invalid genders included.

## Consequences

- `mloc:gender` in messages (the next grammar slice) selects on this
  function's answer.
