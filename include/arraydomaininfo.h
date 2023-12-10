/**
 * arraydomaininfo.h
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
#ifndef ARRAYDOMAININFO_H
#define ARRAYDOMAININFO_H
#include "dedupdomains.h"

struct ContextDomain;
struct pfb_context;

typedef struct ArrayDomainInfo
{
	// one of these for each pfb_context_t
	struct ContextDomain *cd;
	// number of ContextDomain allocated
	// same size as pfb_contexts_t::end_context - pfb_contexts_t::begin_context
	size_len_t len_cd;

	// used to calculate the index into 'cd'. the pfb_context_t array and the
	// 'cd' array are the same size.  this is here because collect_DomainInfo()
	// will have a DomainInfo with a context pointer and will use it to
	// calculate the index into the 'cd' array.
	struct pfb_context *begin_pfb_context;
} ArrayDomainInfo_t;

extern void init_ArrayDomainInfo(struct ArrayDomainInfo *array_di, size_t alloc_contexts);
extern void free_ArrayDomainInfo(struct ArrayDomainInfo *array_di);

#endif
