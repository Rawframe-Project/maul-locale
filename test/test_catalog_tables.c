// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog units' validators, given each unit alone in memory of its
// own size, so that a check left out shows as a read past the unit under
// the sanitizers as well as a unit let through. A white-box test: the
// validators are internal.

#include "catalog_tables.h"
#include "table.h"
#include "test_harness.h"

#include "maul-locale/message.h"

#include <stdlib.h>
#include <string.h>

static const char* const s_keys[] = {"menu.open", "menu.close", "title", "a"};
enum
{
    keyCount = 4,
};

typedef struct Unit
{
    uint8_t* bytes;
    uint32_t size;
} Unit;

static void Put32At(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
    {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}

static uint32_t Get32At(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// A keys unit as the tool writes one, with its set id left 0 (the
// validator does not read it).
static Unit KeysUnit(void)
{
    size_t lengths[keyCount];
    uint32_t textSize = 0;
    for (int i = 0; i < keyCount; i++)
    {
        lengths[i] = strlen(s_keys[i]);
        textSize += (uint32_t)lengths[i];
    }
    KeyTable t;
    Unit u = {0};
    if (!KeyTableBuild(s_keys, lengths, keyCount, &t))
    {
        return u;
    }
    uint32_t pilots = (t.buckets * 2 + 3) & ~3U;
    uint32_t remap = (t.places - t.count) * 4;
    uint32_t offsets = (t.count + 1) * 4;
    u.size = catalogKeysHead + pilots + remap + offsets + textSize;
    u.bytes = calloc(u.size, 1);
    if (u.bytes != nullptr)
    {
        uint8_t* p = u.bytes;
        Put32At(p, t.count);
        Put32At(p + 4, t.buckets);
        Put32At(p + 8, t.places);
        Put32At(p + 16, (uint32_t)t.seed);
        Put32At(p + 20, (uint32_t)(t.seed >> 32));
        p += catalogKeysHead;
        for (uint32_t i = 0; i < t.buckets; i++)
        {
            p[i * 2] = (uint8_t)t.pilots[i];
            p[i * 2 + 1] = (uint8_t)(t.pilots[i] >> 8);
        }
        p += pilots;
        for (uint32_t i = 0; i < t.places - t.count; i++)
        {
            Put32At(p + (size_t)i * 4, t.remap[i]);
        }
        p += remap;
        uint8_t* text = p + offsets;
        uint32_t at = 0;
        for (uint32_t index = 0; index < t.count; index++)
        {
            Put32At(p + (size_t)index * 4, at);
            for (uint32_t j = 0; j < t.count; j++)
            {
                if (t.index[j] == index)
                {
                    memcpy(text + at, s_keys[j], lengths[j]);
                    at += (uint32_t)lengths[j];
                }
            }
        }
        Put32At(p + (size_t)t.count * 4, at);
    }
    KeyTableFree(&t);
    return u;
}

// A messages unit of two compiled messages and an absent one.
static Unit MessagesUnit(void)
{
    static const char* const s_texts[] = {"Open", "Hello {$name}"};
    uint8_t compiled[2][256];
    size_t sizes[2] = {0, 0};
    mlocMessageError error;
    Unit u = {0};
    for (int i = 0; i < 2; i++)
    {
        if (mlocCompileMessage(s_texts[i], strlen(s_texts[i]), compiled[i], sizeof(compiled[i]),
                               &sizes[i], &error)
                .status != mloc_success)
        {
            return u;
        }
    }
    uint32_t count = 3;
    u.size = catalogMessagesHead + (count + 1) * 4 + (uint32_t)(sizes[0] + sizes[1]);
    u.bytes = calloc(u.size, 1);
    if (u.bytes != nullptr)
    {
        uint8_t* p = u.bytes;
        Put32At(p, count);
        uint8_t* offsets = p + catalogMessagesHead;
        uint8_t* text = offsets + (count + 1) * 4;
        Put32At(offsets + 4, (uint32_t)sizes[0]);
        Put32At(offsets + 8, (uint32_t)sizes[0]);
        Put32At(offsets + 12, (uint32_t)(sizes[0] + sizes[1]));
        memcpy(text, compiled[0], sizes[0]);
        memcpy(text + sizes[0], compiled[1], sizes[1]);
    }
    return u;
}

// The unit copied to memory of the given size, with one word set, and
// checked.
static bool Valid(Unit u, bool keys, uint32_t size, size_t at, uint32_t value)
{
    uint8_t* copy = malloc(size > 0 ? size : 1);
    if (copy == nullptr)
    {
        return true;
    }
    memcpy(copy, u.bytes, size < u.size ? size : u.size);
    if (at + 4 <= size)
    {
        Put32At(copy + at, value);
    }
    DataSpan span = {copy, size};
    bool valid =
        (keys ? mlocValidCatalogKeys(span) : mlocValidCatalogMessages(span)) == mloc_success;
    free(copy);
    return valid;
}

static void TestKeys(void)
{
    Unit u = KeysUnit();
    CatalogKeys k;
    if (u.bytes == nullptr || !mlocReadCatalogKeys((DataSpan){u.bytes, u.size}, &k) ||
        k.places <= k.count)
    {
        CHECK(false, "a keys unit is built");
        free(u.bytes);
        return;
    }
    size_t remap = (size_t)(k.remap - u.bytes);
    size_t offsets = (size_t)(k.offsets - u.bytes);
    size_t last = offsets + (size_t)k.count * 4;
    uint32_t keep = 0xFFFFFFFFU;
    CHECK(Valid(u, true, u.size, u.size, keep), "the unit as built is valid");
    CHECK(!Valid(u, true, u.size, remap, k.count) && !Valid(u, true, u.size, remap, 0xFFFFFFF0U),
          "a remap entry at the count or far past it");
    CHECK(!Valid(u, true, u.size, last, k.textSize + 1) &&
              !Valid(u, true, u.size, last, Get32At(u.bytes + last - 4) - 1) &&
              !Valid(u, true, u.size, offsets + 4, 0xFFFFFFF0U),
          "the last offset past the text or falling, a middle offset far past it");
    CHECK(!Valid(u, true, catalogKeysHead - 1, u.size, keep) &&
              !Valid(u, true, (uint32_t)last, u.size, keep),
          "a unit cut inside its head or its offsets");
    size_t text = (size_t)(k.text - u.bytes);
    uint32_t first = Get32At(u.bytes + text);
    CHECK(!Valid(u, true, u.size, text, first ^ 0x20) &&
              !Valid(u, true, u.size, text, first & 0xFFFFFF00U),
          "a key changed is not found at its index; a key holding a NUL");
    free(u.bytes);
}

static void TestMessages(void)
{
    Unit u = MessagesUnit();
    if (u.bytes == nullptr)
    {
        CHECK(false, "a messages unit is built");
        return;
    }
    size_t offsets = catalogMessagesHead;
    size_t last = offsets + 3 * 4;
    uint32_t text = u.size - (uint32_t)(catalogMessagesHead + 4 * 4);
    uint32_t keep = 0xFFFFFFFFU;
    CHECK(Valid(u, false, u.size, u.size, keep), "the unit as built is valid");
    CHECK(!Valid(u, false, catalogMessagesHead - 1, u.size, keep) &&
              !Valid(u, false, catalogMessagesHead, 0, 1),
          "a unit shorter than its head; a count whose offsets the unit lacks");
    CHECK(!Valid(u, false, u.size, last, text + 1) && !Valid(u, false, u.size, offsets + 8, 0) &&
              !Valid(u, false, u.size, offsets + 4, text + 8),
          "the last offset past the messages; offsets falling; a middle one past them");
    CHECK(!Valid(u, false, u.size, 4, 1) && !Valid(u, false, u.size, 4, 0x80000000U),
          "a reserved word that is not zero");
    free(u.bytes);
}

int main(void)
{
    TestKeys();
    TestMessages();
    return s_failures == 0 ? 0 : 1;
}
