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
#include <ctype.h>
#include <string.h>
#include <unistd.h>

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

static int parse_site_record(xmlNode *node, scrap500_site_t *site)
{
    xmlNode *tmp = NULL;
    xmlNode *table = NULL;
    char *str = NULL;

    str = get_child_text(node, "h1", 2);
    if (str)
        site->name = strdup(str);

    table = get_child_element(node, "table", 1);

    tmp = get_child_element(table, "tr", 1);
    tmp = get_child_element(tmp, "td", 1);
    str = get_child_text(tmp, "a", 1);
    if (str)
        site->url = strdup(str);

    tmp = get_child_element(table, "tr", 2);
    str = get_child_text(tmp, "td", 1);
    if (str)
        site->segment = strdup(str);

    tmp = get_child_element(table, "tr", 3);
    str = get_child_text(tmp, "td", 1);
    if (str)
        site->city = strdup(str);

    tmp = get_child_element(table, "tr", 4);
    str = get_child_text(tmp, "td", 1);
    if (str)
        site->country = strdup(str);

    return 0;
}

static inline char *get_system_name(const char *str)
{
    char *spos = NULL;
    char *epos = NULL;
    char buf[512] = { 0, };

    sprintf(buf, "%s", str);

    for (spos = buf; isspace(spos[0]) || spos[0] == '"'; spos++)
        ;

    for (epos = spos; isspace(epos[0]) && epos[1] == '-'; epos++)
        ;

    epos[0] = '\0';

out:
    return strdup(spos);
}

static int parse_system_record(xmlNode *node, scrap500_system_t *system)
{
    xmlNode *tmp = NULL;
    xmlNode *table = NULL;
    char *str = NULL;

    str = get_child_text(node, "h1", 1);
    if (str)
        system->name = get_system_name(str);

#if 0
    table = get_child_element(node, "table", 1);

    tmp = get_child_element(table, "tr", 1);
    tmp = get_child_element(tmp, "td", 1);
    str = get_child_text(tmp, "a", 1);
    if (str)
        system->url = strdup(str);

    tmp = get_child_element(table, "tr", 2);
    str = get_child_text(tmp, "td", 1);
    if (str)
        system->segment = strdup(str);

    tmp = get_child_element(table, "tr", 3);
    str = get_child_text(tmp, "td", 1);
    if (str)
        system->city = strdup(str);

    tmp = get_child_element(table, "tr", 4);
    str = get_child_text(tmp, "td", 1);
    if (str)
        system->country = strdup(str);
#endif

    return 0;
}


static int parse_list_td_rank(xmlNode *td)
{
    char *str = NULL;
    xmlNode *span = get_child_element(td, "span", 1);

    str = (char *) span->children->content;

    return atoi(str);
}

static uint64_t parse_list_td_site(xmlNode *td)
{
    char *pos = NULL;
    char *str = NULL;
    xmlNode *a = get_child_element(td, "a", 1);

    str = (char *) a->properties->children->content;
    pos = strrchr(str, '/');

    return atol(&pos[1]);
}

static uint64_t parse_list_td_system(xmlNode *td)
{
    char *pos = NULL;
    char *str = NULL;
    xmlNode *a = get_child_element(td, "a", 1);

    str = (char *) a->properties->children->content;
    pos = strrchr(str, '/');

    return atol(&pos[1]);
}

static int parse_list_table(scrap500_list_t *list, xmlNode *table)
{
    int ret = 0;
    int pos = 0;
    int count = 0;
    char *str = NULL;
    xmlNode *tr = NULL;
    xmlNode *td = NULL;
    scrap500_rank_t *rank = NULL;

    tr = get_child_element(table, "tr", 1);

    do {
        td = get_child_element(tr, "td", 1);    /* col1: rank pos */
        pos = parse_list_td_rank(td);
        rank = &list->rank[pos-1];
        rank->rank = pos;

        td = get_child_element(tr, "td", 2);    /* col2: site */
        rank->site_id = parse_list_td_site(td);

        td = get_child_element(tr, "td", 3);    /* col3: system */
        rank->system_id = parse_list_td_system(td);

        tr = tr->next;
        count++;
    } while (count < 100);

    return ret;
}

