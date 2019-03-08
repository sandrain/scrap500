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
#include <pthread.h>
#include <libxml/HTMLparser.h>
#include <curl/curl.h>

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

struct _scrap500_rank {
    int rank;
    char *system_name;
    uint64_t system_id;
    uint64_t site_id;
};

typedef struct _scrap500_rank scrap500_rank_t;

struct _scrap500_list {
    uint32_t id;                /* YYYYMM */
    char url[5][PATH_MAX];      /* five webpages ..?page=[1-5] */
    FILE *tmpfp[5];
    scrap500_rank_t rank[500];
};

typedef struct _scrap500_list scrap500_list_t;

extern char *scrap500_datadir;

static inline
void scrap500_list_html_filename(scrap500_list_t *list, int page, char *buf)
{
    if (buf)
        sprintf(buf, "%s/list-%d%02d.%d.html",
                     scrap500_datadir, list->id/100, list->id%100, page);
}

int scrap500_http_fetch_list(scrap500_list_t *list);

int scrap500_parser_parse_list(scrap500_list_t *list);

int scrap500_parser_parse_site_record(xmlNode *node, scrap500_site_t *site);

typedef void * scrap500_db_t;

scrap500_db_t scrap500_db_open(const char *dbname, int initdb);

void scrap500_db_close(scrap500_db_t db);

int scrap500_db_write_list(scrap500_db_t db, scrap500_list_t *list);

#endif /* __SCRAP500_H__ */

