// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalogs: a program's messages of one locale, compiled by the catalog
// tool (mloccat) into a catalog file, found by key through a minimal perfect
// hash or by an index resolved once. The catalogs of one set (a source and
// its translations) share one key order, so an index means the same
// message in each. A chain formats a set's messages through catalogs in
// fallback order, binding each message once into a cache of the caller's
// memory.

#ifndef MAUL_LOCALE_CATALOG_H
#define MAUL_LOCALE_CATALOG_H

#include "maul-locale/locale.h"
#include "maul-locale/message.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // An opened catalog: a view of its bytes, which must outlive it.
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

    /// Opens a catalog the catalog tool compiled, from its bytes, checking
    /// all of them: its keys, its messages and that they agree. A catalog
    /// names nothing in the data by index or version, so it opens with a
    /// library of other data (another CLDR or tzdb release); its messages
    /// are formatted with a formatter's data and locale.
    ///
    /// @param bytes       The catalog's bytes; any alignment.
    /// @param size        Their length.
    /// @param catalogOut  Receives the catalog; left unchanged on failure.
    ///                    It points into the bytes, which must outlive it.
    /// @return mloc_success; mloc_errorVersion for a catalog a later
    ///         version wrote (another catalog format, a flag or field this
    ///         one does not know) or one holding a message it does not
    ///         read as its version; mloc_errorFormat for bytes that are no
    ///         catalog: a field missing, keys and messages that disagree, a
    ///         locale that is not a locale id; mloc_errorInvalid for a null
    ///         pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocOpenCatalog(const void* bytes, size_t size,
                                                       mlocCatalog* catalogOut);

    /// Reads what a catalog holds.
    ///
    /// @param catalog  A catalog mlocOpenCatalog opened.
    /// @param infoOut  Receives its set id, counts, kind and locale.
    /// @return mloc_success; mloc_errorInvalid for a catalog the library
    ///         did not open or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocCatalog_GetInfo(const mlocCatalog* catalog,
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
    MLOC_NODISCARD MLOC_API mlocResult mlocCatalog_FindKey(const mlocCatalog* catalog,
                                                           const char* key, size_t length,
                                                           uint32_t* indexOut);

    /// Gives a key's compiled message, as mlocCompileMessage writes one,
    /// to bind and format (message.h).
    ///
    /// @param catalog     A catalog mlocOpenCatalog opened.
    /// @param index       A key's index (mlocCatalog_FindKey), the same in
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
    MLOC_NODISCARD MLOC_API mlocResult mlocCatalog_GetMessage(const mlocCatalog* catalog,
                                                              uint32_t index,
                                                              const void** messageOut,
                                                              size_t* sizeOut);

    // The most catalogs a chain holds; more are refused with
    // mloc_errorLimit.
