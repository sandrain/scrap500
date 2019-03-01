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
#include <getopt.h>

#include "scrap500.h"

static char *program;

static struct option const long_opts[] = {
    { "all", 0, 0, 'a' },
    { "csv", 1, 0, 'c' },
    { "help", 0, 0, 'h' },
    { "list", 1, 0, 'l' },
    { "sqlite", 1, 0, 's' },
    { 0, 0, 0, 0},
};

int main(int argc, char **argv)
{
    int ret = 0;

    return ret;
}

