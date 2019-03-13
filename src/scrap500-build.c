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
#include <sqlite3.h>

#include "scrap500.h"

char *scrap500_datadir = "/tmp/scrap500";
static char output[PATH_MAX];

static uint64_t site_id;
static uint64_t system_id;

static sqlite3 *db;

enum {
    SQL_SITE = 0,
    SQL_SYSTEM,
    SQL_SYSATTR,
    SQL_TOP500,
    N_SQLS,
};

extern const char *schema_sqlstr;

static char *sqlstr[] = {
    /* site */
    "insert into site(site_id,name,url,segment,city,country)\n"
    "values(?,?,?,?,?,?);\n",
    /* system */
    "insert into system(system_id,name,summary,manufacturer,url)\n"
    "values(?,?,?,?,?);\n",
    /* sysattr */
    "insert into sysattr_val(nid,sval,rval)\n"
    "values(1,?,?),\n"
    "values(2,?,?),\n"
    "values(3,?,?),\n"
    "values(4,?,?),\n"
    "values(5,?,?),\n"
    "values(6,?,?),\n"
    "values(7,?,?),\n"
    "values(8,?,?),\n"
    "values(9,?,?),\n"
    "values(10,?,?),\n"
    "values(11,?,?),\n"
    "values(12,?,?),\n"
    "values(13,?,?),\n"
    "values(14,?,?),\n"
    "values(15,?,?),\n"
    "values(16,?,?);\n",
    /* top500 */
    "insert into top500(time,rank,system_id,site_id)\n"
    "values(?,?,?,?);\n",
};

static sqlite3_stmt *sqlstmts[N_SQLS];

static inline int exec_simple_sql(sqlite3 *dbconn, const char *sql)
{
    return sqlite3_exec(dbconn, sql, 0, 0, 0);
}

static inline int begin_transaction(sqlite3 *dbconn)
{
    return exec_simple_sql(dbconn, "begin transaction;");
}

static inline int end_transaction(sqlite3 *dbconn)
{
    return exec_simple_sql(dbconn, "end transaction;");
}

static inline int rollback_transaction(sqlite3 *dbconn)
{
    return exec_simple_sql(dbconn, "rollback transaction;");
}

static sqlite3 *db_init(const char *dbname)
{
    int ret = 0;
    int i = 0;
    sqlite3 *dbconn = NULL;

    ret = sqlite3_open(dbname, &dbconn);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "failed to open database %s: %s\n",
                        dbname, sqlite3_errstr(ret));
    }

    ret = exec_simple_sql(dbconn, "pragma foreign_keys=on;");
    ret |= exec_simple_sql(dbconn, "pragma temp_store=2;");
    if (ret != SQLITE_OK) {
        fprintf(stderr, "failed to set pragma: %s\n",
                        sqlite3_errstr(ret));
        return NULL;
    }

    ret = exec_simple_sql(dbconn, schema_sqlstr);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "failed to create a database: %s\n",
                        sqlite3_errstr(ret));
        return NULL;
    }

    for (i = 0; i < N_SQLS; i++) {
        ret = sqlite3_prepare_v2(dbconn, sqlstr[i], -1, &sqlstmts[i], NULL);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "failed to prepare sql: %s, error: %s\n",
                            sqlstr[i], sqlite3_errstr(ret));
            return NULL;
        }
    }

    return dbconn;
}

static inline int db_close(sqlite3 *dbconn)
{
    return sqlite3_close(dbconn);
}

static int db_insert_site(sqlite3 *dbconn, scrap500_site_t *site)
{
    int ret = 0;
    sqlite3_stmt *stmt = NULL;

    stmt = sqlstmts[SQL_SITE];

    ret = sqlite3_bind_int64(stmt, 1, site->id);
    ret |= sqlite3_bind_text(stmt, 2, __strprint(site->name), -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 3, __strprint(site->url), -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 4, __strprint(site->segment), -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 5, __strprint(site->city), -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 6, __strprint(site->country), -1, SQLITE_STATIC);
    if (ret) {
        fprintf(stderr, "failed to bind values: %s\n", sqlite3_errstr(ret));
        goto out;
    }

    do {
        ret = sqlite3_step(stmt);
    } while (ret = SQLITE_BUSY);

    ret = ret == SQLITE_DONE ? 0 : EIO;
out:
    sqlite3_reset(stmt);
    return ret;
}

