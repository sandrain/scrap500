/* Copyright (C) 2019 Hyogi Sim <simh@ornl.gov>
 * ---------------------------------------------------------------------------
 * See COPYING for the license.
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>

#include "scrap500.h"

char *scrap500_datadir = "/tmp/scrap500";

static uint64_t site_id;
static uint64_t system_id;

static void dump_site(uint64_t site_id)
{
    int ret = 0;
    scrap500_site_t site = { 0, };

    ret = scrap500_parser_parse_site(site_id, &site);
    if (ret)
        fprintf(stderr, "failed to parse the site %llu (error: %d)\n",
                        _llu(site_id), ret);
    else
        scrap500_site_dump(&site);
}

static void dump_system(uint64_t system_id)
{
    int ret = 0;
    scrap500_system_t system = { 0, };

    ret = scrap500_parser_parse_system(system_id, &system);
    if (ret)
        fprintf(stderr, "failed to parse the system %llu (error: %d)\n",
                        _llu(system_id), ret);
    else
        scrap500_system_dump(&system);
}

static int populate_site(void)
{
    int ret = 0;
    uint64_t site_id = 0;
    uint64_t count = 0;
    char *pos = NULL;
    DIR *dirp = NULL;
    struct dirent *dp = NULL;
    char path[PATH_MAX] = { 0, };
    scrap500_site_t site = { 0, };

    sprintf(path, "%s/site", scrap500_datadir);

    dirp = opendir(path);
    if (!dirp) {
        fprintf(stderr, "cannot open the directory %s: %s\n",
                        scrap500_datadir, strerror(errno));
        goto out;
    }

    while ((dp = readdir(dirp)) != NULL) {
        if (dp->d_name[0] == '.')
            continue;

        site_id = strtoull(dp->d_name, &pos, 0);
        if (pos[0] != '.')
            continue;

        ret = scrap500_parser_parse_site(site_id, &site);
        if (ret) {
            fprintf(stderr, "failed to parse the site data.\n");
            goto out_close;
        }

        count++;
    }

    printf("processed %llu site records\n", count);

out_close:
    closedir(dirp);
out:
    return ret;
}

static int populate_system(void)
{
    int ret = 0;
    uint64_t system_id = 0;
    uint64_t count = 0;
    char *pos = NULL;
    DIR *dirp = NULL;
    struct dirent *dp = NULL;
    char path[PATH_MAX] = { 0, };
    scrap500_system_t system = { 0, };

    sprintf(path, "%s/system", scrap500_datadir);

    dirp = opendir(path);
    if (!dirp) {
        fprintf(stderr, "cannot open the directory %s: %s\n",
                        scrap500_datadir, strerror(errno));
        goto out;
    }

    while ((dp = readdir(dirp)) != NULL) {
        if (dp->d_name[0] == '.')
            continue;

        system_id = strtoull(dp->d_name, &pos, 0);
        if (pos[0] != '.')
            continue;

        ret = scrap500_parser_parse_system(system_id, &system);
        if (ret) {
            fprintf(stderr, "failed to parse the system data.\n");
            goto out_close;
        }

        count++;
    }

    printf("processed %llu system records\n", count);

out_close:
    closedir(dirp);
out:
    return ret;
}

static int populate_list(void)
{
    int ret = 0;

out:
    return ret;
}

static char program[PATH_MAX];

static struct option const long_opts[] = {
    { "datadir", 1, 0, 'd' },
    { "help", 0, 0, 'h' },
    { 0, 0, 0, 0},
};

static const char *short_opts = "d:hs:S:";

static const char *usage_str =
"Usage: %s [options..]\n"
"\n"
"  available options:\n"
"  -d, --datadir=<path>     store files in <path> (default: /tmp/scrap500)\n"
"  -h, --help               print help message\n"
"  -s, --site=<site_id>     parse <site_id> and print the result\n"
"  -S, --system=<system_id> parse <system_id> and print the result\n"
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

    while ((ch = getopt_long(argc, argv,
                             short_opts, long_opts, &optidx)) >= 0) {
        switch (ch) {
        case 'd':
            scrap500_datadir = strdup(optarg);
            break;

        case 's':
            site_id = strtoull(optarg, NULL, 0);
            break;

        case 'S':
            system_id = strtoull(optarg, NULL, 0);
            break;

        case 'h':
        default:
            usage(0);
            break;
        }
    }

    if (site_id + system_id) {
        if (site_id)
            dump_site(site_id);

        if (system_id)
            dump_system(system_id);

        goto out;
    }

    ret = populate_site();
    if (ret) {
        fprintf(stderr, "failed to populate site data.\n");
        goto out;
    }

    ret = populate_system();
    if (ret) {
        fprintf(stderr, "failed to populate system data.\n");
        goto out;
    }

    ret = populate_list();
    if (ret) {
        fprintf(stderr, "failed to populate list data.\n");
        goto out;
    }

out:
    return ret;
}

