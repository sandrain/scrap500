/* Copyright (C) 2019 - UT-Battelle, LLC. All right reserved.
 * 
 * Please refer to COPYING for the license.
 * Written by: Hyogi Sim <sandrain@gmail.com>
 * ---------------------------------------------------------------------------
 * 
 */
#include <config.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>

#include "scrap500.h"

#define call_curl(fn)                                           \
        do {                                                    \
            CURLcode c = (fn);                                  \
            if (c != CURLE_OK)                                  \
                fprintf(stderr, "%s\n", curl_easy_strerror(c)); \
        } while (0)

static inline void prepare_list(CURLM *cm, scrap500_list_t *list)
{
    int i = 0;
    char *url = NULL;
    FILE *fp = NULL;
    CURL *curl = NULL;
    CURLcode cc = 0;

    for (i = 0; i < 5; i++) {
        curl = curl_easy_init();

        fp = list->tmpfp[i];
        url = list->url[i];
        sprintf(url, "https://www.top500.org/list/%d/%d/?page=%d",
                     list->id/100, list->id%100, i+1);

        cc = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fp);
        cc |= curl_easy_setopt(curl, CURLOPT_URL, url);
        if (cc != CURLE_OK) {
            fprintf(stderr, "## curl processing failed: %s\n",
                            curl_easy_strerror(cc));
            return;
        }

        curl_multi_add_handle(cm, curl);
    }
}

int scrap500_http_fetch_list(scrap500_list_t *list)
{
    int ret = 0;
    int still_running = 0;
    int len = 0;
    int repeats = 0;
    int http_rc = 0;
    CURLM *cm = NULL;
    CURL *curl = NULL;
    CURLcode cc = 0;
    CURLMsg *msg = NULL;

    if (!list)
        return EINVAL;

    cm = curl_multi_init();

    prepare_list(cm, list);

    curl_multi_perform(cm, &still_running);

    while (still_running) {
        CURLMcode mc = 0;
        int numfds = 0;

        mc = curl_multi_wait(cm, NULL, 0, 1000, &numfds);
        if (mc != CURLM_OK) {
            fprintf(stderr, "curl multi processing failed: %d\n", mc);
            break;
        }

        if (!numfds) {
            repeats++;
            if (repeats > 1)
                usleep(1e5);
        }
        else
            repeats = 0;

        curl_multi_perform(cm, &still_running);
    }

    while (NULL != (msg = curl_multi_info_read(cm, &len))) {
        if (msg->msg == CURLMSG_DONE) {
            curl = msg->easy_handle;

            cc = msg->data.result;
            if (cc != CURLE_OK) {
                fprintf(stderr, "curl handle failed: %s\n",
                                curl_easy_strerror(cc));
                ret = EIO;
            }

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_rc);

            if (http_rc != 200) {
                fprintf(stderr, "curl failed: HTTP status code=%d\n", http_rc);
                ret = EIO;
            }
        }
        else
            fprintf(stderr, "curl multi processing error (msg=%d)\n",
                            msg->msg);

        curl_multi_remove_handle(cm, curl);
        curl_easy_cleanup(curl);

        if (ret)
            break;
    }

out:
    curl_multi_cleanup(cm);

    return ret;
}

struct _worker_data {
    int range;
    scrap500_list_t *list;
};

typedef struct _worker_data worker_data_t;

static void *site_fetch_func(void *_data)
{
    int ret = 0;
    int i = 0;
    int fd = 0;
    FILE *fp = NULL;
    worker_data_t *data = (worker_data_t *) _data;
    int range = data->range;
    scrap500_list_t *list = data->list;
    scrap500_rank_t *rank = NULL;
    char filename[PATH_MAX] = { 0, };
    CURL *curl = NULL;
    CURLcode cc = 0;

//    for (i = range*100; i < range*100+100; i++) {
    for (i = 0; i < 500; i++) {
        rank = &list->rank[i];
        scrap500_site_html_filename(rank->site_id, filename);

        fd = open(filename, O_RDWR|O_CREAT|O_EXCL, 0644);
        if (fd < 0) {
            if (errno == EEXIST)
                continue;
            else {
                fprintf(stderr, "failed to create a file %s: %s\n",
                                filename, strerror(errno));
                goto out;
            }
        }

        fp = fdopen(fd, "w");
        if (!fp) {
            fprintf(stderr, "failed to open file %s: %s\n",
                            filename, strerror(errno));
            goto out;
        }

        curl = curl_easy_init();
        if (!curl) {
            fprintf(stderr, "curl init failed for %s\n", filename);
            goto out;
        }

        sprintf(filename, "https://www.top500.org/site/%llu",
                          _llu(rank->site_id));
        printf("downloading.. %s\n", filename);

        cc = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fp);
        cc |= curl_easy_setopt(curl, CURLOPT_URL, filename);
        if (cc != CURLE_OK) {
            fprintf(stderr, "curl error: %s\n", curl_easy_strerror(cc));
            goto out;
        }

        cc = curl_easy_perform(curl);
        if (cc != CURLE_OK) {
            fprintf(stderr, "curl processing failed for %s: %s\n",
                            filename, curl_easy_strerror(cc));
            goto out;
        }

        if (curl)
            curl_easy_cleanup(curl);

        fclose(fp);
    }

