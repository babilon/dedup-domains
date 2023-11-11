/**
 * dedupdomains.h
 *
 * Part of pfb_dnsbl_prune
 *
 * Copyright (c) 2023 robert.babilon@gmail.com
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef DEDUPDOMAINS_H
#define DEDUPDOMAINS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include "codecoverage.h"
#include "logdiagnostics.h"

#define UNUSED(x) (void)(x)

#ifdef RELEASE
#define NIL_ASSERT
#elif defined(RELEASE_LOGGING)
#define LOG_AND_CONTINUE
#else
// This will dramatically slow the program down.
//#define HASH_DEBUG
#endif

#if defined(NIL_ASSERT)
#define ASSERT(x) do {} while(0)

#elif defined(EXIT_ON_FATAL)
#define ASSERT(x) do { \
    if(!(x)) { \
        ELOG_STDERR("[%s:%d] %s: 'ASSERT' failed: %s\n", __FILE__, __LINE__, __FUNCTION__, #x); \
        exit(EXIT_FAILURE); \
    } while(0)

#elif defined(LOG_AND_CONTINUE)
#define ASSERT(x) do { \
    if(!(x)) { \
        ELOG_STDERR("[%s:%d] %s: 'ASSERT' failed: %s\n", __FILE__, __LINE__, __FUNCTION__, #x); \
    } \
} while(0)
#else
#define ASSERT(x) assert((x))
#endif

typedef unsigned int uint;
typedef unsigned char uchar;

#ifdef USE_SIZE_T_LINENUMBER
typedef size_t linenumber_t;
#define LINENUMBER_MAX SIZE_MAX
#else
typedef unsigned int linenumber_t;
#define LINENUMBER_MAX UINT_MAX
#endif

#ifdef USE_SIZE_T
typedef size_t size_len_t;
#else
typedef uint size_len_t;
#endif

#if 0
#define DEBUG_PRINTF(fmt, ...) do { \
    fprintf(stdout, fmt, ##__VA_ARGS__); \
} while(0)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif

extern void open_globalErrLog();
FILE *get_globalErrLog();
extern void close_globalErrLog();
extern void free_globalErrLog();

#define ELOG_STDERR(fmt, ...) do { \
    open_globalErrLog(); \
    fprintf(get_globalErrLog(), fmt, ##__VA_ARGS__); \
    close_globalErrLog(); \
} while(0)

#endif
