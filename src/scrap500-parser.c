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

static char *strtrim_dup(char *str)
{
    char *pos = NULL;
    char *epos = NULL;
    char buf[1024] = { 0, };

    sprintf(buf, "%s", str);

    pos = buf;
    while (isspace(pos[0]))
        pos++;

    epos = &buf[strlen(buf) - 1];
    while (isspace(epos[0]))
        epos--;

    epos[1] = '\0';

    return strdup(pos);
}

static char *strtolower(char *str)
{
    char *pos = str;

    if (!pos)
        return NULL;

    while (pos[0] != '\0') {
        pos[0] = tolower(pos[0]);
        pos++;
    }

    return str;
}

static int parse_number(const char *str, double *val)
{
    int i = 0;
    char buf[512] = { 0, };
    const char *pos = NULL;

    for (pos = str; *pos != '\0'; pos++) {
        if (pos[0] == ',')
            continue;

        buf[i++] = pos[0];
    }

    i = sscanf(buf, "%lf", val);

    return i == 1 ? 0 : errno;
}

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

static inline int parse_system_name(scrap500_system_t *system, const char *str)
{
    char *pos = NULL;
    char buf[512] = { 0, };

    sprintf(buf, "%s", str);

    pos = strstr(buf, " - ");

    if (!pos) { /* only name without summary */
        system->name = strtrim_dup(buf);
        goto out;
    }

    pos[0] = '\0';

    system->name = strtrim_dup(buf);
    system->summary = strtrim_dup(&pos[2]);

out:
    return 0;
}

enum {
    SYSATTR_SITE = 1,
    SYSATTR_URL,
    SYSATTR_MANUFACTURER,
    SYSATTR_CORES,
    SYSATTR_MEMORY,
    SYSATTR_PROCESSOR,
    SYSATTR_INTERCONNECT,
    SYSATTR_LINPACK,
    SYSATTR_TPEAK,
    SYSATTR_NMAX,
    SYSATTR_NHALF,
    SYSATTR_HPCG,
    SYSATTR_POWER,
    SYSATTR_PML,
    SYSATTR_MCORES,
    SYSATTR_OS,
    SYSATTR_COMPILER,
    SYSATTR_MATHLIB,
    SYSATTR_MPI,
    N_SYSATTR,
};

static const char *attr_names[] = {
    NULL,
    "site",
    "system url",
    "manufacturer",
    "cores",
    "memory",
    "processor",
    "interconnect",
    "linpack performance (rmax)",
    "theoretical peak (rpeak)",
    "nmax",
    "nhalf",
    "hpcg [tflop/s]",
    "power",
    "power measurement level",
    "measured cores",
    "operating system",
    "compiler",
    "math library",
    "mpi",
};

static int parse_sysattr_site(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *pos = NULL;
    char *str = (char *) td->children->properties->children->content;

    if (!str) {
        ret = EINVAL;
        goto out;
    }

    pos = strrchr(str, '/');
    pos++;

    system->site_id = strtoull(pos, NULL, 0);

out:
    return ret;
}

static int parse_sysattr_url(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->properties->children->content;

    if (str)
        system->url = strdup(str);

    return ret;
}

static int parse_sysattr_manufacturer(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;

    if (str)
        system->manufacturer = strdup(str);

    return ret;
}

static int parse_sysattr_cores(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->cores = val;
    }

    return ret;
}

static int parse_sysattr_memory(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->memory = val;
    }

    return ret;
}

static int parse_sysattr_processor(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;

    if (str)
        system->processor = strdup(str);

    return ret;
}

static int parse_sysattr_interconnect(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;

    if (str)
        system->interconnect = strdup(str);

    return ret;
}

static int parse_sysattr_linpack(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->linpack_perf = val;
    }

    return ret;
}

static int parse_sysattr_tpeak(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->theoretical_peak = val;
    }

    return ret;
}

static int parse_sysattr_nmax(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->nmax = val;
    }

    return ret;
}

static int parse_sysattr_nhalf(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->nhalf = val;
    }

    return ret;
}

static int parse_sysattr_hpcg(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->hpcg = val;
    }

    return ret;
}

static int parse_sysattr_power(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->power = val;
    }

    return ret;
}

