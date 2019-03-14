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

static const char *schema_sqlstr =
"--\n"
"-- scrap500.schema.sqlite3.sql\n"
"--\n"
"\n"
"pragma foreign_keys = on;\n"
"\n"
"begin transaction;\n"
"\n"
"drop table if exists site;\n"
"drop table if exists system;\n"
"drop table if exists sysattr_name;\n"
"drop table if exists sysattr_val;\n"
"drop table if exists top500;\n"
"\n"
"-- [table] site\n"
"create table site (\n"
"    id integer primary key not null,\n"
"    site_id integer not null,\n"
"    name text,\n"
"    url text,\n"
"    segment text,\n"
"    city text,\n"
"    country text,\n"
"    unique(site_id)\n"
");\n"
"\n"
"-- [table] system\n"
"create table system (\n"
"    id integer primary key not null,\n"
"    system_id integer not null,\n"
"    site_id integer not null references site(site_id),\n"
"    name text,\n"
"    summary text,\n"
"    manufacturer text,\n"
"    url text,\n"
"    cores real,\n"
"    memory real,\n"
"    processor text,\n"
"    interconnect text,\n"
"    linpack real,\n"
"    tpeak real,\n"
"    nmax real,\n"
"    nhalf real,\n"
"    hpcg real,\n"
"    power real,\n"
"    pml real,\n"
"    mcores real,\n"
"    os text,\n"
"    compiler text,\n"
"    mathlib text,\n"
"    mpi text,\n"
"    unique(system_id)\n"
");\n"
"\n"
#if 0
"-- [table] sysattr_name\n"
"create table sysattr_name (\n"
"    id integer primary key not null,\n"
"    name text not null,\n"
"    unique(name)\n"
");\n"
"\n"
"insert into sysattr_name(id, name) values (1, 'Cores');\n"
"insert into sysattr_name(id, name) values (2, 'Memory');\n"
"insert into sysattr_name(id, name) values (3, 'Processor');\n"
"insert into sysattr_name(id, name) values (4, 'Interconnect');\n"
"insert into sysattr_name(id, name) values (5, 'Linpack Performance (Rmax)');\n"
"insert into sysattr_name(id, name) values (6, 'Theoretical Peak (Rpeak)');\n"
"insert into sysattr_name(id, name) values (7, 'Nmax');\n"
"insert into sysattr_name(id, name) values (8, 'Nhalf');\n"
"insert into sysattr_name(id, name) values (9, 'HPCG [TFlop/s]');\n"
"insert into sysattr_name(id, name) values (10, 'Power');\n"
"insert into sysattr_name(id, name) values (11, 'Power Measurement Level');\n"
"insert into sysattr_name(id, name) values (12, 'Measured Cores');\n"
"insert into sysattr_name(id, name) values (13, 'Operating System');\n"
"insert into sysattr_name(id, name) values (14, 'Compiler');\n"
"insert into sysattr_name(id, name) values (15, 'Math Library');\n"
"insert into sysattr_name(id, name) values (16, 'MPI');\n"
"\n"
"-- [table] sysattr_val\n"
"create table sysattr_val (\n"
"    id integer primary key not null,\n"
"    system_id integer not null references system(system_id),\n"
"    nid integer not null references sysattr_name(id),\n"
"    sval text,          -- text value or unit for rval\n"
"    rval real,\n"
"    unique(system_id, nid)\n"
");\n"
"\n"
#endif
"-- [table] top500\n"
"create table top500 (\n"
"    id integer primary key not null,\n"
"    time integer not null,\n"
"    rank integer not null,\n"
"    system_id integer not null references system(system_id),\n"
"    site_id integer not null references site(site_id),\n"
"    unique(time, rank, system_id)   -- there exist ties in rank\n"
");\n"
"\n"
"end transaction;\n"
"\n";

enum {
    SQL_SITE = 0,
    SQL_SYSTEM,
    SQL_TOP500,
    N_SQLS,
};

