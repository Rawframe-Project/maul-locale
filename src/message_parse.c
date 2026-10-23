// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_parse.h"

#include "message_text.h"

#include <stdbool.h>
#include <string.h>

typedef struct Parser
{
    const uint8_t* text;
    size_t end;
    size_t at;
    MessageEmit* emit;
    // The first syntax error's offset, or SIZE_MAX.
    size_t syntaxAt;
    // The first data model error the parse finds.
    mlocMessageError model;
    size_t modelAt;
} Parser;

static bool Fail(Parser* p)
{
    if (p->syntaxAt == SIZE_MAX)
    {
        p->syntaxAt = p->at;
    }
    return false;
}

static void ModelError(Parser* p, mlocMessageError error, size_t at)
{
    if (p->model == mloc_messageErrorNone)
    {
        p->model = error;
        p->modelAt = at;
    }
}

static bool At(const Parser* p, int c)
{
    return p->at < p->end && p->text[p->at] == (uint8_t)c;
}

static bool Expect(Parser* p, int c)
{
    if (!At(p, c))
    {
        return Fail(p);
    }
    p->at++;
    return true;
}

// The code point at the parse's position, or past U+10FFFF at the end.
static uint32_t Peek(const Parser* p, size_t* sizeOut)
{
    if (p->at >= p->end)
    {
        *sizeOut = 0;
        return 0x110000;
    }
    return mlocMessageDecode(p->text, p->at, p->end, sizeOut);
}

// Skips optional whitespace (o): spaces and bidi marks. Returns whether a
// space was among them, which makes it required whitespace (s).
static bool SkipSpace(Parser* p)
{
    bool space = false;
    for (;;)
    {
        size_t size;
        uint32_t c = Peek(p, &size);
        if (mlocMessageIsSpace(c))
        {
            space = true;
        }
        else if (!mlocMessageIsBidi(c))
        {
            return space;
        }
        p->at += size;
    }
}

// name = [bidi] name-start *name-char [bidi]: sets the name's bytes,
// without the marks; a name whose NFC form is no name is refused.
static bool ParseName(Parser* p, size_t* startOut, size_t* lengthOut)
{
    size_t size;
    uint32_t c = Peek(p, &size);
    if (mlocMessageIsBidi(c))
    {
        p->at += size;
        c = Peek(p, &size);
    }
    if (!mlocMessageIsNameStart(c))
    {
        return Fail(p);
    }
    size_t start = p->at;
    do
    {
        p->at += size;
        c = Peek(p, &size);
    } while (mlocMessageIsNameChar(c));
    *startOut = start;
    *lengthOut = p->at - start;
    // Stored in NFC, it must still be a name.
    size_t broken = mlocMessageNfcNameBreak(p->text + start, *lengthOut);
    if (broken < *lengthOut)
    {
        p->at = start + broken;
        return Fail(p);
    }
    if (mlocMessageIsBidi(c))
    {
        p->at += size;
    }
    return true;
}

static bool EmitName(Parser* p, size_t start, size_t length)
{
    if (!mlocEmitNfc(p->emit, p->text + start, length))
    {
        p->at = start;
        return Fail(p);
    }
    return true;
}

// identifier = [namespace ":"] name, written to the pool in NFC.
static bool ParseIdentifier(Parser* p, MessageString* out)
{
    size_t start = 0;
    size_t length = 0;
    MessageString string = mlocEmitBeginString(p->emit);
    if (!ParseName(p, &start, &length) || !EmitName(p, start, length))
    {
        return false;
    }
    if (At(p, ':'))
    {
        p->at++;
        mlocEmitBytes(p->emit, (const uint8_t*)":", 1);
        if (!ParseName(p, &start, &length) || !EmitName(p, start, length))
        {
            return false;
        }
    }
    *out = mlocEmitEndString(p->emit, string);
    return true;
}

// variable = "$" name: its index.
static bool ParseVariable(Parser* p, uint32_t* indexOut)
{
    size_t start = 0;
    size_t length = 0;
    if (!Expect(p, '$') || !ParseName(p, &start, &length))
    {
        return false;
    }
    if (!mlocEmitVariable(p->emit, p->text + start, length, indexOut))
    {
        p->at = start;
        return Fail(p);
    }
    return true;
}

