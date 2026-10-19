// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// File reading and writing (file.h).

#include "file.h"

#include <stdio.h>
#include <stdlib.h>

char* FileRead(const char* path, size_t* lengthOut)
{
    FILE* f = fopen(path, "rb");
    if (f == nullptr)
    {
        return nullptr;
    }
    size_t capacity = 1 << 16;
    size_t length = 0;
    char* text = malloc(capacity + 1);
    while (text != nullptr)
    {
        length += fread(text + length, 1, capacity - length, f);
        if (length < capacity)
        {
            break;
        }
        capacity *= 2;
        char* grown = realloc(text, capacity + 1);
        if (grown == nullptr)
        {
            free(text);
        }
        text = grown;
    }
    int failed = ferror(f);
    fclose(f);
    if (text == nullptr || failed)
    {
        free(text);
        return nullptr;
    }
    text[length] = '\0';
    *lengthOut = length;
    return text;
}

int FileWrite(const char* path, const void* bytes, size_t length)
{
    FILE* f = fopen(path, "wb");
    if (f == nullptr)
    {
        return 0;
    }
    size_t written = fwrite(bytes, 1, length, f);
    return fclose(f) == 0 && written == length;
}
