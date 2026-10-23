// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// PO exchange, for translators who work in gettext's tools. An export
// holds one translation: an entry per key the source asks to translate,
// msgctxt its key, msgid the source's MF2 text, msgstr the translation's
// (empty when it has none), flagged mf2-format, and fuzzy when stale; the
// source entry's comment and @param lines are its extracted comments. An
// import puts each translated entry back into the translation, adding the
// keys it lacked where the source has them, and records the msgid's hash
// as the source it answers, unless fuzzy.

#ifndef MLOCCAT_EXCHANGE_PO_H
#define MLOCCAT_EXCHANGE_PO_H

#include <stdio.h>

// Writes the PO file of a translation; 0 when the set does not read or
// the file could not be written.
int CatalogExportPo(const char* source, const char* translation, const char* po, FILE* report);

// Reads a PO file into the translation; 0, and nothing written, when the
// set or the PO file does not read, an entry has no msgctxt, or a
// translation does not compile.
int CatalogImportPo(const char* source, const char* translation, const char* po, FILE* report);

#endif
