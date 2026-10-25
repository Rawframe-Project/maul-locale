// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmenter's filters (segmenter.h): the suppressions, as ICU4C's
// FilteredBreakIterator applies them (segment_tables.h), and Japanese's
// rule 13.3. The text at hand is the window, the last bytes of the
// earlier pieces, then the current piece.

#include "bytes.h"
#include "invariant.h"
#include "segmenter.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"

#include <string.h>

// How far before a break the suppressions read: the space skipped and
// the longest entry.
enum
{
    lookBack = segmentMaxSuppression + 2,
};

static size_t Start(const Segmenter* s)
{
    return s->pieceStart - s->windowLength;
}

static size_t End(const Segmenter* s)
{
    return s->pieceStart + s->pieceLength;
}

// The byte at an offset of the whole text, or -1 when it is not at hand.
static int ByteAt(const Segmenter* s, size_t offset)
{
    if (offset >= s->pieceStart)
    {
        size_t i = offset - s->pieceStart;
        return i < s->pieceLength ? (uint8_t)s->piece[i] : -1;
    }
    size_t back = s->pieceStart - offset;
    return back <= s->windowLength ? s->window[s->windowLength - back] : -1;
}

// The code point at an offset, as Maul Unicode reads it (a maximal
// ill-formed subpart is one U+FFFD): its size, 0 at the end of the text,
// or -1 when it needs text not yet fed.
static int DecodeAt(const Segmenter* s, size_t offset, uint32_t* codePointOut)
{
    char bytes[4];
    size_t n = 0;
    while (n < sizeof(bytes) && ByteAt(s, offset + n) >= 0)
    {
        bytes[n] = (char)ByteAt(s, offset + n);
        n++;
    }
    if (n == 0)
    {
        return s->moreFollows ? -1 : 0;
    }
    size_t size = 0;
    muniResult r = muniDecodeUtf8(bytes, n, codePointOut, &size);
    // A sequence cut by the end of the text at hand may go on.
    if (r != muni_success && size == n && n < sizeof(bytes) && s->moreFollows)
    {
        return -1;
    }
    return (int)size;
}

// A code point's class in the segmenter's property, its overrides first.
static uint8_t ClassOf(const Segmenter* s, uint32_t codePoint)
{
    for (uint32_t i = 0; i < s->overrideCount; i++)
    {
        if (s->overrides[i].codePoint == codePoint)
        {
            return s->overrides[i].value;
        }
    }
    return s->kind == mloc_segmentWord ? muniGetWordBreak(codePoint)
                                       : muniGetSentenceBreak(codePoint);
}

static bool IsHiragana(uint32_t codePoint)
{
    return muniGetScript(codePoint) == MUNI_SCRIPT('H', 'i', 'r', 'a');
}

// Moves rule 13.3's cursor up to an offset, as far as the text goes.
static void AdvanceCursor(Segmenter* s, size_t offset)
{
    while (s->cursor < offset)
    {
        uint32_t codePoint = 0;
        int size = DecodeAt(s, s->cursor, &codePoint);
        if (size <= 0)
        {
            return;
        }
        uint8_t c = ClassOf(s, codePoint);
        if (c != muni_wbExtend && c != muni_wbFormat && c != muni_wbZwj)
        {
            s->cursorHiragana = IsHiragana(codePoint);
        }
        s->cursor += (size_t)size;
    }
}

// Rule 13.3, $Hiragana × $Hiragana with $Hiragana = (\p{Hiragana} $FE*).
static int KeepsHiragana(Segmenter* s, size_t offset)
{
    if (offset < s->cursor)
    {
        // Held back past the window: before a mid-word character.
        return 1;
    }
    AdvanceCursor(s, offset);
    MLOC_ASSERT(s->cursor == offset);
    uint32_t codePoint = 0;
    int size = DecodeAt(s, offset, &codePoint);
    if (size < 0)
    {
        return -1;
    }
    return size == 0 || !s->cursorHiragana || !IsHiragana(codePoint);
}

static uint32_t EntryStart(const Segmenter* s, uint32_t i)
{
    return mlocLoad32(s->offsets + 4 * (size_t)i);
}

static uint32_t EntryLength(const Segmenter* s, uint32_t i)
{
    return mlocLoad32(s->offsets + 4 * (size_t)i + 4) - EntryStart(s, i);
}

// Whether the text before an offset ends with an entry.
static bool EndsWith(const Segmenter* s, size_t offset, uint32_t i)
{
    uint32_t length = EntryLength(s, i);
    if (length > offset)
    {
        return false;
    }
    const uint8_t* entry = s->text + EntryStart(s, i);
    for (uint32_t k = 0; k < length; k++)
    {
        if (ByteAt(s, offset - length + k) != entry[k])
        {
            return false;
        }
    }
    return true;
}

// The longest backward entry the text before an offset ends with, or
// UINT32_MAX: the entry ICU4C's backward trie walk stops at.
static uint32_t LongestBackward(const Segmenter* s, size_t offset)
{
    uint32_t best = UINT32_MAX;
    for (uint32_t i = 0; i < s->backward; i++)
    {
        if ((best == UINT32_MAX || EntryLength(s, i) > EntryLength(s, best)) &&
            EndsWith(s, offset, i))
        {
            best = i;
        }
    }
    return best;
}

