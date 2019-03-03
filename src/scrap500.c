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
#include <pthread.h>

#include "scrap500.h"

static uint32_t n_lists;
static uint32_t *lists;

static uint32_t n_threads;

static inline uint32_t *allocate_lists(struct tm *now)
{
    uint32_t years = (now->tm_year + 1900) - 1993;

    return calloc(years*2, sizeof(*lists));
}

static inline void set_all_lists(void)
{
    uint32_t i = 0;
    uint32_t list_id = 0;

    for (i = 0; i < n_lists; i += 2) {
        list_id = (1993 + i)*100;

        lists[i] = list_id + 6;
        lists[i+1] = list_id + 11;
    }
}

static inline void list_append(char *datestr)
{
    unsigned long date = 0;
    char *endptr = NULL;

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

    lists[n_lists++] = (uint32_t) date;

    return;

die:
    exit(errno);
}

static void *scrap500_run(void *data)
{
    uint32_t i = 0;

    for (i = 0; i < n_lists; i++) {
        if (lists[i])
            printf("%u: %u\n", i, lists[i]);
    }

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
    { "csv", 1, 0, 'c' },
    { "help", 0, 0, 'h' },
    { "list", 1, 0, 'l' },
    { "sqlite", 1, 0, 's' },
    { "threads", 1, 0, 'n' },
    { 0, 0, 0, 0},
};

static const char *short_opts = "ac:hl:st:";

static const char *usage_str =
"Usage: %s [options..]\n"
"\n"
"  available options:\n"
"  -a, --all              get all available lists\n"
"  -c, --csv=<dir>        store output in csv format in <dir>\n"
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

    lists = allocate_lists(now);
    if (!lists) {
        perror("failed to allocate memory");
        return ENOMEM;
    }

    while ((ch = getopt_long(argc, argv,
                             short_opts, long_opts, &optidx)) >= 0) {
        switch (ch) {
        case 'a':
            set_all_lists();
            break;

        case 'c':
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

    scrap500_run(NULL);

    return ret;
}

