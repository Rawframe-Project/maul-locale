// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_write.h"

#include "message_text.h"

#include <stdbool.h>
#include <string.h>

// The bytes that fit go to the buffer; length counts them all.
typedef struct Writer
{
    char* buffer;
    size_t capacity;
    size_t length;
    const MessageView* view;
} Writer;

static void Put(Writer* w, const uint8_t* bytes, size_t length)
{
    if (w->length < w->capacity)
    {
        size_t room = w->capacity - w->length;
        memcpy(w->buffer + w->length, bytes, length < room ? length : room);
    }
    // Saturating: a variable's name written at each of its references
    // can exceed a 32-bit size_t, which then reports the capacity short.
    w->length = length > SIZE_MAX - w->length ? SIZE_MAX : w->length + length;
}

static void PutText(Writer* w, const char* text)
{
    Put(w, (const uint8_t*)text, strlen(text));
}

static uint32_t Field(const Writer* w, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(w->view, kind, index, field);
}

static void PutPool(Writer* w, uint32_t offset, uint32_t length)
{
    Put(w, w->view->pool + offset, length);
}

// Pool bytes with a backslash before each byte that must be escaped:
// "\" and "|" in a quoted literal, "\", "{" and "}" in text.
static void PutEscaped(Writer* w, uint32_t offset, uint32_t length, bool quoted)
{
    const uint8_t* text = w->view->pool + offset;
    uint32_t from = 0;
    for (uint32_t i = 0; i < length; i++)
    {
        uint8_t b = text[i];
        bool escape =
            b == '\\' || (quoted ? b == '|' : b == messageOpenBrace || b == messageCloseBrace);
        if (escape)
        {
            Put(w, text + from, i - from);
            PutText(w, "\\");
            from = i;
        }
    }
    Put(w, text + from, length - from);
}

// A literal unquoted when it is one or more name characters, else
// quoted.
static void PutLiteral(Writer* w, uint32_t offset, uint32_t length)
{
    const uint8_t* text = w->view->pool + offset;
    bool bare = length > 0;
    for (size_t at = 0; bare && at < length;)
    {
        size_t size;
        bare = mlocMessageIsNameChar(mlocMessageDecode(text, at, length, &size));
        at += size;
    }
    if (bare)
    {
        PutPool(w, offset, length);
        return;
    }
    PutText(w, "|");
    PutEscaped(w, offset, length, true);
    PutText(w, "|");
}

static void PutVariable(Writer* w, uint32_t variable)
{
    PutText(w, "$");
    PutPool(w, Field(w, messageVariables, variable, 0), Field(w, messageVariables, variable, 1));
}

// Each parameter after a space: the options first, then the attributes,
// as the grammar orders them.
static void PutParameters(Writer* w, uint32_t first, uint32_t count)
{
    for (uint32_t pass = 0; pass < 2; pass++)
    {
        for (uint32_t i = first; i < first + count; i++)
        {
            uint32_t kind = Field(w, messageParameters, i, 0);
            bool option = kind == parameterOptionLiteral || kind == parameterOptionVariable;
            if (option != (pass == 0))
            {
                continue;
            }
            PutText(w, option ? " " : " @");
            PutPool(w, Field(w, messageParameters, i, 1), Field(w, messageParameters, i, 2));
            if (kind == parameterAttribute)
            {
                continue;
            }
            PutText(w, "=");
            if (kind == parameterOptionVariable)
            {
                PutVariable(w, Field(w, messageParameters, i, 3));
            }
            else
            {
                PutLiteral(w, Field(w, messageParameters, i, 3), Field(w, messageParameters, i, 4));
            }
        }
    }
}

static void PutExpression(Writer* w, uint32_t e)
{
    PutText(w, "{");
    uint32_t operand = Field(w, messageExpressions, e, 0);
    if (operand == operandVariable)
    {
        PutVariable(w, Field(w, messageExpressions, e, 1));
    }
    else if (operand == operandLiteral)
    {
        PutLiteral(w, Field(w, messageExpressions, e, 1), Field(w, messageExpressions, e, 2));
    }
    uint32_t function = Field(w, messageExpressions, e, 4);
    if (function > 0)
    {
        PutText(w, operand == operandNone ? ":" : " :");
        PutPool(w, Field(w, messageExpressions, e, 3), function);
    }
    PutParameters(w, Field(w, messageExpressions, e, 5), Field(w, messageExpressions, e, 6));
    PutText(w, "}");
}

