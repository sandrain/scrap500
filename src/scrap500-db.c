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
#include <errno.h>
#include <sqlite3.h>

#include "scrap500.h"

static char *schema =
"begin transaction;\n"
"\n"
"drop table if exists site;\n"
"drop table if exists system;\n"
"drop table if exists list;\n"
"\n"
"create table site (\n"
" id integer primary key not null,\n"
" site_id integer not null,\n"
" name text,\n"
" url text,\n"
" segment text,\n"
" city text,\n"
" country text,\n"
" unique(site_id)\n"
");\n"
"\n"
"create table system (\n"
" id integer primary key not null,\n"
" name text,\n"
" system_id integer not null,\n"
" site_id integer not null,\n"
" manufacturer text,\n"
" cores integer,\n"
" mcores integer, -- measured cores\n"
" memory float,\n"
" processor text,\n"
" interconnect text,\n"
" linpack_rmax float,\n"
" theoretical_peak float,\n"
" nmax float,\n"
" nhalf float,\n"
" hpcg float, -- tflop/s\n"
" power float,\n"
" mpower_level float, -- power measurement level\n"
" os text,\n"
" compiler text,\n"
" mathlib text,\n"
" mpi text,\n"
" url text,\n"
" unique(system_id)\n"
");\n"
"\n"
"create table list (\n"
" id integer primary key not null,\n"
" ym integer not null,\n"
" rank integer not null,\n"
" system_id integer not null references system(system_id),\n"
" site_id integer not null references site(site_id),\n"
" unique(ym, rank, system_id)\n"
");\n"
"\n"
"end transaction;\n"
"\n";

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

scrap500_db_t scrap500_db_open(const char *dbname, int initdb)
{
    int ret = 0;
    sqlite3 *dbconn = NULL;

    if (!dbname)
        goto out;

    ret = sqlite3_open(dbname, &dbconn);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "failed to open db %s (%s)\n",
                        dbname, sqlite3_errstr(ret));
        ret = EIO;
        goto out;
    }

    ret = exec_simple_sql(dbconn, "pragma foreign_keys=on;");
    ret |= exec_simple_sql(dbconn, "pragma temp_store=2;");
    if (ret != SQLITE_OK) {
        fprintf(stderr, "failed to create a database.\n");
        ret = EIO;
    }

    if (initdb) {
        ret = exec_simple_sql(dbconn, schema);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "failed to create a database.\n");
            ret = EIO;
        }
    }

out:
    return (void *) dbconn;
}

void scrap500_db_close(scrap500_db_t db)
{
    sqlite3_close((sqlite3 *) db);
}

int scrap500_db_write_list(scrap500_db_t db, scrap500_list_t *list)
{
    int ret = 0;
    int i = 0;
    sqlite3 *dbconn = (sqlite3 *) db;
    scrap500_rank_t *rank = NULL;
    char sql[8192] = { 0, };
    char *pos = NULL;

    if (!list)
        return EINVAL;

    begin_transaction(dbconn);

    for (i = 0; i < 500; i++) {
        rank = &list->rank[i];

        pos = sql;
        pos += sprintf(pos,
                       "insert or ignore into site (site_id) values (%llu);\n",
                       _llu(rank->site_id));
        pos += sprintf(pos,
                       "insert or ignore into system (system_id) values (%llu);\n",
                       _llu(rank->system_id));
        pos += sprintf(pos,
                       "insert or ignore into list (ym, rank, system_id, site_id)\n"
                       "values (%d, %d, %llu, %llu);\n",
                       list->id, rank->rank,
                       _llu(rank->system_id), _llu(rank->site_id));

        ret = exec_simple_sql(dbconn, sql);
        if (ret != SQLITE_OK) {
            fprintf(stderr, "db query failed, sql=\n%s\nerror: %s\n",
                            sql, sqlite3_errmsg(dbconn));
            rollback_transaction(dbconn);
            goto out;
        }
    }

    end_transaction(dbconn);
out:
    return ret;
}

