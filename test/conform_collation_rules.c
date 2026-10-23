// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation rule parser on every rule string of CLDR's
// common/collation: each parses, the counts of what it holds are printed
// for the record, and the list written back parses into the same list.
//
// usage: conform_collation_rules common/collation/*.xml

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_rules.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Counts
{
    size_t strings;
    size_t resets;
    size_t before[4];
    size_t positions;
    size_t relations;
    size_t prefixes;
    size_t expansions;
    size_t imports;
    size_t settings;
    size_t reorders;
    size_t sets;
} Counts;

static void Count(const CollationRules* r, Counts* c)
{
    c->strings++;
    for (size_t k = 0; k < r->count; k++)
    {
        const CollationRule* rule = &r->rules[k];
        c->imports += rule->kind == ruleImport;
        c->relations += rule->kind == ruleRelation;
        c->prefixes += rule->kind == ruleRelation && rule->prefixLength > 0;
        c->expansions += rule->kind == ruleRelation && rule->extensionLength > 0;
        if (rule->kind == ruleReset)
        {
            c->resets++;
            c->before[rule->strength < ruleIdentical ? rule->strength : 0]++;
            c->positions += rule->position != 0;
        }
    }
    const uint8_t values[] = {r->strength,  r->alternate,     r->maxVariable, r->caseFirst,
                              r->caseLevel, r->normalization, r->numeric,     r->backwards};
    for (size_t k = 0; k < sizeof(values); k++)
    {
        c->settings += values[k] != ruleUnset;
    }
    c->reorders += r->reorderGiven;
    c->sets += (r->suppressCount > 0) + (r->optimizeCount > 0);
}

static bool CheckFile(const char* path, Counts* counts)
{
    char error[256] = "";
    XmlNode* root = XmlParseFile(path, error, sizeof(error));
    XmlNode* collations = root == nullptr ? nullptr : XmlFindChild(root, "collations");
    if (root == nullptr)
    {
        printf("%s: %s\n", path, error);
        return false;
    }
    bool ok = true;
    for (int i = 0; collations != nullptr && i < collations->childCount; i++)
    {
        const XmlNode* collation = collations->children[i];
        const XmlNode* cr =
            strcmp(collation->name, "collation") == 0 ? XmlFindChild(collation, "cr") : nullptr;
        if (cr == nullptr)
        {
            continue;
        }
        const char* type = XmlGetAttribute(collation, "type");
        const char* alt = XmlGetAttribute(collation, "alt");
        CollationRules rules;
        if (!CollationRulesParse(cr->text, strlen(cr->text), &rules, error, sizeof(error)))
        {
            printf("%s %s%s%s: %s\n", path, type == nullptr ? "?" : type, alt == nullptr ? "" : " ",
                   alt == nullptr ? "" : alt, error);
            ok = false;
            continue;
        }
        Count(&rules, counts);
        size_t length = 0;
        char* written = CollationRulesWrite(&rules, &length);
        CollationRules again;
        bool same = CollationRulesParse(written, length, &again, error, sizeof(error)) &&
                    CollationRulesSame(&rules, &again);
        if (!same)
        {
            printf("%s %s: written back, %s\n", path, type == nullptr ? "?" : type,
                   error[0] != '\0' ? error : "parses into another list");
            ok = false;
        }
        else
        {
            CollationRulesFree(&again);
        }
        free(written);
        CollationRulesFree(&rules);
    }
    XmlFree(root);
    return ok;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fputs("usage: conform_collation_rules common/collation/*.xml\n", stderr);
        return 2;
    }
    Counts c = {0};
    bool ok = true;
    for (int i = 1; i < argc; i++)
    {
        ok = CheckFile(argv[i], &c) && ok;
    }
    printf("%d files, %zu rule strings: %zu resets (%zu before 1, %zu before 2, %zu before 3, "
           "%zu at a special position), %zu relations (%zu with a prefix, %zu with an "
           "expansion), %zu imports, %zu settings, %zu reorders, %zu sets; each written back "
           "parses the same\n",
           argc - 1, c.strings, c.resets, c.before[1], c.before[2], c.before[3], c.positions,
           c.relations, c.prefixes, c.expansions, c.imports, c.settings, c.reorders, c.sets);
    return ok ? 0 : 1;
}
