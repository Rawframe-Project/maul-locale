// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// XLIFF 2.2 exchange (record mloc-0060), for translators who work in CAT
// tools. An export holds one translation: a unit per key the source asks
// to translate, its key the unit's name, its MF2 text one segment whose
// pattern text is text and whose placeholders, markup and syntax are
// codes (<ph>, <sc> and <ec>) over the unit's original data, so a tool
// protects them and joining text and data gives the message back. The
// source entry's comment and @param lines are notes; @mloc-max-length is
// the Size and Length Restriction module's sizeRestriction in code points.
// A translation is the segment's target, state "translated", or
// "initial" when stale. A selection message the Plural, Gender and
// Select module can carry is instead a segment per case (record
// mloc-0061, xliff_pgs.h). An import reads each target back, as PO's
// does (exchange_po.h), folding a PGS unit's segments into one message.

#ifndef MLOCCAT_EXCHANGE_XLIFF_H
#define MLOCCAT_EXCHANGE_XLIFF_H

#include <stdio.h>

// Writes the XLIFF file of a translation; 0 when the set does not read or
// the file could not be written.
int CatalogExportXliff(const char* source, const char* translation, const char* xliff,
                       FILE* report);

// Reads an XLIFF file into the translation; 0, and nothing written, when
// the set or the file does not read, a unit has no name, a code names no
// data, a translation does not compile, or a PGS unit does not fold (a
// switch not the source's, a segment without a case or of another count
// of values, a unit translated in part).
int CatalogImportXliff(const char* source, const char* translation, const char* xliff,
                       FILE* report);

#endif
