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

int scrap500_get_http(const char *url, FILE *tmpfp)
{
    int ret = 0;

    return ret;
}