static char *sqlstr[] = {
    /* site */
    "insert into site(site_id,name,url,segment,city,country)\n"
    "values(?,?,?,?,?,?);\n",
    /* system */
    "insert into system(system_id,name,summary,manufacturer,url,\n"
    "cores,memory,processor,interconnect,linpack,tpeak,nmax,nhalf,hpcg,\n"
    "power,pml,mcores,os,compiler,mathlib,mpi)\n"
    "values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);\n",
#if 0
    /* sysattr */
    "insert into sysattr_val(system_id,nid,sval,rval)\n"
    "values(?,1,?,?),\n"
    "(?,2,?,?),\n"
    "(?,3,?,?),\n"
    "(?,4,?,?),\n"
    "(?,5,?,?),\n"
    "(?,6,?,?),\n"
    "(?,7,?,?),\n"
    "(?,8,?,?),\n"
    "(?,9,?,?),\n"
    "(?,10,?,?),\n"
    "(?,11,?,?),\n"
    "(?,12,?,?),\n"
    "(?,13,?,?),\n"
    "(?,14,?,?),\n"
    "(?,15,?,?),\n"
    "(?,16,?,?);\n",
#endif
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
    ret |= sqlite3_bind_text(stmt, 2, site->name, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 3, site->url, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 4, site->segment, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 5, site->city, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, 6, site->country, -1, SQLITE_STATIC);
    if (ret) {
        fprintf(stderr, "failed to bind values: %s\n", sqlite3_errstr(ret));
        goto out;
    }

    do {
        ret = sqlite3_step(stmt);
    } while (ret == SQLITE_BUSY);

    if (ret != SQLITE_DONE) {
        fprintf(stderr, "failed to insert site: %s\n", sqlite3_errstr(ret));
        ret = EIO;
    }
    else
        ret = 0;

out:
    sqlite3_reset(stmt);
    return ret;
}

static int db_insert_system(sqlite3 *dbconn, scrap500_system_t *system)
{
    int ret = 0;
    int n = 1;
    uint64_t system_id = system->id;
    sqlite3_stmt *stmt = NULL;

    stmt = sqlstmts[SQL_SYSTEM];

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, system->name, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, n++, system->summary, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, n++, system->manufacturer, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, n++, system->url, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->cores);
    ret |= sqlite3_bind_double(stmt, n++, system->memory);
    ret |= sqlite3_bind_text(stmt, n++, system->processor, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, n++, system->interconnect, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->linpack);
    ret |= sqlite3_bind_double(stmt, n++, system->tpeak);
    ret |= sqlite3_bind_double(stmt, n++, system->nmax);
    ret |= sqlite3_bind_double(stmt, n++, system->nhalf);
    ret |= sqlite3_bind_double(stmt, n++, system->hpcg);
    ret |= sqlite3_bind_double(stmt, n++, system->power);
    ret |= sqlite3_bind_double(stmt, n++, system->pml);
    ret |= sqlite3_bind_double(stmt, n++, system->mcores);
    ret |= sqlite3_bind_text(stmt, n++, system->os, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, n++, system->compiler, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, n++, system->mathlib, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_text(stmt, n++, system->mpi, -1, SQLITE_STATIC);
    if (ret) {
        fprintf(stderr, "failed to bind values: %s\n", sqlite3_errstr(ret));
        goto out;
    }

    do {
        ret = sqlite3_step(stmt);
    } while (ret == SQLITE_BUSY);

    if (ret != SQLITE_DONE) {
        fprintf(stderr, "failed to insert system: %s\n", sqlite3_errstr(ret));
        ret = EIO;
    }
    else
        ret = 0;

out:
    sqlite3_reset(stmt);
    return ret;
}

#if 0
static int db_insert_sysattrs(sqlite3 *dbconn, scrap500_system_t *system)
{
    int ret = 0;
    int n = 1;
    uint64_t system_id = system->id;
    sqlite3_stmt *stmt = NULL;

    stmt = sqlstmts[SQL_SYSATTR];

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->cores);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->memory);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, system->processor, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, system->interconnect, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->linpack_perf);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->theoretical_peak);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->nmax);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->nhalf);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->hpcg);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->power);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->power_measurement_level);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, "", -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, system->measured_cores);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, system->os, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, system->compiler, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, system->mathlib, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);

    ret |= sqlite3_bind_int64(stmt, n++, system_id);
    ret |= sqlite3_bind_text(stmt, n++, system->mpi, -1, SQLITE_STATIC);
    ret |= sqlite3_bind_double(stmt, n++, .0f);

    if (ret) {
        fprintf(stderr, "failed to bind values: %s\n", sqlite3_errstr(ret));
        goto out;
    }

    do {
        ret = sqlite3_step(stmt);
    } while (ret == SQLITE_BUSY);

    if (ret != SQLITE_DONE) {
        fprintf(stderr, "failed to insert system attributes: %s\n",
                        sqlite3_errstr(ret));
        ret = EIO;
    }
    else
        ret = 0;

