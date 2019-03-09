/* Copyright (C) 2019 Hyogi Sim <simh@ornl.gov>
 * ---------------------------------------------------------------------------
 * See COPYING for the license.
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <libxml/HTMLparser.h>
#include <sqlite3.h>

static const char *datadir;
static DIR *dirp;

static char *schema =
"begin transaction;\n"
"drop table if exists sysattr;\n"
"create table sysattr (\n"
" id integer not null primary key,\n"
" attr text not null,\n"
" count integer not null default 1,\n"
" unique(attr)\n"
");\n"
"end transaction;\n";

static const char *dbname = "sysattrs.sqlite3.db";
static sqlite3 *dbconn;

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

static xmlNode *get_child_element(xmlNode *node, const char *name, int nth)
{
    int count = 0;
    xmlNode *current = NULL;

    for (current = node->children; current; current = current->next) {
        if (strcmp((char *) current->name, name))
            continue;

        count++;
        if (count == nth)
            return current;
    }

    return NULL;
}

static char *get_child_text(xmlNode *node, const char *name, int nth)
{
    int count = 0;
    xmlNode *current = NULL;

    for (current = node->children; current; current = current->next) {
        if (strcmp((char *) current->name, name))
            continue;

        count++;

        if (count == nth) {
            current = get_child_element(current, "text", 1);
            return current ? (char *) current->content : NULL;
        }
    }

    return NULL;
}

static const char *insert_sql =
  "insert into sysattr(attr) values(?)\n"
  "on conflict(attr) do update set count=count+1;\n";
static sqlite3_stmt *insert_stmt;

static inline int parse_system_table(xmlNode *table)
{
    int ret = 0;
    xmlNode *th = NULL;
    xmlNode *tr = NULL;
    uint64_t count = 0;
    char *pos = NULL;
    char attr[512] = { 0, };

    tr = table->children;

    do {
        th = tr->children;
        sprintf(attr, "%s", (char *) th->children->content);
        if (NULL != (pos = strchr(attr, ':')))
            *pos = '\0';

        ret = sqlite3_bind_text(insert_stmt, 1, attr, -1, SQLITE_STATIC);
        if (ret) {
            fprintf(stderr, "db bind error (%s)\n", sqlite3_errmsg(dbconn));
            ret = EIO;
            goto out;
        }

        ret = sqlite3_step(insert_stmt);
        if (ret != SQLITE_DONE) {
            fprintf(stderr, "db query error (%s)\n", sqlite3_errmsg(dbconn));
            ret = EIO;
            goto out;
        }

        sqlite3_reset(insert_stmt);
    } while (NULL != (tr = tr->next));

    ret = 0;

out:
    return ret;
}

static int do_system_html(const char *filename)
{
    int ret = 0;
    int opts = 0;
    htmlDocPtr doc = NULL;
    xmlNode *root = NULL;
    xmlNode *tmp = NULL;
    xmlNode *table = NULL;
    char pathname[PATH_MAX] = { 0, };

    sprintf(pathname, "%s/%s", datadir, filename);

    printf("parsing.. %s\n", pathname);

    opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR
           | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

    doc = htmlReadFile(pathname, NULL, opts);
    if (!doc) {
        fprintf(stderr, "cannot parse the document %s\n", filename);
        ret = EIO;
        goto out;
    }

    root = xmlDocGetRootElement(doc);
    if (!root) {
        fprintf(stderr, "cannot parse the document %s\n", filename);
        ret = EIO;
        goto out;
    }

    tmp = get_child_element(root, "body", 1);
    tmp = get_child_element(tmp, "div", 2);
    tmp = get_child_element(tmp, "div", 1);
    tmp = get_child_element(tmp, "div", 1);
    table = get_child_element(tmp, "table", 1);

    ret = parse_system_table(table);
    if (ret)
        fprintf(stderr, "failed to parse the system record.\n");

out:
    if (doc)
        xmlFreeDoc(doc);

    return ret;
}

static int do_sysattr(void)
{
    int ret = 0;
    int count = 0;
    uint64_t id = 0;
    struct dirent *dp = NULL;
    char *pos = NULL;

    while (NULL != (dp = readdir(dirp))) {
        id = strtoull(dp->d_name, &pos, 0);

        if (strcmp(pos, ".html"))
            continue;

        ret = do_system_html(dp->d_name);
        if (ret) {
            fprintf(stderr, "failed on %s\n", dp->d_name);
            goto out;
        }

        count++;
    }

    printf("%d entries.\n", count);

out:
    return ret;
}

int main(int argc, char **argv)
{
    int ret = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <dirname>\n", argv[0]);
        return 1;
    }

    datadir = argv[1];
    dirp = opendir(datadir);
    if (!dirp) {
        perror("failed to open directory");
        return 1;
    }

    ret = sqlite3_open(dbname, &dbconn);
    assert(ret == SQLITE_OK);

    ret = exec_simple_sql(dbconn, schema);
    if (ret) {
        fprintf(stderr, "query failed: %s\nerror: %s\n",
                        schema, sqlite3_errstr(ret));
        goto out;
    }

    exec_simple_sql(dbconn, "pragram temp_store=2;");

    ret = sqlite3_prepare(dbconn, insert_sql, -1, &insert_stmt, NULL);
    if (ret != SQLITE_OK) {
        fprintf(stderr, "failed to prepare db query (%s)\n",
                        sqlite3_errmsg(dbconn));
        goto out;
    }

    begin_transaction(dbconn);

    ret = do_sysattr();
    if (ret)
        fprintf(stderr, "failed to process attributes.\n");

out:
    if (ret)
        rollback_transaction(dbconn);
    else
        end_transaction(dbconn);

    sqlite3_close(dbconn);

    closedir(dirp);

    return ret;
}

