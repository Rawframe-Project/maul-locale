// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Little-endian loads from data bytes, the same on every host: each is
// assembled from its bytes, which compilers fold into one load on a
// little-endian machine.

#ifndef MLOC_BYTES_H
#define MLOC_BYTES_H

#include <stdint.h>

static inline uint16_t mlocLoad16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t mlocLoad32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t mlocLoad64(const uint8_t* p)
{
    return (uint64_t)mlocLoad32(p) | ((uint64_t)mlocLoad32(p + 4) << 32);
}

#endif