out:
    sqlite3_reset(stmt);
    return ret;
}
#endif

static int db_insert_list(sqlite3 *dbconn, scrap500_list_t *list)
{
    int ret = 0;
    int i = 0;
    uint64_t list_id = list->id;
    scrap500_rank_t *rank = NULL;
    sqlite3_stmt *stmt = NULL;

    stmt = sqlstmts[SQL_TOP500];

    for (i = 0; i < 500; i++) {
        rank = &list->rank[i];

        ret = sqlite3_bind_int64(stmt, 1, list_id);
        ret |= sqlite3_bind_int(stmt, 2, i+1);
        ret |= sqlite3_bind_int64(stmt, 3, rank->system_id);
        ret |= sqlite3_bind_int64(stmt, 4, rank->site_id);
        if (ret) {
            fprintf(stderr, "failed to bind values: %s\n",
                            sqlite3_errstr(ret));
            ret = EIO;
            goto out;
        }

        do {
            ret = sqlite3_step(stmt);
        } while (ret == SQLITE_BUSY);

        sqlite3_reset(stmt);

        if (ret != SQLITE_DONE) {
            fprintf(stderr, "failed to insert values: %s\n",
                            sqlite3_errstr(ret));
            ret = EIO;
            goto out;
        }
        else
            ret = 0;
    }

out:
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

        count++;

        scrap500_site_reset(&site);
        site.id = site_id;

        printf("## processing site %llu, total %8llu\n",
               _llu(site_id), _llu(count));

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
    }

    printf("\n## processed %llu site records\n", _llu(count));

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

    begin_transaction(db);

    while ((dp = readdir(dirp)) != NULL) {
        if (dp->d_name[0] == '.')
            continue;

        system_id = strtoull(dp->d_name, &pos, 0);
        if (pos[0] != '.')
            continue;

        count++;

        printf("## processing system %llu, total %8llu\n",
               _llu(system_id), _llu(count));

        scrap500_system_reset(&system);
        system.id = system_id;

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

#if 0
        ret = db_insert_sysattrs(db, &system);
        if (ret) {
            fprintf(stderr, "failed to insert the system attributes.\n");
            goto out_close;
        }
#endif
    }

    printf("\n## processed %llu system records\n", _llu(count));

out_close:
    if (ret)
        rollback_transaction(db);
    else
        end_transaction(db);

    closedir(dirp);
out:
    return ret;
}

static inline int get_list_ids(uint64_t **_ids, uint64_t *_count)
{
    int ret = 0;
    int i = 0;
    int count = 0;
    uint64_t id = 0;
    uint64_t *ids = NULL;
    time_t nowp = time(NULL);
    struct tm *now = localtime(&nowp);

    count = 2*((now->tm_year + 1900) - 1993);

    ids = calloc(count, sizeof(*ids));
    if (!ids) {
        perror("failed to allocate list ids");
        ret = errno;
        goto out;
    }

    for (i = 0; i < count; i += 2) {
        id = (1993 + i/2)*100;
        ids[i] = id + 6;
        ids[i+1] = id + 11;
    }

    *_ids = ids;
    *_count = count;

out:
    return ret;
}

static int populate_list(void)
{
    int ret = 0;
    uint64_t i = 0;
    uint64_t count = 0;
    uint64_t *ids = NULL;
    uint64_t list_id = 0;
    scrap500_list_t list = { 0, };

    ret = get_list_ids(&ids, &count);
    if (ret) {
        fprintf(stderr, "failed to get the list ids\n");
        goto out;
    }

    begin_transaction(db);

    for (i = 0; i < count; i++) {
        list_id = ids[i];
        list.id = list_id;

        printf("## processing list %llu, total %8llu\n",
               _llu(list_id), _llu(i+1));

        ret = scrap500_parser_parse_list(&list);
        if (ret) {
            fprintf(stderr, "failed to parse list %llu\n", _llu(list_id));
            goto out_close;
        }

        ret = db_insert_list(db, &list);
        if (ret) {
            fprintf(stderr, "failed to insert the list.\n");
            goto out_close;
        }
    }

    printf("\n## processed %llu lists\n", _llu(count));

out_close:
    if (ret)
        rollback_transaction(db);
    else
        end_transaction(db);

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

    if (output[0] == '\0')
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

