// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's data: the compiled-in data and data files the program
// holds, each opened once into an mlocData view and read in place. The
// library reads no file and allocates nothing; the bytes a view was opened
// from must outlive it and everything made from it, and not change.

#ifndef MAUL_LOCALE_DATA_H
#define MAUL_LOCALE_DATA_H

#include "maul-locale/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // An opened source of data: compiled-in data or a data file. It is
    // copyable and holds no resource; nothing closes it.
    typedef struct mlocData
    {
        uint64_t opaque[4];
    } mlocData;

    // The sources a part looks data up in, searched in order: for example a
    // downloaded language pack before the compiled-in data. The caller
    // keeps the array. A message formatter, a zone formatter, and a date,
    // time or date-time formatter made with a zone keep a pointer to the
    // set itself, so the set and its array must outlive them and stay as
    // they were.
    typedef struct mlocDataSet
    {
        const mlocData* sources;
        size_t count;
    } mlocDataSet;

    // What a data file was generated from. A library opens only data of
    // its own data version, every field equal, but for a zones file (the zones
    // alone, written by mlocgen zonesfile), whose tzdb release may differ.
    typedef struct mlocDataVersion
    {
        mlocVersion cldr;
        mlocVersion unicode;
        // The data format version the data was written in
        // (MLOC_DATA_FORMAT_VERSION).
        uint32_t format;
        // The time zone database release, such as "2026e", padded with
        // NULs; all eight bytes are used when the name is that long.
        char tzdb[8];
        // A hash of the code tables generated with the data and of the
        // pinned sources.
        uint64_t tables;
    } mlocDataVersion;

    /// Returns the data version this library reads.
    ///
    /// @param versionOut  Receives the version.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_API void mlocGetDataVersion(mlocDataVersion* versionOut);

    /// Reads the version of a data file from its header alone, so that a
    /// program can tell which file to fetch before it opens one.
    ///
    /// @param bytes       The file's first bytes; at least 64.
    /// @param size        Their size.
    /// @param versionOut  Receives the version; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer;
    ///         mloc_errorFormat when the bytes are no data file header of a
    ///         format this library knows.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocReadDataVersion(const void* bytes, size_t size,
                                                           mlocDataVersion* versionOut);

    /// Opens a data file held in memory. The whole file is validated here,
    /// once; a view that opened is never checked again. So the bytes must
    /// not change while the view or anything made from it is used: after
    /// a change, any call that reads them may read outside the file or
    /// give any answer, and a build with assertions on may stop with a
    /// trap.
    ///
    /// @param bytes    The file, 8-byte aligned (any allocator's memory is).
    /// @param size     The file's size.
    /// @param dataOut  Receives the view; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or bytes
    ///         that are not 8-byte aligned; mloc_errorVersion for a
    ///         file of another data version (mlocReadDataVersion names
    ///         it), a zones file's release aside; mloc_errorFormat for
    ///         bytes that are not a valid data file.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocOpenData(const void* bytes, size_t size,
                                                    mlocData* dataOut);

    /// Opens the data compiled into the library.
    ///
    /// @param dataOut  Receives the view.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_API void mlocOpenBuiltinData(mlocData* dataOut);

    /// Returns the number of locales a source has data for.
    ///
    /// @param data  An opened source.
    /// @return The count.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_API size_t mlocData_GetLocaleCount(const mlocData* data);

    /// Returns one locale a source has data for, as CLDR names it
    /// ("de_AT", "sr_Latn"), in the order of their bytes.
    ///
    /// @param data       An opened source.
    /// @param index      Below mlocData_GetLocaleCount.
    /// @param idOut      Receives the id, which is not NUL-terminated and
    ///                   lives as long as the source's bytes; left
    ///                   unchanged on failure.
    /// @param lengthOut  Receives its length in bytes; left unchanged on
    ///                   failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or an
    ///         index out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocData_GetLocale(const mlocData* data, size_t index,
                                                          const char** idOut, size_t* lengthOut);

#ifdef __cplusplus
}
#endif

#endif