static bool Flush(Parser* p, size_t from, bool nfc)
{
    if (nfc)
    {
        if (!mlocEmitNfc(p->emit, p->text + from, p->at - from))
        {
            p->at = from;
            return Fail(p);
        }
        return true;
    }
    mlocEmitBytes(p->emit, p->text + from, p->at - from);
    return true;
}

static bool IsEscapable(uint8_t b)
{
    return b == '\\' || b == messageOpenBrace || b == '|' || b == messageCloseBrace;
}

// The rest of a quoted literal after its "|", unescaped.
static bool ParseQuoted(Parser* p, bool nfc)
{
    size_t from = p->at;
    for (;;)
    {
        if (p->at >= p->end || p->text[p->at] == 0)
        {
            return Fail(p);
        }
        uint8_t b = p->text[p->at];
        if (b == '|')
        {
            bool flushed = Flush(p, from, nfc);
            p->at++;
            return flushed;
        }
        if (b == '\\')
        {
            if (!Flush(p, from, nfc))
            {
                return false;
            }
            p->at++;
            if (p->at >= p->end || !IsEscapable(p->text[p->at]))
            {
                return Fail(p);
            }
            mlocEmitBytes(p->emit, p->text + p->at, 1);
            from = ++p->at;
            continue;
        }
        p->at++;
    }
}

// literal = quoted-literal / unquoted-literal, its value to the pool.
static bool ParseLiteral(Parser* p, bool nfc, MessageString* out)
{
    MessageString string = mlocEmitBeginString(p->emit);
    if (At(p, '|'))
    {
        p->at++;
        if (!ParseQuoted(p, nfc))
        {
            return false;
        }
    }
    else
    {
        size_t from = p->at;
        size_t size;
        while (mlocMessageIsNameChar(Peek(p, &size)))
        {
            p->at += size;
        }
        if (p->at == from)
        {
            return Fail(p);
        }
        if (!Flush(p, from, nfc))
        {
            return false;
        }
    }
    *out = mlocEmitEndString(p->emit, string);
    return true;
}

static bool StartsLiteral(const Parser* p)
{
    size_t size;
    return At(p, '|') || mlocMessageIsNameChar(Peek(p, &size));
}

// option = identifier o "=" o (literal / variable).
static bool ParseOption(Parser* p)
{
    MessageString name;
    if (!ParseIdentifier(p, &name))
    {
        return false;
    }
    SkipSpace(p);
    if (!Expect(p, '='))
    {
        return false;
    }
    SkipSpace(p);
    uint32_t fields[5] = {parameterOptionLiteral, name.offset, name.length, 0, 0};
    if (At(p, '$'))
    {
        fields[0] = parameterOptionVariable;
        if (!ParseVariable(p, &fields[3]))
        {
            return false;
        }
    }
    else
    {
        MessageString value;
        if (!ParseLiteral(p, false, &value))
        {
            return false;
        }
        fields[3] = value.offset;
        fields[4] = value.length;
    }
    mlocEmitRecord(p->emit, messageParameters, fields);
    return true;
}

// *(s option): sets whether whitespace ends them, which attributes need.
static bool ParseOptions(Parser* p, bool* spaceOut)
{
    for (;;)
    {
        bool space = SkipSpace(p);
        size_t size;
        if (!space || !mlocMessageIsNameStart(Peek(p, &size)))
        {
            *spaceOut = space;
            return true;
        }
        if (!ParseOption(p))
        {
            return false;
        }
    }
}

// *(s attribute), attribute = "@" identifier [o "=" o literal]; ends
// after the whitespace that follows.
static bool ParseAttributes(Parser* p, bool space)
{
    while (space && At(p, '@'))
    {
        p->at++;
        MessageString name;
        if (!ParseIdentifier(p, &name))
        {
            return false;
        }
        uint32_t fields[5] = {parameterAttribute, name.offset, name.length, 0, 0};
        size_t back = p->at;
        SkipSpace(p);
        if (At(p, '='))
        {
            p->at++;
            SkipSpace(p);
            MessageString value;
            if (!ParseLiteral(p, false, &value))
            {
                return false;
            }
            fields[0] = parameterAttributeLiteral;
            fields[3] = value.offset;
            fields[4] = value.length;
        }
        else
        {
            p->at = back;
        }
        mlocEmitRecord(p->emit, messageParameters, fields);
        space = SkipSpace(p);
    }
    return true;
}