static int parse_sysattr_pml(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->power_measurement_level = val;
    }

    return ret;
}

static int parse_sysattr_mcores(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;
    char *unit = NULL;
    double val = .0f;

    if (str) {
        ret = parse_number(str, &val);
        if (ret)
            fprintf(stderr, "cannot parse %s: %s\n", str, strerror(ret));
        else
            system->measured_cores = val;
    }

    return ret;
}

static int parse_sysattr_os(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;

    if (str)
        system->os = strdup(str);

    return ret;
}

static int parse_sysattr_compiler(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;

    if (str)
        system->compiler = strdup(str);

    return ret;
}

static int parse_sysattr_mathlib(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;

    if (str)
        system->mathlib = strdup(str);

    return ret;
}

static int parse_sysattr_mpi(scrap500_system_t *system, xmlNode *td)
{
    int ret = 0;
    char *str = (char *) td->children->content;

    if (str)
        system->mpi = strdup(str);

    return ret;
}

static int (*parse_sysattr_funcs[])(scrap500_system_t *, xmlNode *) = {
    NULL,
    parse_sysattr_site,
    parse_sysattr_url,
    parse_sysattr_manufacturer,
    parse_sysattr_cores,
    parse_sysattr_memory,
    parse_sysattr_processor,
    parse_sysattr_interconnect,
    parse_sysattr_linpack,
    parse_sysattr_tpeak,
    parse_sysattr_nmax,
    parse_sysattr_nhalf,
    parse_sysattr_hpcg,
    parse_sysattr_power,
    parse_sysattr_pml,
    parse_sysattr_mcores,
    parse_sysattr_os,
    parse_sysattr_compiler,
    parse_sysattr_mathlib,
    parse_sysattr_mpi,
};

static inline int system_attr(const char *str)
{
    int i = 0;

    for (i = 1; i < N_SYSATTR; i++)
        if (0 == strcmp(str, attr_names[i]))
            return i;

    return 0;
}

static int parse_system_attribute(scrap500_system_t *system,
                                  const char *attr_str, xmlNode *td)
{
    int ret = 0;
    int attr = 0;
    char *pos = NULL;
    char buf[1024] = { 0, };

    sprintf(buf, "%s", attr_str);
    pos = strrchr(buf, ':');
    if (pos)
        pos[0] = '\0';

    attr = system_attr(strtolower(buf));
    if (0 == attr || attr >= N_SYSATTR) {
        fprintf(stderr, "unknown attribute: %s\n", buf);
        ret = EINVAL;
        goto out;
    }

    ret = parse_sysattr_funcs[attr](system, td);

out:
    return ret;
}

static int parse_system_table(scrap500_system_t *system, xmlNode *table)
{
    int ret = 0;
    int count = 0;
    char *attr_str = NULL;
    xmlNode *tr = NULL;
    xmlNode *th = NULL;
    xmlNode *td = NULL;

    tr = get_child_element(table, "tr", 1);

    for ( ; tr; tr = tr->next, count++) {
        td = get_child_element(tr, "td", 1);
        if (!td)
            continue;

        if (!td->children)
            continue;   /* column shown but no data available */

        th = get_child_element(tr, "th", 1);

#if 0
        printf("[%d] %s: %s\n",
               ret, get_child_text(tr, "th", 1), get_child_text(tr, "td", 1));
#endif

        attr_str = get_child_text(tr, "th", 1);

        ret = parse_system_attribute(system, attr_str, td);
        if (ret) {
            fprintf(stderr, "failed to parse attribute %s\n", attr_str);
            goto out;
        }
    }

    printf("%d attributes found\n", count);

out:
    return ret;
}

static int parse_system_record(xmlNode *node, scrap500_system_t *system)
{
    int ret = 0;
    char *str = NULL;
    xmlNode *table = NULL;

    str = get_child_text(node, "h1", 1);
    if (str) {
        ret = parse_system_name(system, str);
        if (ret) {
            fprintf(stderr, "failed to parse system name: %s\n", str);
            goto out;
        }
    }

    table = get_child_element(node, "table", 1);

    ret = parse_system_table(system, table);
    if (ret)
        fprintf(stderr, "failed to parse system table\n");

out:
    return ret;
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

    site->id = site_id;
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

    system->id = system_id;
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

