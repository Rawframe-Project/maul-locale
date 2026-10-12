# mloc-0004. No part slower than the fastest implementation

Status: Accepted

## Context

On the same machine and inputs, ICU4X formats numbers six times and
dates three to four times faster than ICU4C, and builds formatters
fourteen to eighteen times faster; ICU4C compares two strings seven
times faster than ICU4X, through a table for Latin text that ICU4X
does not have.

## Decision

Every part has a benchmark against ICU4C and ICU4X with the same
inputs. No part is released slower than the faster of the two at the
same work, or with more code or data than ICU4X's for the same
locales. Formatters are built once and format many times; formatting
takes no lock and makes no system call.

## Consequences

Speed is measured per part and per release, and a regression is a
failed release criterion, not a remark.
