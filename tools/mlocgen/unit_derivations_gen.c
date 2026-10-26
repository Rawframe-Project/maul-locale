// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The grammatical derivations (unit_derivations_gen.h).

#include "unit_derivations_gen.h"

#include "kinds.h"
#include "table_gen.h"
#include "unit_tables.h"
#include "xml.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    categoryCount = 6,
};

static const char* const s_categories[categoryCount] = {"zero", "one",  "two",
                                                        "few",  "many", "other"};

static const char* const s_cases[unitCaseCount] = {"nominative",
                                                   "abessive",
                                                   "ablative",
                                                   "accusative",
                                                   "adessive",
                                                   "allative",
                                                   "causal",
                                                   "comitative",
                                                   "dative",
                                                   "delative",
                                                   "elative",
                                                   "ergative",
                                                   "essive",
                                                   "genitive",
                                                   "illative",
                                                   "inessive",
                                                   "instrumental",
                                                   "locative",
                                                   "locativecopulative",
                                                   "oblique",
                                                   "partitive",
                                                   "prepositional",
                                                   "sociative",
                                                   "sublative",
                                                   "superessive",
                                                   "terminative",
                                                   "translative",
                                                   "vocative"};

static const char* const s_genders[unitGenderCount] = {
    "", "animate", "common", "feminine", "inanimate", "masculine", "neuter", "personal"};

static const char* const s_structures[unitDeriveStructures] = {"per", "times", "power", "prefix"};

static int IndexIn(const char* const* names, int count, const char* name)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(name, names[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

int UnitCaseIndex(const char* name)
{
    return IndexIn(s_cases, unitCaseCount, name);
}

const char* UnitCaseName(int index)
{
    return s_cases[index];
}

const char* UnitGenderName(int index)
{
    return s_genders[index];
}

int UnitGenderIndex(const char* name)
{
    return name[0] == '\0' ? -1 : IndexIn(s_genders, unitGenderCount, name);
}

// A component's value: a category's index (plural) or a case's (case),
// compound or none.
static uint8_t DerivedValue(const char* feature, const char* value, const char* where)
{
    if (value == nullptr)
    {
        GenDie("a derivation without its value", where);
    }
    if (strcmp(value, "compound") == 0)
    {
        return unitDerivedCompound;
    }
    bool plural = strcmp(feature, "plural") == 0;
    int index = plural ? IndexIn(s_categories, categoryCount, value) : UnitCaseIndex(value);
    if (index < 0)
    {
        GenDie(plural ? "a plural derivation of no category" : "a case derivation of no case",
               value);
    }
    return (uint8_t)index;
}

// A compound's gender: a component's, or a gender.
static uint8_t DerivedGender(const char* value, const char* where)
{
    if (value == nullptr)
    {
        GenDie("a gender derivation without its value", where);
    }
    if (strcmp(value, "0") == 0 || strcmp(value, "1") == 0)
    {
        return value[0] == '0' ? unitGenderFirst : unitGenderSecond;
    }
    int index = UnitGenderIndex(value);
    if (index < 0)
    {
        GenDie("a gender derivation of no gender", value);
    }
    return (uint8_t)index;
}

// A structure's index.
static int StructureOf(const char* structure)
{
    int s = structure == nullptr ? -1 : IndexIn(s_structures, unitDeriveStructures, structure);
    if (s < 0)
    {
        GenDie("a derivation of no structure", structure == nullptr ? "" : structure);
    }
    return s;
}

// Fills a language's record from its grammaticalDerivations element.
static void ReadDerivation(const XmlNode* node, uint8_t* record)
{
    memset(record + unitDerivationPlural, unitDerivedNone,
           unitDerivationSize - unitDerivationPlural);
    for (int i = 0; i < node->childCount; i++)
    {
        const XmlNode* rule = node->children[i];
        const char* feature = XmlGetAttribute(rule, "feature");
        const char* structure = XmlGetAttribute(rule, "structure");
        bool component = strcmp(rule->name, "deriveComponent") == 0;
        if (feature == nullptr || (!component && strcmp(rule->name, "deriveCompound") != 0))
        {
            continue;
        }
        int s = StructureOf(structure);
        if (component && (strcmp(feature, "plural") == 0 || strcmp(feature, "case") == 0))
        {
            uint8_t* at = record + (feature[0] == 'p' ? unitDerivationPlural : unitDerivationCase);
            at[2 * s] = DerivedValue(feature, XmlGetAttribute(rule, "value0"), structure);
            at[2 * s + 1] = DerivedValue(feature, XmlGetAttribute(rule, "value1"), structure);
        }
        else if (!component && strcmp(feature, "gender") == 0)
        {
            record[unitDerivationGender + s] =
                DerivedGender(XmlGetAttribute(rule, "value"), structure);
        }
        else
        {
            GenDie("a derivation of a feature the generator does not know", feature);
        }
    }
}

static int CompareRecords(const void* a, const void* b)
{
    return memcmp(a, b, unitDerivationLanguage);
}

int GenerateUnitDerivations(const char* commonDirectory, DataFile* file, char* errorOut,
                            size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/grammaticalFeatures.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    enum
    {
        maxLanguages = 64,
    };
    uint8_t unit[unitDerivationsHeaderSize + maxLanguages * unitDerivationSize] = {0};
    uint32_t count = 0;
    const XmlNode* data = XmlFindChild(root, "grammaticalData");
    for (int i = 0; data != nullptr && i < data->childCount; i++)
    {
        const XmlNode* node = data->children[i];
        const char* locales = XmlGetAttribute(node, "locales");
        for (const char* at = locales;
             strcmp(node->name, "grammaticalDerivations") == 0 && at != nullptr && *at != '\0';)
        {
            size_t length = strcspn(at, " ");
            if (length == 0 || length > unitDerivationLanguage || count == maxLanguages)
            {
                GenDie("a derivation's language too long, or too many", at);
            }
            uint8_t* record = unit + unitDerivationsHeaderSize + count++ * unitDerivationSize;
            memcpy(record, at, length);
            ReadDerivation(node, record);
            at += length + strspn(at + length, " ");
        }
    }
    XmlFree(root);
    GenSort(unit + unitDerivationsHeaderSize, count, unitDerivationSize, CompareRecords);
    GenPut(unit, count, 2);
    DataFileAdd(file, kindUnitDerivations, nullptr, unit,
                unitDerivationsHeaderSize + count * unitDerivationSize);
    return 1;
}