// An expression's operand: a variable, a literal, or none before a
// function.
static bool ParseOperand(Parser* p, uint32_t* fields)
{
    if (At(p, '$'))
    {
        fields[0] = operandVariable;
        return ParseVariable(p, &fields[1]);
    }
    if (At(p, ':'))
    {
        fields[0] = operandNone;
        return true;
    }
    if (!StartsLiteral(p))
    {
        return Fail(p);
    }
    MessageString literal;
    if (!ParseLiteral(p, false, &literal))
    {
        return false;
    }
    fields[0] = operandLiteral;
    fields[1] = literal.offset;
    fields[2] = literal.length;
    return true;
}

// The rest of an expression after its "{" at source: its fields (the
// record's eight) and its index.
static bool ParseExpression(Parser* p, size_t source, uint32_t* fields, uint32_t* indexOut)
{
    memset(fields, 0, 8 * sizeof *fields);
    fields[5] = p->emit->used[messageParameters];
    fields[7] = (uint32_t)source;
    if (!ParseOperand(p, fields))
    {
        return false;
    }
    bool space = fields[0] == operandNone || SkipSpace(p);
    if (At(p, ':') && space)
    {
        p->at++;
        MessageString function;
        if (!ParseIdentifier(p, &function) || !ParseOptions(p, &space))
        {
            return false;
        }
        fields[3] = function.offset;
        fields[4] = function.length;
    }
    if (!ParseAttributes(p, space) || !Expect(p, messageCloseBrace))
    {
        return false;
    }
    fields[6] = p->emit->used[messageParameters] - fields[5];
    *indexOut = mlocEmitRecord(p->emit, messageExpressions, fields);
    return true;
}

// The rest of a markup after its "{" and the spaces after it.
static bool ParseMarkup(Parser* p, size_t source, uint32_t* indexOut)
{
    uint32_t fields[6] = {At(p, '#') ? markupOpen : markupClose};
    p->at++;
    fields[3] = p->emit->used[messageParameters];
    fields[5] = (uint32_t)source;
    MessageString name;
    bool space;
    if (!ParseIdentifier(p, &name) || !ParseOptions(p, &space) || !ParseAttributes(p, space))
    {
        return false;
    }
    if (fields[0] == markupOpen && At(p, '/'))
    {
        p->at++;
        fields[0] = markupStandalone;
    }
    if (!Expect(p, messageCloseBrace))
    {
        return false;
    }
    fields[1] = name.offset;
    fields[2] = name.length;
    fields[4] = p->emit->used[messageParameters] - fields[3];
    *indexOut = mlocEmitRecord(p->emit, messageMarkups, fields);
    return true;
}

// placeholder = expression / markup, after its "{": emits its piece.
static bool ParsePlaceholder(Parser* p)
{
    size_t source = p->at - 1;
    SkipSpace(p);
    uint32_t piece[3] = {pieceExpression, 0, 0};
    bool parsed;
    if (At(p, '#') || At(p, '/'))
    {
        piece[0] = pieceMarkup;
        parsed = ParseMarkup(p, source, &piece[1]);
    }
    else
    {
        uint32_t fields[8];
        parsed = ParseExpression(p, source, fields, &piece[1]);
    }
    if (parsed)
    {
        mlocEmitRecord(p->emit, messagePieces, piece);
    }
    return parsed;
}

static void EndText(Parser* p, MessageString* text)
{
    *text = mlocEmitEndString(p->emit, *text);
    if (text->length > 0)
    {
        uint32_t piece[3] = {pieceText, text->offset, text->length};
        mlocEmitRecord(p->emit, messagePieces, piece);
    }
    *text = mlocEmitBeginString(p->emit);
}

