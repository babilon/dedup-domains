/**
 * pfb_prune.h
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
#ifndef PFB_PRUNE_H
#define PFB_PRUNE_H
#include "dedupdomains.h"

struct ArrayDomainInfo;
struct pfb_contexts;

extern char* pfb_strdup(const char *in, size_len_t in_size);
extern void pfb_consolidate(struct DomainTree **root, struct ArrayDomainInfo *array_di);
extern void pfb_read_csv(struct pfb_contexts *cs);
extern void pfb_write_csv(struct pfb_contexts *cs, struct ArrayDomainInfo *array_di, bool);

#ifdef COLLECT_DIAGNOSTICS
extern size_t collected_domains_counter;
#endif

#endif
