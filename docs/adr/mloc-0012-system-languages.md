# mloc-0012. The system's languages come from Maul Window

Status: Accepted

## Context

The library makes no operating system call. The user's preferred
languages, and the moment they change, are known only to the platform,
and Maul Window already speaks to every platform the family supports.

## Decision

Maul Window reports the user's preferred languages in order, as BCP 47
strings, and an event when they change. Maul Locale parses them and
matches them against a program's available locales with CLDR's
language distances.

## Consequences

The library stays free of platform code, and programs without Maul
Window pass the languages they obtain themselves.
