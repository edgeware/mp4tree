#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/*
 ******************************************************************************
 *                           Runtime options                                  *
 ******************************************************************************
 */

struct options_struct
{
    const char * filter;
    const char * filename;
    const char * initseg;
    int          truncate;
    bool         selftest;
} g_options;

