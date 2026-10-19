// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalogs (records mloc-0053 and mloc-0054): a program's messages of one
// locale, compiled by the catalog tool (mloccat) into a data file, found
// by key through a minimal perfect hash or by an index resolved once.
// The catalogs of one set (a source and its translations) share one key
// order, so an index means the same message in each.

#ifndef MAUL_LOCALE_CATALOG_H
#define MAUL_LOCALE_CATALOG_H

#include "maul-locale/data.h"
#include "maul-locale/locale.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // An opened catalog: a view of its data, which must outlive it.
    // Opaque; make it with mlocOpenCatalog.
    typedef struct mlocCatalog
    {
        uint64_t opaque[8];
    } mlocCatalog;

    // What a catalog holds.
    typedef struct mlocCatalogInfo
    {
        // The set's id, the same in every catalog compiled with it.
        uint64_t setId;
        // The keys of the set.
        uint32_t keyCount;
        // The keys this catalog has a message for.
        uint32_t messageCount;
        // Whether it is the set's source catalog.
        bool source;
        // The catalog's locale.
        mlocLocale locale;
    } mlocCatalogInfo;

    /// Opens a catalog the catalog tool compiled: the data must hold one
    /// locale with a catalog's keys and messages. Its tables were
    /// checked when the data was opened (mlocOpenData); this checks that
    /// they agree.
    ///
    /// @param catalogOut  Receives the catalog; left unchanged on failure.
    /// @param data        The catalog's data.
    /// @return mloc_success; mloc_errorNotFound for data with no catalog;
    ///         mloc_errorData for data whose keys and messages disagree or
    ///         that holds more than one catalog; mloc_errorInvalid for a
    ///         null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocOpenCatalog(mlocCatalog* catalogOut,
                                                       const mlocData* data);

    /// Reads what a catalog holds.
    ///
    /// @param catalog  A catalog mlocOpenCatalog opened.
    /// @param infoOut  Receives its set id, counts, kind and locale.
    /// @return mloc_success; mloc_errorInvalid for a catalog the library
    ///         did not open or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetCatalogInfo(const mlocCatalog* catalog,
                                                          mlocCatalogInfo* infoOut);

    /// Finds a key's index: a hash, a table read and one comparison. Keys
    /// are compared as bytes; the tool stores them in NFC.
    ///
    /// @param catalog   A catalog mlocOpenCatalog opened.
    /// @param key       The key, UTF-8; may be null when length is 0.
    /// @param length    Its length in bytes.
    /// @param indexOut  Receives the index, below the key count.
    /// @return mloc_success; mloc_errorNotFound for a key the set does not
    ///         have; mloc_errorInvalid for a catalog the library did not
    ///         open or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindCatalogKey(const mlocCatalog* catalog,
                                                          const char* key, size_t length,
                                                          uint32_t* indexOut);

    /// Gives a key's compiled message, as mlocCompileMessage writes one,
    /// to bind and format (message.h).
    ///
    /// @param catalog     A catalog mlocOpenCatalog opened.
    /// @param index       A key's index (mlocFindCatalogKey), the same in
    ///                    every catalog of the set.
    /// @param messageOut  Receives the message's first byte, in the
    ///                    catalog's data.
    /// @param sizeOut     Receives its size in bytes.
    /// @return mloc_success; mloc_errorNotFound when this catalog has no
    ///         message for the key (a translation not yet made);
    ///         mloc_errorInvalid for an index past the key count, a catalog
    ///         the library did not open or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetCatalogMessage(const mlocCatalog* catalog,
                                                             uint32_t index,
                                                             const void** messageOut,
                                                             size_t* sizeOut);

#ifdef __cplusplus
}
#endif

#endif
