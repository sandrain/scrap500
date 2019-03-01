/* Copyright (C) 2019 - UT-Battelle, LLC. All right reserved.
 * 
 * Please refer to COPYING for the license.
 * Written by: Hyogi Sim <sandrain@gmail.com>
 * ---------------------------------------------------------------------------
 * 
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrap500.h"

static xmlNode *get_child_element(xmlNode *node, const char *name, int nth)
{
    int count = 0;
    xmlNode *current = NULL;

    for (current = node->children; current; current = current->next) {
        if (strcmp((char *) current->name, name))
            continue;

        count++;
        if (count == nth)
            return current;
    }

    return NULL;
}

static char *get_child_text(xmlNode *node, const char *name, int nth)
{
    int count = 0;
    xmlNode *current = NULL;

    for (current = node->children; current; current = current->next) {
        if (strcmp((char *) current->name, name))
            continue;

        count++;

        if (count == nth) {
            current = get_child_element(current, "text", 1);
            return current ? (char *) current->content : NULL;
        }
    }

    return NULL;
}

int scrap500_parser_parse_site_record(xmlNode *node, scrap500_site_t *site)
{
    xmlNode *tmp = NULL;
    xmlNode *table = NULL;
    char *str = NULL;

    str = get_child_text(node, "h1", 2);
    if (str)
        site->name = str;

    table = get_child_element(node, "table", 1);

    tmp = get_child_element(table, "tr", 1);
    tmp = get_child_element(tmp, "td", 1);
    str = get_child_text(tmp, "a", 1);
    if (str)
        site->url = str;

    tmp = get_child_element(table, "tr", 2);
    str = get_child_text(tmp, "td", 1);
    if (str)
        site->segment = str;

    tmp = get_child_element(table, "tr", 3);
    str = get_child_text(tmp, "td", 1);
    if (str)
        site->city = str;

    tmp = get_child_element(table, "tr", 4);
    str = get_child_text(tmp, "td", 1);
    if (str)
        site->country = str;

    return 0;
}

