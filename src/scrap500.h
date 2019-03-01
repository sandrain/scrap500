/* Copyright (C) 2019 - UT-Battelle, LLC. All right reserved.
 * 
 * Please refer to COPYING for the license.
 * Written by: Hyogi Sim <sandrain@gmail.com>
 * ---------------------------------------------------------------------------
 * 
 */
#ifndef __SCRAP500_H__
#define __SCRAP500_H__

#include <config.h>

#include <stdint.h>
#include <errno.h>
#include <libxml/HTMLparser.h>

struct _scrap500_site {
    uint64_t id;
    char *name;
    char *url;
    char *segment;
    char *city;
    char *country;
};

typedef struct _scrap500_site scrap500_site_t;

struct _scrap500_system {
    uint64_t id;
    char *name;
    scrap500_site_t *site;
};

typedef struct _scrap500_system scrap500_system_t;

struct _scrap500_list {
    uint32_t ym;    /* YYYYMM */
    scrap500_system_t systems[500];
};

typedef struct _scrap500_list scrap500_list_t;

int scrap500_parser_parse_site_record(xmlNode *node, scrap500_site_t *site);

int scrap500_get_http(const char *url, FILE *tmpfp);

#endif /* __SCRAP500_H__ */

