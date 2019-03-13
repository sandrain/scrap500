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
#include <libgen.h>
#include <libxml/HTMLparser.h>
#include <curl/curl.h>

#define _llu(x)  ((unsigned long long) (x))
#define __strprint(s)  ((s) ? (s) : "")

struct _scrap500_site {
    uint64_t id;
    char *name;
    char *url;
    char *segment;
    char *city;
    char *country;
};

typedef struct _scrap500_site scrap500_site_t;

static inline void scrap500_site_dump(scrap500_site_t *site)
{
    if (!site)
        return;

    printf("\n## site: %s (id=%llu)\n"
           "## url: %s\n"
           "## segment: %s\n"
           "## city: %s\n"
           "## country: %s\n"
           "\n",
           __strprint(site->name), _llu(site->id),
           __strprint(site->url),
           __strprint(site->segment),
           __strprint(site->city),
           __strprint(site->country));
}

struct _scrap500_system {
    uint64_t id;
    uint64_t site_id;
    char *name;
    char *summary;
    char *url;
    char *manufacturer;

    double cores;
    double memory;
    char *processor;
    char *interconnect;
    double linpack_perf;
    double theoretical_peak;
    double nmax;
    double nhalf;
    double hpcg;
    double power;
    double power_measurement_level;
    double measured_cores;
    char *os;
    char *compiler;
    char *mathlib;
    char *mpi;
};

typedef struct _scrap500_system scrap500_system_t;

static inline void scrap500_system_dump(scrap500_system_t *system)
{
    if (!system)
        return;

    printf("\n## system: %s (id=%llu)\n"
           "## summary: %s\n"
           "## site_id: %lu\n"
           "## url: %s\n"
           "## manufacturer: %s\n"
           "## cores: %lu\n"
           "## memory: %lu GB\n"
           "## processor: %s\n"
           "## interconnect: %s\n"
           "## linpack_perf: %.2lf\n"
           "## theoretical_peak: %.2lf\n"
           "## nmax: %.2lf\n"
           "## nhalf: %.2lf\n"
           "## hpcg: %.2lf\n"
           "## power: %.2lf\n"
           "## power_measurement_level: %.2lf\n"
           "## measured_cores: %.2lf\n"
           "## os: %s\n"
           "## compiler: %s\n"
           "## mathlib: %s\n"
           "## mpi: %s\n"
           "\n",
           __strprint(system->name), _llu(system->id),
           __strprint(system->summary),
           system->site_id,
           __strprint(system->url),
           __strprint(system->manufacturer),
           (unsigned long) system->cores,
           (unsigned long) system->memory,
           __strprint(system->processor),
           __strprint(system->interconnect),
           system->linpack_perf,
           system->theoretical_peak,
           system->nmax,
           system->nhalf,
           system->hpcg,
           system->power,
           system->power_measurement_level,
           system->measured_cores,
           __strprint(system->os),
           __strprint(system->compiler),
           __strprint(system->mathlib),
           __strprint(system->mpi));

}

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

static inline void read_program_name(const char *path, char *program)
{
    char *str = strdup(path);
    char *name = basename(str);

    sprintf(program, "%s", name);
    free(str);
}

static inline
void scrap500_list_html_filename(scrap500_list_t *list, int page, char *buf)
{
    if (buf)
        sprintf(buf, "%s/list/%d%02d.%d.html",
                     scrap500_datadir, list->id/100, list->id%100, page);
}

static inline void scrap500_site_html_filename(uint64_t site_id, char *buf)
{
    if (buf)
        sprintf(buf, "%s/site/%llu.html", scrap500_datadir, _llu(site_id));
}

static inline void scrap500_system_html_filename(uint64_t site_id, char *buf)
{
    if (buf)
        sprintf(buf, "%s/system/%llu.html", scrap500_datadir, _llu(site_id));
}

int scrap500_http_fetch_list(scrap500_list_t *list);

int scrap500_http_fetch_specs(scrap500_list_t *list);

int scrap500_parser_parse_list(scrap500_list_t *list);

int scrap500_parser_parse_specs(scrap500_list_t *list);

int scrap500_parser_parse_site(uint64_t site_id, scrap500_site_t *site);

int scrap500_parser_parse_system(uint64_t system_id, scrap500_system_t *system);

typedef void * scrap500_db_t;

scrap500_db_t scrap500_db_open(const char *dbname, int initdb);

void scrap500_db_close(scrap500_db_t db);

int scrap500_db_write_list(scrap500_db_t db, scrap500_list_t *list);

#endif /* __SCRAP500_H__ */

