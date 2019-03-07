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
#include <unistd.h>
#include <curl/curl.h>

#include "scrap500.h"

#define call_curl(fn)                                           \
        do {                                                    \
            CURLcode c = (fn);                                  \
            if (c != CURLE_OK)                                  \
                fprintf(stderr, "%s\n", curl_easy_strerror(c)); \
        } while (0)

#if 0
int scrap500_get_http(const char *url, FILE *fp)
{
    CURL *curl = NULL;
    CURLcode cc = 0;

    printf("%s\n", url);
    rewind(fp);

    curl = curl_easy_init();

    cc = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fp);
    cc |= curl_easy_setopt(curl, CURLOPT_URL, url);
    if (cc != CURLE_OK) {
        fprintf(stderr, "## curl processing failed: %s\n",
                        curl_easy_strerror(cc));
        return EIO;
    }

    cc = curl_easy_perform(curl);
    if (cc != CURLE_OK) {
        fprintf(stderr, "## curl processing failed: %s\n",
                        curl_easy_strerror(cc));
        return EIO;
    }

    curl_easy_cleanup(curl);

    rewind(fp);

    return 0;
}
#endif

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

