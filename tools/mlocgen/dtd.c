// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reading CLDR's DTD annotations (dtd.h). The DTDs put each annotation
// on its own line after the declaration it belongs to.

#include "dtd.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ElementInfo
{
    int ordered;
    // Attribute name to its AttributeKind plus one.
    Map attributes;
} ElementInfo;

static ElementInfo* Element(Dtd* dtd, const char* name)
{
    ElementInfo* info = MapGet(&dtd->elements, name);
    if (info == nullptr)
    {
        info = calloc(1, sizeof(ElementInfo));
        if (info == nullptr)
        {
            fputs("mlocgen: out of memory\n", stderr);
            exit(1);
        }
        MapPut(&dtd->elements, name, info);
    }
    return info;
}

// Copies the n-th white-space separated word of line into word.
static int Word(const char* line, int n, char* word, size_t capacity)
{
    for (int i = 0; i <= n; i++)
    {
        while (*line == ' ' || *line == '\t')
        {
            line++;
        }
        size_t length = strcspn(line, " \t\r\n>");
        if (length == 0)
        {
            return 0;
        }
        if (i == n)
        {
            if (length >= capacity)
            {
                return 0;
            }
            memcpy(word, line, length);
            word[length] = '\0';
            return 1;
        }
        line += length;
    }
    return 0;
}

int DtdLoad(Dtd* dtd, const char* path, char* errorOut, size_t errorCapacity)
{
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        snprintf(errorOut, errorCapacity, "cannot open %s", path);
        return 0;
    }
    char line[4096];
    char element[128] = "";
    char attribute[128] = "";
    int lastWasElement = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        const char* text = line + strspn(line, " \t");
        if (strncmp(text, "<!ELEMENT", 9) == 0 && Word(text, 1, element, sizeof(element)))
        {
            Element(dtd, element);
            lastWasElement = 1;
        }
        else if (strncmp(text, "<!ATTLIST", 9) == 0 && Word(text, 1, element, sizeof(element)) &&
                 Word(text, 2, attribute, sizeof(attribute)))
        {
            MapPut(&Element(dtd, element)->attributes, attribute,
                   (void*)(intptr_t)(attributeDistinguishing + 1));
            lastWasElement = 0;
        }
        else if (strncmp(text, "<!--@ORDERED-->", 15) == 0 && lastWasElement)
        {
            Element(dtd, element)->ordered = 1;
        }
        else if (strncmp(text, "<!--@VALUE-->", 13) == 0 && !lastWasElement && attribute[0])
        {
            MapPut(&Element(dtd, element)->attributes, attribute,
                   (void*)(intptr_t)(attributeValue + 1));
        }
        else if (strncmp(text, "<!--@METADATA-->", 16) == 0 && !lastWasElement && attribute[0])
        {
            MapPut(&Element(dtd, element)->attributes, attribute,
                   (void*)(intptr_t)(attributeMetadata + 1));
        }
    }
    int failed = ferror(file);
    fclose(file);
    if (failed)
    {
        snprintf(errorOut, errorCapacity, "cannot read %s", path);
        return 0;
    }
    return 1;
}

AttributeKind DtdAttributeKind(const Dtd* dtd, const char* element, const char* attribute)
{
    const ElementInfo* info = MapGet(&dtd->elements, element);
    intptr_t kind = info == nullptr ? 0 : (intptr_t)MapGet(&info->attributes, attribute);
    return kind == 0 ? attributeDistinguishing : (AttributeKind)(kind - 1);
}

int DtdIsOrdered(const Dtd* dtd, const char* element)
{
    const ElementInfo* info = MapGet(&dtd->elements, element);
    return info != nullptr && info->ordered;
}

static void FreeElement(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    ElementInfo* info = value;
    MapFree(&info->attributes);
    free(info);
}

void DtdFree(Dtd* dtd)
{
    MapEach(&dtd->elements, FreeElement, nullptr);
    MapFree(&dtd->elements);
}