// A run of text up to the next byte the pattern treats otherwise.
static void ParseTextRun(Parser* p)
{
    size_t from = p->at;
    while (p->at < p->end)
    {
        uint8_t b = p->text[p->at];
        if (b == messageOpenBrace || b == messageCloseBrace || b == '\\' || b == 0)
        {
            break;
        }
        p->at++;
    }
    mlocEmitBytes(p->emit, p->text + from, p->at - from);
}

// pattern, to the end of the text or, quoted, to its "}}": its pieces'
// first index and count.
static bool ParsePattern(Parser* p, bool quoted, uint32_t* pieces)
{
    pieces[0] = p->emit->used[messagePieces];
    MessageString text = mlocEmitBeginString(p->emit);
    for (;;)
    {
        if (p->at >= p->end)
        {
            if (quoted)
            {
                return Fail(p);
            }
            break;
        }
        uint8_t b = p->text[p->at];
        if (b == messageCloseBrace)
        {
            if (!quoted || p->at + 1 >= p->end || p->text[p->at + 1] != messageCloseBrace)
            {
                return Fail(p);
            }
            p->at += 2;
            break;
        }
        if (b == 0)
        {
            return Fail(p);
        }
        if (b == '\\')
        {
            p->at++;
            if (p->at >= p->end || !IsEscapable(p->text[p->at]))
            {
                return Fail(p);
            }
            mlocEmitBytes(p->emit, p->text + p->at++, 1);
            continue;
        }
        if (b != messageOpenBrace)
        {
            ParseTextRun(p);
            continue;
        }
        EndText(p, &text);
        p->at++;
        if (!ParsePlaceholder(p))
        {
            return false;
        }
        text = mlocEmitBeginString(p->emit);
    }
    EndText(p, &text);
    pieces[1] = p->emit->used[messagePieces] - pieces[0];
    return true;
}

static bool StartsQuotedPattern(const Parser* p)
{
    return p->at + 1 < p->end && p->text[p->at] == messageOpenBrace &&
           p->text[p->at + 1] == messageOpenBrace;
}

// A variant's keys, up to its quoted pattern: whether all are "*".
static bool ParseKeys(Parser* p, uint32_t selectors, size_t start, bool* catchAllOut)
{
    uint32_t keys = 0;
    bool catchAll = true;
    for (;;)
    {
        MessageString key = {0, 0};
        uint32_t kind = keyCatchAll;
        if (At(p, '*'))
        {
            p->at++;
        }
        else if (!StartsLiteral(p) || !ParseLiteral(p, true, &key))
        {
            return Fail(p);
        }
        else
        {
            kind = keyLiteral;
            catchAll = false;
        }
        mlocEmitKey(p->emit, kind, key);
        keys++;
        bool space = SkipSpace(p);
        if (StartsQuotedPattern(p))
        {
            break;
        }
        if (!space)
        {
            return Fail(p);
        }
    }
    if (keys != selectors)
    {
        ModelError(p, mloc_messageErrorVariantKeyMismatch, start);
    }
    *catchAllOut = catchAll && keys == selectors;
    return true;
}

// The selectors after ".match", each after required whitespace; ends
// after the whitespace before the first variant.
static bool ParseSelectors(Parser* p, uint32_t* countOut)
{
    uint32_t count = 0;
    for (;;)
    {
        bool space = SkipSpace(p);
        if (!space)
        {
            return Fail(p);
        }
        if (!At(p, '$'))
        {
            break;
        }
        uint32_t selector[2] = {0, (uint32_t)p->at};
        if (!ParseVariable(p, &selector[0]))
        {
            return false;
        }
        mlocEmitRecord(p->emit, messageSelectors, selector);
        count++;
    }
    if (count == 0)
    {
        return Fail(p);
    }
    *countOut = count;
    return true;
}

