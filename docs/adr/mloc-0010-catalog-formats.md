# mloc-0010. Catalogs: a source format, a compiled form, exchange files

Status: Accepted

## Context

MessageFormat 2 defines one message, not a file of messages, and no
standard message resource format exists yet. Translators' tools
exchange XLIFF; the gettext world uses PO, whose keys are the source
text.

## Decision

Authors write catalogs in a plain text format of the library's own:
keys, MessageFormat 2 messages, and metadata for translators. A
compiler turns them into binary catalogs, compiled in or loaded, with
messages already parsed and keys in a minimal perfect hash. A tool
converts between the source format and XLIFF 2.x and PO in both
directions. Keys identify messages; a hash of the source marks
translations stale. Any number of catalogs are open at once.

## Consequences

Lookups are a hash probe with no parsing at run time, and translators
keep their tools.
