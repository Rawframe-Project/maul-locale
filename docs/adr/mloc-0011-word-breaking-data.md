# mloc-0011. Word breaking: dictionaries by default, models by choice

Status: Accepted

## Context

Thai, Lao, Khmer and Burmese write words without spaces, so a line
cannot wrap between words without a segmenter; Maul Unicode takes one
through a hook. ICU's dictionaries are permissively licensed but large
(about 1.7 MB compiled for the four scripts); ICU4X's models for the
same scripts take about 300 KB and run slower.

## Decision

The library provides dictionary segmenters for the four scripts and
for Chinese and Japanese word boundaries, compiled by the generator,
and model-based segmenters for the four scripts as a smaller choice.
Each is a separate data part. Every data source's notice ships with
the data in `THIRD_PARTY.md`.

## Consequences

A program chooses between size and speed per script, and every
required notice travels with the data.
