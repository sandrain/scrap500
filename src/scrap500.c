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
#include <sys/time.h>
#include <time.h>
#include <libgen.h>
#include <limits.h>
#include <getopt.h>

#include "scrap500.h"

static int debug;

static uint32_t n_list;
static uint32_t n_list_all;
static scrap500_list_t *scrap500_list;

static uint32_t n_threads;

static inline scrap500_list_t *allocate_list(struct tm *now)
{
    uint32_t years = (now->tm_year + 1900) - 1993;

    n_list_all = years*2;

    return calloc(n_list_all, sizeof(*scrap500_list));
}

static inline void set_all_list(void)
{
    uint32_t i = 0;
    uint32_t id = 0;
    scrap500_list_t *list = NULL;

    for (i = 0; i < n_list_all; i += 2) {
        id = (1993 + i/2)*100;

        list = &scrap500_list[i];
        list->id = id + 6;


        list = &list[1];
        list->id = id + 11;
    }

    n_list = n_list_all;
}

static inline void list_append(char *datestr)
{
    unsigned long date = 0;
    char *endptr = NULL;
    scrap500_list_t *list = NULL;

    errno = 0;

    date = strtoul(datestr, &endptr, 0);
    if (errno) {
        perror("failed to parse the date");
        goto die;
    }

    if (endptr[0] != '\0') {
        fprintf(stderr, "given date %s is not valid.\n", datestr);
        goto die;
    }

    list = &scrap500_list[n_list++];
    list->id = (uint32_t) date;

    return;

die:
    exit(errno);
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

static inline void rewind_tmpfiles(scrap500_list_t *list)
{
    int i = 0;

    if (!list)
        return;

    for (i = 0; i < 5; i++)
        if (list->tmpfp[i])
            rewind(list->tmpfp[i]);
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

static inline void dump_list(scrap500_list_t *list)
{
    int i = 0;
    scrap500_rank_t *rank = NULL;

    if (!list)
        return;

    printf("## list: %d\n", list->id);

    for (i = 0; i < 500; i++) {
        rank = &list->rank[i];

        printf("[%3d] site=%6llu, system=%6llu\n",
                rank->rank, rank->site_id, rank->system_id);
    }
}

static void *scrap500_run(void *data)
{
    int i = 0;
    int ret = 0;
    int page = 0;
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

        if (debug)
            dump_list(list);
    }

out:
    return NULL;
}

static char program[PATH_MAX];

static inline void read_program_name(const char *path)
{
    char *str = strdup(path);
    char *name = basename(str);

    sprintf(program, "%s", name);
    free(str);
}

static struct option const long_opts[] = {
    { "all", 0, 0, 'a' },
    { "debug", 0, 0, 'd' },
    { "csv", 1, 0, 'c' },
    { "help", 0, 0, 'h' },
    { "list", 1, 0, 'l' },
    { "sqlite", 1, 0, 's' },
    { "threads", 1, 0, 'n' },
    { 0, 0, 0, 0},
};

static const char *short_opts = "ac:dhl:st:";

static const char *usage_str =
"Usage: %s [options..]\n"
"\n"
"  available options:\n"
"  -a, --all              get all available list\n"
"  -c, --csv=<dir>        store output in csv format in <dir>\n"
"  -d, --debug            run in a debugging mode with noisy output\n"
"  -h, --help             print help message\n"
"  -l, --list=<YYYYMM>    get the list of <YYYYMM>\n"
"  -s, --sqlite=<db file> store output in sqlite datbase <db file>\n"
"  -t, --threads=<N>      spawn n threads\n"
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
    uint32_t date = 0;
    time_t nowp = 0;
    struct tm *now = NULL;

    read_program_name(argv[0]);

    nowp = time(NULL);
    now = localtime(&nowp);

    scrap500_list = allocate_list(now);
    if (!scrap500_list) {
        perror("failed to allocate memory");
        return ENOMEM;
    }

    while ((ch = getopt_long(argc, argv,
                             short_opts, long_opts, &optidx)) >= 0) {
        switch (ch) {
        case 'a':
            set_all_list();
            break;

        case 'c':
            break;

        case 'd':
            debug = 1;
            break;

        case 'l':
            list_append(optarg);
            break;

        case 's':
            break;

        case 't':
            n_threads = atoi(optarg);
            break;

        case 'h':
        default:
            usage(0);
            break;
        }
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    scrap500_run(NULL);

    curl_global_cleanup();

    return ret;
}

