/* Copyright (C) 2019 Hyogi Sim <simh@ornl.gov>
 * ---------------------------------------------------------------------------
 * See COPYING for the license.
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>

#include "scrap500.h"

static scrap500_list_t *scrap500_list;
static uint64_t n_list;

char *scrap500_datadir = "/tmp/scrap500";

static inline int allocate_list(void)
{
    time_t tnow = time(NULL);
    struct tm *now = localtime(&tnow);
    uint32_t years = (now->tm_year + 1900) - 1993;
    uint32_t i = 0;
    uint32_t id = 0;
    scrap500_list_t *list = NULL;

    n_list = years*2;
    scrap500_list = (scrap500_list_t *) calloc(n_list, sizeof(*list));
    if (!scrap500_list) {
        perror("cannot allocate memory for the list");
        return errno;
    }

    for (i = 0; i < n_list; i += 2) {
        id = (1993 + i/2)*100;

        list = &scrap500_list[i];
        list->id = id + 6;

        list = &list[1];
        list->id = id + 11;
    }

    return 0;
}

static int prepare_datadir(void)
{
    int ret = 0;
    char cmd[4096] = { 0, };

    sprintf(cmd, "[ ! -d \"%s\" ] && mkdir %s",
                 scrap500_datadir, scrap500_datadir);
    ret = system(cmd);

    sprintf(cmd, "[ ! -d \"%s/list\" ] && mkdir %s/list",
                 scrap500_datadir, scrap500_datadir);
    ret = system(cmd);

    sprintf(cmd, "[ ! -d \"%s/site\" ] && mkdir %s/site",
                 scrap500_datadir, scrap500_datadir);
    ret = system(cmd);

    sprintf(cmd, "[ ! -d \"%s/system\" ] && mkdir %s/system",
                 scrap500_datadir, scrap500_datadir);
    ret = system(cmd);

out:
    return 0;
}

static inline int open_tmpfiles(scrap500_list_t *list)
{
    int i = 0;
    FILE *fp = NULL;
    char filename[PATH_MAX] = { 0, };

    if (!list)
        return EINVAL;

    for (i = 0; i < 5; i++) {
        scrap500_list_html_filename(list, i+1, filename);
        fp = fopen(filename, "w");
        if (!fp) {
            perror("failed to create a tmpfile");
            return errno;
        }

        list->tmpfp[i] = fp;
    }

    return 0;
}

static inline void close_tmpfiles(scrap500_list_t *list)
{
    int i = 0;

    if (!list)
        return;

    for (i = 0; i < 5; i++)
        if (list->tmpfp[i])
            fclose(list->tmpfp[i]);
}


static int do_fetch(void)
{
    int i = 0;
    int ret = 0;
    int page = 0;
    scrap500_db_t db = NULL;
    scrap500_list_t *list = NULL;

    for (i = 0; i < n_list; i++) {
        list = &scrap500_list[i];
        if (!list)
            continue;

        open_tmpfiles(list);

        ret = scrap500_http_fetch_list(list);
        if (ret) {
            fprintf(stderr, "failed to fetch the list.\n");
            goto out;
        }

        close_tmpfiles(list);

        ret = scrap500_parser_parse_list(list);
        if (ret) {
            fprintf(stderr, "failed to parse the list.\n");
            goto out;
        }

        ret = scrap500_http_fetch_specs(list);
        if (ret) {
            fprintf(stderr, "failed to fetch specifications.\n");
            goto out;
        }
    }

out:
    return ret;
}

static char program[PATH_MAX];

static struct option const long_opts[] = {
    { "datadir", 1, 0, 'd' },
    { "help", 0, 0, 'h' },
    { 0, 0, 0, 0},
};

static const char *short_opts = "d:h";

static const char *usage_str =
"Usage: %s [options..]\n"
"\n"
"  available options:\n"
"  -d, --datadir=<path>   store files in <path> (default: /tmp/scrap500)\n"
"  -h, --help             print help message\n"
"\n";

static inline void usage(int ec)
{
    fprintf(stdout, usage_str, program);
    exit(ec);
}

int main(int argc, char **argv)
{
    int ret = 0;
    int optidx = 0;
    int ch = 0;

    read_program_name(argv[0], program);

    ret = allocate_list();
    if (ret)
        goto out;

    while ((ch = getopt_long(argc, argv,
                             short_opts, long_opts, &optidx)) >= 0) {
        switch (ch) {
        case 'd':
            scrap500_datadir = strdup(optarg);
            break;

        case 'h':
        default:
            usage(0);
            break;
        }
    }

    prepare_datadir();

    ret = do_fetch();

out:
    return ret;
}

