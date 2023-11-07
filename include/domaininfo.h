/**
 * domaininfo.h
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
#ifndef DOMAIN_INFO_H
#define DOMAIN_INFO_H
#include "dedupdomains.h"
#include "matchstrength.h"

struct DomainView;

typedef struct DomainInfo
{
    // Pointer to pfb_context_t for FILE read from and FILE to write to. Carried
    // here to remember during consolidate and write which file to retrieve the
    // line number from and file to write to.
    void *context;
    linenumber_t linenumber;

#if defined(BUILD_TESTS) || defined(REGEX_ENABLED)
    // useful/necessary for tests and debug.
    // necessary for regex pruning
    char *fqd;
#endif
    size_len_t len; // number of characters in fqd
    MatchStrength_t match_strength;
    bool alive;
} DomainInfo_t;

extern DomainInfo_t* convert_DomainInfo(struct DomainView *dv);
extern void free_DomainInfo(DomainInfo_t **di);

#endif