// matcher, after ".match": the selectors, then the variants to the end.
static bool ParseMatcher(Parser* p)
{
    size_t start = p->at - 6;
    uint32_t selectors;
    if (!ParseSelectors(p, &selectors))
    {
        return false;
    }
    bool fallback = false;
    while (p->at < p->end)
    {
        bool catchAll;
        uint32_t variant[3] = {0, 0, (uint32_t)p->at};
        if (!ParseKeys(p, selectors, p->at, &catchAll))
        {
            return false;
        }
        p->at += 2;
        if (!ParsePattern(p, true, variant))
        {
            return false;
        }
        mlocEmitRecord(p->emit, messageVariants, variant);
        fallback = fallback || catchAll;
        SkipSpace(p);
    }
    if (!fallback)
    {
        ModelError(p, mloc_messageErrorMissingFallbackVariant, start);
    }
    return true;
}

static bool Keyword(Parser* p, const char* word)
{
    size_t length = strlen(word);
    if (p->end - p->at < length || memcmp(p->text + p->at, word, length) != 0)
    {
        return false;
    }
    p->at += length;
    return true;
}

// local-declaration after ".local": s variable o "=" o expression.
static bool ParseLocal(Parser* p)
{
    uint32_t fields[3] = {declarationLocal, 0, 0};
    if (!SkipSpace(p))
    {
        return Fail(p);
    }
    if (!ParseVariable(p, &fields[1]))
    {
        return false;
    }
    SkipSpace(p);
    if (!Expect(p, '='))
    {
        return false;
    }
    SkipSpace(p);
    size_t source = p->at;
    if (!Expect(p, messageOpenBrace))
    {
        return false;
    }
    SkipSpace(p);
    uint32_t expression[8];
    if (!ParseExpression(p, source, expression, &fields[2]))
    {
        return false;
    }
    mlocEmitRecord(p->emit, messageDeclarations, fields);
    return true;
}

// input-declaration after ".input": o variable-expression.
static bool ParseInput(Parser* p)
{
    uint32_t fields[3] = {declarationInput, 0, 0};
    SkipSpace(p);
    size_t source = p->at;
    if (!Expect(p, messageOpenBrace))
    {
        return false;
    }
    SkipSpace(p);
    if (!At(p, '$'))
    {
        return Fail(p);
    }
    uint32_t expression[8];
    if (!ParseExpression(p, source, expression, &fields[2]))
    {
        return false;
    }
    fields[1] = expression[1];
    mlocEmitRecord(p->emit, messageDeclarations, fields);
    return true;
}

// complex-message: declarations, then a quoted pattern or a matcher.
static bool ParseComplex(Parser* p)
{
    for (;;)
    {
        SkipSpace(p);
        bool parsed;
        if (StartsQuotedPattern(p))
        {
            uint32_t pieces[2];
            p->at += 2;
            if (!ParsePattern(p, true, pieces))
            {
                return false;
            }
            break;
        }
        if (Keyword(p, ".input"))
        {
            parsed = ParseInput(p);
        }
        else if (Keyword(p, ".local"))
        {
            parsed = ParseLocal(p);
        }
        else if (Keyword(p, ".match"))
        {
            return ParseMatcher(p);
        }
        else
        {
            return Fail(p);
        }
        if (!parsed)
        {
            return false;
        }
    }
    SkipSpace(p);
    return p->at == p->end || Fail(p);
}

MessageParse mlocParseMessage(const uint8_t* text, size_t length, MessageEmit* emit)
{
    Parser p = {text, length, 0, emit, SIZE_MAX, mloc_messageErrorNone, 0};
    MessageParse result = {mloc_messageErrorNone, 0, 0};
    SkipSpace(&p);
    bool complex = At(&p, '.') || StartsQuotedPattern(&p);
    bool parsed;
    if (complex)
    {
        result.flags = messageComplex;
        parsed = ParseComplex(&p);
    }
    else
    {
        uint32_t pieces[2];
        p.at = 0;
        parsed = ParsePattern(&p, false, pieces);
    }
    // An error recorded anywhere stands, whatever the calls above it
    // made of it.
    if (!parsed || p.syntaxAt != SIZE_MAX)
    {
        result.error = mloc_messageErrorSyntax;
        result.errorAt = p.syntaxAt;
    }
    else if (p.model != mloc_messageErrorNone)
    {
        result.error = p.model;
        result.errorAt = p.modelAt;
    }
    return result;
}