#define MLOC_MAX_CATALOG_CHAIN 8

    // A chain: catalogs of one set in fallback order ("fr-CA", "fr", the
    // source "en"), each with its locale's formatter, and the cache their
    // messages are bound into. Opaque; make it with mlocInitCatalogChain.
    typedef struct mlocCatalogChain
    {
        uint64_t opaque[8];
    } mlocCatalogChain;

    // Which text a chain wrote, and why.
    typedef int32_t mlocCatalogReason;
    enum
    {
        // The message of the chain's first catalog.
        mloc_catalogFound = 0,
        // A later catalog's message: the ones before it lack it (a
        // translation not yet made).
        mloc_catalogFallback = 1,
        // The source's message: the chain's did not bind (mlocMessageFormatter_Bind
        // refused it).
        mloc_catalogSource = 2,
        // The key: the set has no such key.
        mloc_catalogMissingKey = 3,
        // The key: no catalog of the chain has the message (the source is
        // not in the chain).
        mloc_catalogAbsent = 4,
        // The key: the message did not bind, and neither did the source's
        // or the source is not in the chain.
        mloc_catalogFailed = 5,
        // The key: the message does not fit in the cache's room left.
        mloc_catalogCacheFull = 6,
    };

    // A chain's account of a format call.
    typedef struct mlocCatalogResult
    {
        mlocCatalogReason reason;
        // The chain position whose message was written; the chain's count
        // when the key was.
        uint32_t position;
    } mlocCatalogResult;

    /// Measures the cache that binds every message of a chain, so that
    /// none is ever written as its key for want of room: the head, eight
    /// bytes per key of the set, and each message bound as the chain binds
    /// it.
    ///
    /// @param catalogs    The catalogs, in fallback order.
    /// @param formatters  Each catalog's formatter, of its locale.
    /// @param count       Their number, 1 to MLOC_MAX_CATALOG_CHAIN.
    /// @param sizeOut     Receives the size in bytes.
    /// @return mloc_success; mloc_errorLimit and mloc_errorInvalid as
    ///         mlocInitCatalogChain.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocMeasureCatalogChain(const mlocCatalog* catalogs, const mlocMessageFormatter* formatters,
                            size_t count, size_t* sizeOut);

    /// Makes a chain. Its messages are bound when first formatted, or all
    /// at once by mlocCatalogChain_Bind. The catalogs, formatters and cache
    /// must outlive the chain, and the cache serves this chain alone.
    ///
    /// @param catalogs    The catalogs, in fallback order, of one set.
    /// @param formatters  Each catalog's formatter, of its locale.
    /// @param count       Their number, 1 to MLOC_MAX_CATALOG_CHAIN.
    /// @param cache       The cache's memory; any alignment.
    /// @param cacheSize   Its size: at least 16 bytes and 8 per key of the
    ///                    set; mlocMeasureCatalogChain gives the size with
    ///                    room for every message. Bound messages use at
    ///                    most 4 GiB of it.
    /// @param chainOut    Receives the chain; left unchanged on failure.
    /// @return mloc_success; mloc_errorCapacity for a cache smaller than
    ///         its head and slots; mloc_errorLimit for more than
    ///         MLOC_MAX_CATALOG_CHAIN catalogs; mloc_errorInvalid for a null
    ///         pointer, a count of 0, a catalog the library did not open, or
    ///         catalogs of different sets. A formatter the library did not
    ///         make binds nothing, so its catalog's messages fall back as
    ///         ones that do not bind.
    /// @par Thread safety
    /// Safe from any thread; the cache is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitCatalogChain(const mlocCatalog* catalogs,
                                                            const mlocMessageFormatter* formatters,
                                                            size_t count, void* cache,
                                                            size_t cacheSize,
                                                            mlocCatalogChain* chainOut);

    /// Binds every message of a chain now. Once it succeeds the cache is
    /// complete: format calls only read it, so threads may format with
    /// the chain at once.
    ///
    /// @param chain  A chain from mlocInitCatalogChain.
    /// @return mloc_success; mloc_errorCapacity when a message did not fit
    ///         (the messages that did stay bound, and the chain binds
    ///         lazily still); mloc_errorInvalid for a chain the library did
    ///         not make.
    /// @par Thread safety
    /// Safe from any thread; the chain is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocCatalogChain_Bind(const mlocCatalogChain* chain);

    /// Formats a key's message through a chain: the first catalog that
    /// has it, bound on first use. Every failure writes a stated text (see
    /// mlocCatalogReason): a message with fallbacks writes its own text
    /// and faults, as mlocFormatMessage does; one that does not bind falls
    /// to the source's message, then to the key. Text written as the key
    /// is reported as one fallback part.
    ///
    /// @param chain          A chain from mlocInitCatalogChain.
    /// @param index          The key's index (mlocCatalog_FindKey).
    /// @param arguments      The arguments; may be null when count is 0.
    /// @param argumentCount  Their number.
    /// @param textOut        Receives the text.
    /// @param faultsOut      Receives the faults; may be null.
    /// @param resultOut      Receives the reason and position once the text
    ///                       is written; left unchanged on failure. May be
    ///                       null.
    /// @return As mlocFormatMessage, and mloc_success when the key is
    ///         written; mloc_errorInvalid also for an index past the set's
    ///         keys or a chain the library did not make.
    /// @par Thread safety
    /// Safe from any thread; the chain is used by one thread at a time,
    /// until mlocCatalogChain_Bind has succeeded; then by any number at
    /// once.
    MLOC_NODISCARD MLOC_API mlocResult mlocCatalogChain_FormatMessage(
        const mlocCatalogChain* chain, uint32_t index, const mlocMessageArgument* arguments,
        size_t argumentCount, mlocOutput* textOut, mlocMessageFaults* faultsOut,
        mlocCatalogResult* resultOut);

    /// Finds a key in the chain's set and formats its message, as
    /// mlocCatalogChain_FormatMessage; a key the set does not have is written
    /// as itself (mloc_catalogMissingKey).
    ///
    /// @param chain          A chain from mlocInitCatalogChain.
    /// @param key            The key, UTF-8; may be null when length is 0.
    /// @param length         Its length in bytes.
    /// @param arguments      The arguments; may be null when count is 0.
    /// @param argumentCount  Their number.
    /// @param textOut        Receives the text.
    /// @param faultsOut      Receives the faults; may be null.
    /// @param resultOut      Receives the reason and position once the text
    ///                       is written; left unchanged on failure. May be
    ///                       null.
    /// @return As mlocCatalogChain_FormatMessage.
    /// @par Thread safety
    /// Safe from any thread; the chain is used by one thread at a time,
    /// until mlocCatalogChain_Bind has succeeded; then by any number at
    /// once.
    MLOC_NODISCARD MLOC_API mlocResult mlocCatalogChain_FormatKey(
        const mlocCatalogChain* chain, const char* key, size_t length,
        const mlocMessageArgument* arguments, size_t argumentCount, mlocOutput* textOut,
        mlocMessageFaults* faultsOut, mlocCatalogResult* resultOut);

#ifdef __cplusplus
}
#endif

#endif