out:
    return NULL;
}

static void *system_fetch_func(void *_data)
{
    int ret = 0;
    int i = 0;
    int fd = 0;
    FILE *fp = NULL;
    worker_data_t *data = (worker_data_t *) _data;
    int range = data->range;
    scrap500_list_t *list = data->list;
    scrap500_rank_t *rank = NULL;
    char filename[PATH_MAX] = { 0, };
    CURL *curl = NULL;
    CURLcode cc = 0;

//    for (i = range*100; i < range*100+100; i++) {
    for (i = 0; i < 500; i++) {
        rank = &list->rank[i];
        scrap500_system_html_filename(rank->system_id, filename);

        fd = open(filename, O_RDWR|O_CREAT|O_EXCL, 0644);
        if (fd < 0) {
            if (errno == EEXIST)
                continue;
            else {
                fprintf(stderr, "failed to create a file %s: %s\n",
                                filename, strerror(errno));
                goto out;
            }
        }

        fp = fdopen(fd, "w");
        if (!fp) {
            fprintf(stderr, "failed to open file %s: %s\n",
                            filename, strerror(errno));
            goto out;
        }

        curl = curl_easy_init();
        if (!curl) {
            fprintf(stderr, "curl init failed for %s\n", filename);
            goto out;
        }

        sprintf(filename, "https://www.top500.org/system/%llu",
                          _llu(rank->system_id));
        printf("downloading.. %s\n", filename);

        cc = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fp);
        cc |= curl_easy_setopt(curl, CURLOPT_URL, filename);
        if (cc != CURLE_OK) {
            fprintf(stderr, "curl error: %s\n", curl_easy_strerror(cc));
            goto out;
        }

        cc = curl_easy_perform(curl);
        if (cc != CURLE_OK) {
            fprintf(stderr, "curl processing failed for %s: %s\n",
                            filename, curl_easy_strerror(cc));
            goto out;
        }

        if (curl)
            curl_easy_cleanup(curl);

        fclose(fp);
    }

out:
    return NULL;
}

int scrap500_http_fetch_specs(scrap500_list_t *list)
{
    int ret = 0;
    int i = 0;
#if 0
    int nthreads = 5;
    pthread_t workers[5];
    worker_data_t data[5];
#endif
    worker_data_t data = { 0, };

    if (!list)
        return EINVAL;

    data.range = 0;
    data.list = list;

    site_fetch_func((void *) &data);
    system_fetch_func((void *) &data);

#if 0
    for (i = 0; i < nthreads; i++) {
        worker_data_t *d = &data[i];
        d->range = i;
        d->list = list;

        ret = pthread_create(&workers[i], NULL,
                             site_fetch_func, (void *) d);
        if (ret) {
            perror("failed to create a site fetcher thread");
            goto out;
        }
    }

    for (i = 0; i < nthreads; i++)
        ret = pthread_join(workers[i], NULL);

    for (i = 0; i < nthreads; i++) {
        worker_data_t *d = &data[i];
        d->range = i;
        d->list = list;

        ret = pthread_create(&workers[i], NULL,
                             system_fetch_func, (void *) d);
        if (ret) {
            perror("failed to create a site fetcher thread");
            goto out;
        }
    }

    for (i = 0; i < nthreads; i++)
        ret = pthread_join(workers[i], NULL);
#endif

out:
    return ret;
}

