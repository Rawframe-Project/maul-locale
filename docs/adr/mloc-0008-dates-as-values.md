# mloc-0008. Dates and times as values with arithmetic

Status: Accepted

## Context

JavaScript's Date mixed local times and instants, mutated in place and
read the process's zone; Temporal replaced it with distinct immutable
types and explicit zones. A local time can be skipped or repeated at a
zone transition.

## Decision

A date, a time, a date-time, an instant and a zoned date-time are
distinct immutable value types, with calendar-aware addition and
difference and conversion between zones. A skipped or repeated local
time is resolved by an explicit choice: compatible, earlier, later, or
refused. Leap seconds are not represented. RFC 9557 is the one machine
format parsed. No process zone or clock is read.

## Consequences

Date arithmetic and formatting agree, and no result depends on the
machine's settings.