static void PutMarkup(Writer* w, uint32_t m)
{
    uint32_t kind = Field(w, messageMarkups, m, 0);
    PutText(w, kind == markupClose ? "{/" : "{#");
    PutPool(w, Field(w, messageMarkups, m, 1), Field(w, messageMarkups, m, 2));
    PutParameters(w, Field(w, messageMarkups, m, 3), Field(w, messageMarkups, m, 4));
    PutText(w, kind == markupStandalone ? " /}" : "}");
}

static void PutPattern(Writer* w, uint32_t first, uint32_t count)
{
    for (uint32_t i = first; i < first + count; i++)
    {
        uint32_t kind = Field(w, messagePieces, i, 0);
        uint32_t a = Field(w, messagePieces, i, 1);
        if (kind == pieceText)
        {
            PutEscaped(w, a, Field(w, messagePieces, i, 2), false);
        }
        else if (kind == pieceExpression)
        {
            PutExpression(w, a);
        }
        else
        {
            PutMarkup(w, a);
        }
    }
}

// Whether a pattern can stand as a simple message: its first character
// past spaces and bidi marks is not ".", which would start a complex one.
static bool StandsAlone(const Writer* w)
{
    const MessageView* v = w->view;
    for (uint32_t i = 0; i < v->count[messagePieces]; i++)
    {
        if (Field(w, messagePieces, i, 0) != pieceText)
        {
            return true;
        }
        const uint8_t* text = v->pool + Field(w, messagePieces, i, 1);
        size_t length = Field(w, messagePieces, i, 2);
        for (size_t at = 0; at < length;)
        {
            size_t size;
            uint32_t c = mlocMessageDecode(text, at, length, &size);
            if (!mlocMessageIsSpace(c) && !mlocMessageIsBidi(c))
            {
                return c != '.';
            }
            at += size;
        }
    }
    return true;
}

static void PutDeclarations(Writer* w)
{
    for (uint32_t d = 0; d < w->view->count[messageDeclarations]; d++)
    {
        if (Field(w, messageDeclarations, d, 0) == declarationLocal)
        {
            PutText(w, ".local ");
            PutVariable(w, Field(w, messageDeclarations, d, 1));
            PutText(w, " = ");
        }
        else
        {
            PutText(w, ".input ");
        }
        PutExpression(w, Field(w, messageDeclarations, d, 2));
        PutText(w, "\n");
    }
}

static void PutMatcher(Writer* w)
{
    const MessageView* v = w->view;
    PutText(w, ".match");
    for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
    {
        PutText(w, " ");
        PutVariable(w, Field(w, messageSelectors, s, 0));
    }
    for (uint32_t i = 0; i < v->count[messageVariants]; i++)
    {
        PutText(w, "\n");
        for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
        {
            uint32_t offset;
            uint32_t length;
            if (mlocMessageKey(v, i, s, &offset, &length) == keyCatchAll)
            {
                PutText(w, "*");
            }
            else
            {
                PutLiteral(w, offset, length);
            }
            PutText(w, " ");
        }
        PutText(w, "{{");
        PutPattern(w, Field(w, messageVariants, i, 0), Field(w, messageVariants, i, 1));
        PutText(w, "}}");
    }
}

mlocResult mlocWriteMessage(const MessageView* view, mlocOutput* output)
{
    Writer w = {output->buffer, output->capacity, 0, view};
    if (view->count[messageVariants] > 0)
    {
        PutDeclarations(&w);
        PutMatcher(&w);
    }
    else if (view->count[messageDeclarations] == 0 && StandsAlone(&w))
    {
        PutPattern(&w, 0, view->count[messagePieces]);
    }
    else
    {
        PutDeclarations(&w);
        PutText(&w, "{{");
        PutPattern(&w, 0, view->count[messagePieces]);
        PutText(&w, "}}");
    }
    output->length = w.length;
    return w.length > w.capacity ? mloc_errorCapacity : mloc_success;
}
