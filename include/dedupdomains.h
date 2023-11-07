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

#define UNUSED(x) (void)(x)

#ifdef RELEASE
#define NIL_ASSERT
#else
// This will dramatically slow the program down.
//#define HASH_DEBUG
#endif

#if defined(NIL_ASSERT)
#define ASSERT(x) do {} while(0)

#elif defined(EXIT_ON_FATAL)
#define ASSERT(x) do { if(!(x)) exit(EXIT_FAILURE); } while(0)

#elif defined(LOG_AND_CONTINUE)
#define ASSERT(x) do { if(!(x)) { fprintf(stderr, "[%s:%d] %s failed assert: %s", __FILE__, __LINE__, __FUNCTION__, #x); } } while(0)

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

#ifdef BUILD_TESTS
// for printfs in a few places.
#define DEBUG_MODE if(0)
#else
#define DEBUG_MODE if(0)
#endif

#ifdef COLLECT_DIAGNOSTICS
#define BORROW_SPACES \
    char *spaces_str = NULL; \
    do { \
        const char *tmp1 = __FILE__; \
        const char *tmp2 = __FUNCTION__; \
        size_t spaces = strlen(tmp1) + strlen(tmp2); \
        spaces_str = calloc(spaces + 1, sizeof(char)); \
        memset(spaces_str, ' ', spaces); \
        spaces_str[spaces] = '\0'; \
    } while(0)
#define RETURN_SPACES do { free(spaces_str); } while(0)
#endif


#endif
