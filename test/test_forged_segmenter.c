// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A segmenter whose stamp is right but whose stored counts are not
// (the library checks every index it stores): a queue or window
// past its room, a window longer than the text before the piece, a kind
// that is none, and overrides that are not a set of the code table whole,
// are refused by every call, which then reads none of them; a segmenter
// the library made breaks, before and after.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "segment_overrides.h"
#include "segmenter.h"
#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/segment.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 16];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};

static bool Make(const char* id, mlocSegmentKind kind, const char* text, bool moreFollows,
                 mlocSegmenter* out)
{
    mlocLocale locale;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitSegmenter(&s_data, &locale, kind, text, strlen(text), moreFollows, out) ==
               mloc_success;
}

static size_t Breaker(void* context, const char* run, size_t length, size_t from)
{
    (void)context;
    (void)run;
    (void)from;
    return length;
}

// The ways a segmenter's stored counts are forged, applied to a copy.
enum
{
    forgedQueue,
    forgedWindow,
    forgedWindowBefore,
    forgedKind,
    forgedNullOverrides,
    forgedOverrideCount,
    forgedCount,
};

static const char* const s_forgeries[forgedCount] = {
    "a queue past its room",
    "a window past its room",
    "a window longer than the text before the piece",
    "a kind that is none",
    "overrides counted with none there",
    "a set of overrides counted past its end",
};

static void Forge(Segmenter* s, int how)
{
    const SegmentOverrideSet* set = &mlocSegmentOverrideSets[0];
    switch (how)
    {
    case forgedQueue:
        s->queueCount = segmentQueue + 1;
        break;
    case forgedWindow:
        s->windowLength = segmentWindow + 1;
        s->pieceStart = segmentWindow + 1;
        break;
    case forgedWindowBefore:
        s->windowLength = (uint16_t)(s->pieceStart + 1);
        break;
    case forgedKind:
        s->kind = mloc_segmentLine + 1;
        break;
    case forgedNullOverrides:
        s->overrides = nullptr;
        s->overrideCount = 1;
        break;
    default:
        s->overrides = mlocSegmentOverrides + set->first;
        s->overrideCount = set->count + 1u;
        break;
    }
}

static void TestNextBreak(void)
{
    static mlocSegmenter made;
    static mlocSegmenter forged;
    const char* text = "He met Mr. Smith. Then he left.";
    size_t offset = 0;
    bool mandatory = false;
    bool ok = Make("en-u-ss-standard", mloc_segmentSentence, text, false, &made);
    forged = made;
    CHECK(ok && mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_success,
          "a segmenter the library made breaks");
    for (int how = 0; how < forgedCount; how++)
    {
        forged = made;
        Forge((Segmenter*)&forged, how);
        CHECK(ok && mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
              s_forgeries[how]);
    }
}

static void TestFeed(void)
{
    static mlocSegmenter waiting;
    static mlocSegmenter forged;
    const char* rest = ". Smith. Then he left.";
    size_t offset = 0;
    bool mandatory = false;
    bool ok = Make("en-u-ss-standard", mloc_segmentSentence, "He met Mr", true, &waiting);
    mlocResult r = mloc_success;
    for (int i = 0; ok && r == mloc_success && i < 8; i++)
    {
        r = mlocSegmenter_NextBreak(&waiting, &offset, &mandatory);
    }
    ok = ok && r == mloc_needMoreText;
    for (int how = 0; how < forgedCount; how++)
    {
        forged = waiting;
        Forge((Segmenter*)&forged, how);
        CHECK(ok && mlocSegmenter_Feed(&forged, rest, strlen(rest), false) == mloc_errorInvalid,
              s_forgeries[how]);
    }
    forged = waiting;
    CHECK(ok && mlocSegmenter_Feed(&forged, rest, strlen(rest), false) == mloc_success,
          "the segmenter as made takes the next piece");
}

static void TestComplexBreaker(void)
{
    static mlocSegmenter made;
    static mlocSegmenter forged;
    bool ok = Make("en", mloc_segmentWord, "abc", false, &made);
    for (int how = 0; how < forgedCount; how++)
    {
        forged = made;
        Forge((Segmenter*)&forged, how);
        CHECK(ok && mlocSegmenter_SetComplexBreaker(&forged, Breaker, nullptr) == mloc_errorInvalid,
              s_forgeries[how]);
    }
    forged = made;
    CHECK(ok && mlocSegmenter_SetComplexBreaker(&forged, Breaker, nullptr) == mloc_success,
          "the segmenter as made takes a complex breaker");
}

// A segmenter made with each set of overrides holds that set whole.
static void TestOverrideSets(void)
{
    static mlocSegmenter made;
    bool ok = Make("el-u-ss-standard", mloc_segmentSentence, "a", false, &made);
    const Segmenter* s = (const Segmenter*)&made;
    size_t offset = 0;
    bool mandatory = false;
    CHECK(ok && s->overrides != nullptr && s->overrideCount > 0 &&
              mlocSegmenter_NextBreak(&made, &offset, &mandatory) == mloc_success,
          "a segmenter with a set of overrides breaks");
}

int main(void)
{
    FILE* f = fopen(MLOC_SEGMENTS_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (size == 0 || size == sizeof(s_file) ||
        mlocOpenData(s_file, size, &s_sources[1]) != mloc_success)
    {
        fputs("test_forged_segmenter: cannot open " MLOC_SEGMENTS_FILE "\n", stderr);
        return 2;
    }
    TestNextBreak();
    TestFeed();
    TestComplexBreaker();
    TestOverrideSets();
    return s_failures == 0 ? 0 : 1;
}