int scrap500_parser_parse_list(scrap500_list_t *list)
{
    int ret = 0;
    int page = 0;
    int opts = 0;
    char *str = NULL;
    char filename[PATH_MAX] = { 0, };
    htmlDocPtr doc = NULL;
    xmlNode *root = NULL;
    xmlNode *current = NULL;

    if (!list)
        return EINVAL;

    opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR
           | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

    for (page = 1; page <= 5; page++) {
        scrap500_list_html_filename(list, page, filename);

        doc = htmlReadFile(filename, NULL, opts);
        if (!doc) {
            fprintf(stderr, "cannot parse the document (list=%d)\n", list->id);
            ret = EIO;
            goto out;
        }

        root = xmlDocGetRootElement(doc);
        if (!root) {
            fprintf(stderr, "cannot parse the document (list=%d)\n", list->id);
            ret = EIO;
            goto out;
        }

        current = get_child_element(root, "body", 1);
        current = get_child_element(current, "div", 2);
        current = get_child_element(current, "div", 1);
        current = get_child_element(current, "div", 1);
        current = get_child_element(current, "table", 1);

        ret = parse_list_table(list, current);
    }

out:
    if (doc)
        xmlFreeDoc(doc);

    return ret;
}

int scrap500_parser_parse_site(uint64_t site_id, scrap500_site_t *site)
{
    int ret = 0;
    int opts = 0;
    char filename[PATH_MAX] = { 0, };
    htmlDocPtr doc = NULL;
    xmlNode *root = NULL;
    xmlNode *current = NULL;

    scrap500_site_html_filename(site_id, filename);

    opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR
           | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

    doc = htmlReadFile(filename, NULL, opts);
    if (!doc) {
        fprintf(stderr, "cannot parse the document %s\n", filename);
        ret = EIO;
        goto out;
    }

    root = xmlDocGetRootElement(doc);
    if (!root) {
        fprintf(stderr, "cannot parse the document %s\n", filename);
        ret = EIO;
        goto out;
    }

    current = get_child_element(root, "body", 1);
    current = get_child_element(current, "div", 2);
    current = get_child_element(current, "div", 1);
    current = get_child_element(current, "div", 1);

    ret = parse_site_record(current, site);
    if (ret)
        fprintf(stderr, "failed to parse the site record.\n");

out:
    if (doc)
        xmlFreeDoc(doc);

    return ret;
}

int scrap500_parser_parse_system(uint64_t system_id, scrap500_system_t *system)
{
    int ret = 0;
    int opts = 0;
    char filename[PATH_MAX] = { 0, };
    htmlDocPtr doc = NULL;
    xmlNode *root = NULL;
    xmlNode *current = NULL;

    scrap500_system_html_filename(system_id, filename);

    opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR
           | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

    doc = htmlReadFile(filename, NULL, opts);
    if (!doc) {
        fprintf(stderr, "cannot parse the document %s\n", filename);
        ret = EIO;
        goto out;
    }

    root = xmlDocGetRootElement(doc);
    if (!root) {
        fprintf(stderr, "cannot parse the document %s\n", filename);
        ret = EIO;
        goto out;
    }

    current = get_child_element(root, "body", 1);
    current = get_child_element(current, "div", 2);
    current = get_child_element(current, "div", 1);
    current = get_child_element(current, "div", 1);

    ret = parse_system_record(current, system);
    if (ret)
        fprintf(stderr, "failed to parse the system record.\n");

out:
    if (doc)
        xmlFreeDoc(doc);
    return ret;
}


int scrap500_parser_parse_specs(scrap500_list_t *list)
{
    int ret = 0;

    return ret;
}

