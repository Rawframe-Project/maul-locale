// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// JSON strings (json.h).

#include "json.h"

void JsonString(FILE* out, const char* bytes, size_t length)
{
    fputc('"', out);
    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)bytes[i];
        if (c == '"' || c == '\\')
        {
            fprintf(out, "\\%c", c);
        }
        else if (c == '\n')
        {
            fputs("\\n", out);
        }
        else if (c < 0x20 || c == 0x7F)
        {
            fprintf(out, "\\u%04x", c);
        }
        else
        {
            fputc(c, out);
        }
    }
    fputc('"', out);
}
