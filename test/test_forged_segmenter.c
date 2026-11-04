// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A segmenter whose stamp is right but whose stored counts are not
// (the library checks every index it stores): a queue or window
// past its room, a window longer than the text before the piece, a kind
// that is none, and overrides that are not a set of the code table whole,
// are refused by every call, which then reads none of them; so are a
// queue that starts past the text fed or fills the queue, a piece start
// moved past the text a Japanese filter reads, and a break left waiting
// after the iterator ended, each of which reached an assertion, and a
// queue filled or a break queued too far behind a waiting one; a
// segmenter the library made breaks, before and after.

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

// Forged state that passed the counts' bounds but reached an assertion:
// the queue full with its first break waiting past the text, so that the
// next break pulled is queued past it; one break queued past the text, so
// that the next one's distance wraps; a Japanese word segmenter's piece
// start moved past the text its cursor reads; and a break waiting after
// the iterator ended.
static void TestReachedAssertions(void)
{
    static mlocSegmenter made;
    static mlocSegmenter forged;
    const char* text = "He met Mr. Smith. Then he left.";
    size_t end = strlen(text);
    size_t offset = 0;
    bool mandatory = false;
    bool ok = Make("en-u-ss-standard", mloc_segmentSentence, text, true, &made);
    forged = made;
    Segmenter* s = (Segmenter*)&forged;
    s->queueCount = segmentQueue;
    s->queueBase = end + 10;
    CHECK(ok && mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
          "a full queue waiting past the text is refused");
    forged = made;
    s->queueCount = 1;
    s->queue[0] = 0;
    s->queueBase = end + 10;
    CHECK(ok && mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
          "a queue starting past the text is refused");
    // A Japanese word filter waits at the end of a piece more text
    // follows: its break, queued, waits after the iterator is said to end.
    static mlocSegmenter waiting;
    const char* piece = "\xE6\x97\xA5\xE6\x9C\xAC";
    bool made2 = Make("ja", mloc_segmentWord, piece, true, &waiting);
    forged = waiting;
    s->queueCount = 1;
    s->queue[0] = 0;
    s->queueBase = strlen(piece);
    s->iteratorDone = true;
    CHECK(made2 && mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
          "a break waiting after the iterator ended is refused");
    static mlocSegmenter japanese;
    const char* words = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x81\xAE\xE6\x96\x87";
    ok = Make("ja", mloc_segmentWord, words, false, &japanese);
    const Segmenter* filtered = (const Segmenter*)&japanese;
    CHECK(ok && filtered->filters != 0, "a Japanese word segmenter filters");
    forged = japanese;
    s->pieceStart = 100;
    s->windowLength = 0;
    CHECK(ok && mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
          "a piece start past the text the cursor reads is refused");
    forged = made;
    CHECK(mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_success,
          "the segmenter as made still breaks");
}

// A Japanese word segmenter, more text to follow, whose first queued break
// waits: the piece start moved past it, the cursor at it, so its text is
// not at hand. Each next break pulled is then queued behind it.
static bool Waiting(const char* text, uint8_t queued, mlocSegmenter* out)
{
    if (!Make("ja", mloc_segmentWord, text, true, out))
    {
        return false;
    }
    Segmenter* s = (Segmenter*)out;
    s->queueCount = queued;
    memset(s->queue, 0, sizeof(s->queue));
    s->queueBase = 0;
    s->cursor = 0;
    s->pieceStart = 50;
    s->windowLength = 0;
    return true;
}

// Forged queues that reach the queueing itself: one break short of a full
// queue, which two breaks pulled behind the waiting one would pass, and a
// break pulled further from the waiting one than a queue's distance holds.
static void TestQueueing(void)
{
    static mlocSegmenter forged;
    static char text[320];
    size_t offset = 0;
    bool mandatory = false;
    const char* words = "\xE6\x97\xA5\xE6\x9C\xAC \xE8\xAA\x9E \xE3\x81\xAE \xE6\x96\x87";
    CHECK(Waiting(words, segmentQueue - 1, &forged) &&
              mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
          "a queue one short of full, filled by breaks pulled behind its first, is refused");
    memset(text, 'a', 300);
    memcpy(text + 300, " b c", 5);
    CHECK(Waiting(text, 1, &forged) &&
              mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
          "a break pulled past a queue's distance from its first is refused");
    // A first break at the top of the offsets, its piece empty so its text
    // is not at hand: a break pulled before it is less than 256 below it
    // only by wrapping, and is refused as before the first.
    CHECK(Waiting(words, 1, &forged), "a waiting segmenter is made");
    Segmenter* s = (Segmenter*)&forged;
    s->queueBase = SIZE_MAX;
    s->pieceStart = SIZE_MAX;
    s->pieceLength = 0;
    s->cursor = SIZE_MAX;
    CHECK(mlocSegmenter_NextBreak(&forged, &offset, &mandatory) == mloc_errorInvalid,
          "a break pulled before a first at the top of the offsets is refused");
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
    TestReachedAssertions();
    TestQueueing();
    return s_failures == 0 ? 0 : 1;
}