static int db_insert_system(sqlite3 *dbconn, scrap500_system_t *system)
{
    int ret = 0;
    sqlite3_stmt *stmt = NULL;

    stmt = sqlstmts[SQL_SYSTEM];

    ret = sqlite3_bind_int64(stmt, 1, system->id);
    ret |= sqlite3_bind_text(stmt, 2, __strprint(system->name), -1,
                             SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 2, __strprint(system->summary), -1,
                             SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 2, __strprint(system->manufacturer), -1,
                             SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 2, __strprint(system->url), -1, 
                             SQLITE_STATIC);
    if (ret) {
        fprintf(stderr, "failed to bind values: %s\n", sqlite3_errstr(ret));
        goto out;
    }

    do {
        ret = sqlite3_step(stmt);
    } while (ret = SQLITE_BUSY);

    ret = ret == SQLITE_DONE ? 0 : EIO;
out:
    sqlite3_reset(stmt);
    return ret;
}

static int db_insert_sysattrs(sqlite3 *dbconn, scrap500_system_t *system)
{
    int ret = 0;
    int n = 1;
    sqlite3_stmt *stmt = NULL;

    stmt = sqlstmts[SQL_SYSTEM];

    ret = sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->cores);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->memory);
    ret |= sqlite3_bind_text(stmt, n++, system->processor, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->linpack_perf);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->theoretical_peak);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->nmax);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->nhalf);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->hpcg);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->power);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->power_measurement_level);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->measured_cores);
    ret |= sqlite3_bind_text(stmt, n++, system->os, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);
    ret |= sqlite3_bind_text(stmt, n++, system->compiler, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);
    ret |= sqlite3_bind_text(stmt, n++, system->mathlib, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);
    ret |= sqlite3_bind_text(stmt, n++, system->mpi, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);

    if (ret) {
        fprintf(stderr, "failed to bind values: %s\n", sqlite3_errstr(ret));
        goto out;
    }

    do {
        ret = sqlite3_step(stmt);
    } while (ret = SQLITE_BUSY);

    ret = ret == SQLITE_DONE ? 0 : EIO;
out:
    sqlite3_reset(stmt);
    return ret;
}

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

    begin_transaction(db);

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

        ret = db_insert_site(db, &site);
        if (ret) {
            fprintf(stderr, "failed to insert the site data.\n");
            goto out_close;
        }

        count++;
    }

    printf("processed %llu site records\n", _llu(count));

out_close:
    if (ret)
        rollback_transaction(db);
    else
        end_transaction(db);

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

        ret = db_insert_system(db, &system);
        if (ret) {
            fprintf(stderr, "failed to insert the system data.\n");
            goto out_close;
        }

        ret = db_insert_sysattrs(db, &system);
        if (ret) {
            fprintf(stderr, "failed to insert the system attributes.\n");
            goto out_close;
        }

        count++;
    }

    printf("processed %llu system records\n", _llu(count));

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
    { "output", 1, 0, 'o' },
    { "site", 1, 0, 's' },
    { "system", 1, 0, 'S' },
    { 0, 0, 0, 0},
};

static const char *short_opts = "d:ho:s:S:";

static const char *usage_str =
"Usage: %s [options..]\n"
"\n"
"  available options:\n"
"  -d, --datadir=<path>     store files in <path> (default: /tmp/scrap500)\n"
"  -h, --help               print help message\n"
"  -o, --output=<filename>  white database to <filename>\n"
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

        case 'o':
            sprintf(output, "%s", optarg);
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

    if (!output)
        sprintf(output, "%s/scrap500.db", scrap500_datadir);

    db = db_init(output);
    if (!db) {
        fprintf(stderr, "failed to create database %s\n", output);
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

out_close:
    db_close(db);

out:
    return ret;
}