// ICU4C's forward walk from an offset: 1 when the break stays, 0 when it
// is suppressed, -1 when it needs text not yet fed. The forward entries
// that begin with the bytes read, a range of them since they are sorted,
// are the trie's node. At the end of a code point the walk stops where no
// entry goes on, suppressing at an entry (FINAL_VALUE); at a byte no entry
// goes on with it stops and keeps the break (NO_MATCH); at the end of the
// text it suppresses only right after an entry (INTERMEDIATE_VALUE).
// Whether an entry goes on with a byte after the bytes read.
static bool Takes(const Segmenter* s, uint32_t i, uint32_t read, int byte)
{
    return EntryLength(s, i) > read && s->text[EntryStart(s, i) + read] == byte;
}

static int ForwardWalk(const Segmenter* s, size_t from)
{
    uint32_t lo = s->backward;
    uint32_t hi = s->backward + s->forward;
    for (uint32_t read = 0;; read++)
    {
        if (lo == hi)
        {
            return 1;
        }
        // An entry the bytes read are sorts first among those that begin
        // with them; the bytes end a code point when one is an entry or the
        // next byte of the first longer one begins a code point, since
        // entries are valid UTF-8.
        bool value = EntryLength(s, lo) == read;
        bool goesOn = hi - lo > (value ? 1u : 0u);
        bool boundary = value || (s->text[EntryStart(s, lo) + read] & 0xC0) != 0x80;
        if (read > 0 && boundary && !goesOn)
        {
            return 0;
        }
        int byte = ByteAt(s, from + read);
        if (byte < 0)
        {
            return s->moreFollows ? -1 : (value ? 0 : 1);
        }
        // Those that go on with the byte are contiguous, sorted by it.
        while (lo < hi && !Takes(s, lo, read, byte))
        {
            lo++;
        }
        uint32_t end = lo;
        while (end < hi && Takes(s, end, read, byte))
        {
            end++;
        }
        hi = end;
    }
}

// Whether the suppressions keep a break whose text is at hand.
static int SuppressionKeeps(const Segmenter* s, size_t offset)
{
    size_t at = offset;
    if (at > 0 && ByteAt(s, at - 1) == ' ')
    {
        at--;
    }
    uint32_t best = LongestBackward(s, at);
    if (best == UINT32_MAX)
    {
        return 1;
    }
    if (mlocLoad32(s->values + 4 * (size_t)best) == segmentSuppressionMatch)
    {
        return 0;
    }
    return ForwardWalk(s, at - EntryLength(s, best));
}

int mlocSegmentKeeps(Segmenter* s, size_t offset)
{
    if (offset == End(s) && !s->moreFollows)
    {
        // A break at the end of the text is never suppressed.
        return 1;
    }
    if ((s->filters & segmentHiragana) != 0)
    {
        return KeepsHiragana(s, offset);
    }
    if (offset - (offset < lookBack ? offset : lookBack) < Start(s))
    {
        return s->heldValid && s->held == offset && s->heldSuppressed ? 0 : 1;
    }
    return SuppressionKeeps(s, offset);
}

// Follows the terminator runs (a terminator, closing punctuation and
// spaces, with the Extend and Format characters after each) up to the end
// of the text at hand. A completed run's end is the break the iterator
// may hold back; a run a paragraph separator ends breaks after the
// separator, which the iterator never holds back.
static void ScanRuns(Segmenter* s)
{
    uint32_t codePoint = 0;
    int size = 0;
    while ((size = DecodeAt(s, s->scanned, &codePoint)) > 0)
    {
        uint8_t c = ClassOf(s, codePoint);
        size_t end = s->scanned + (size_t)size;
        s->scanned = end;
        if (c == muni_sbATerm || c == muni_sbSTerm)
        {
            s->scanRun = 1;
        }
        else if ((c == muni_sbExtend || c == muni_sbFormat || c == muni_sbSp ||
                  (c == muni_sbClose && s->scanRun == 1)) &&
                 s->scanRun != 0)
        {
            s->scanRun = c == muni_sbSp ? 2 : s->scanRun;
        }
        else
        {
            if (s->scanRun != 0 && c != muni_sbCr && c != muni_sbLf && c != muni_sbSep)
            {
                s->held = s->runEnd;
                s->heldValid = false;
            }
            s->scanRun = 0;
            continue;
        }
        s->runEnd = end;
    }
}

void mlocSegmentLeavePiece(Segmenter* s)
{
    size_t end = End(s);
    size_t total = s->windowLength + s->pieceLength;
    size_t keep = total < segmentWindow ? total : segmentWindow;
    if ((s->filters & segmentHiragana) != 0)
    {
        AdvanceCursor(s, end - keep);
    }
    if ((s->filters & segmentSuppress) != 0)
    {
        ScanRuns(s);
        // The held break's answer while its text is still at hand.
        size_t held = s->held;
        if (!s->heldValid && held > 0 && held - (held < lookBack ? held : lookBack) < end - keep)
        {
            s->heldSuppressed = SuppressionKeeps(s, s->held) == 0;
            s->heldValid = true;
        }
    }
    uint8_t window[segmentWindow];
    for (size_t i = 0; i < keep; i++)
    {
        window[i] = (uint8_t)ByteAt(s, end - keep + i);
    }
    memcpy(s->window, window, keep);
    s->windowLength = (uint16_t)keep;
    s->pieceStart = end;
    s->piece = nullptr;
    s->pieceLength = 0;
}
