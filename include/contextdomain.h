/**
 * contextdomain.h
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
#ifndef CONTEXTDOMAIN_H
#define CONTEXTDOMAIN_H
#include "dedupdomains.h"

typedef struct ContextDomain
{
	// regex trimming will be done after consolidation and during the final
	// write.  the full domain is necessary to check against regex. regexes may
	// be found after reading some or all of the files.

	// only need the line numbers when writing to final output.
	linenumber_t *linenumbers;
	size_len_t next_idx; // length and the next spot to insert.
	size_len_t alloc_linenumbers; // total space allocated/available

#ifdef COLLECT_DIAGNOSTICS
	size_len_t count_realloc_linenumbers;
#endif
} ContextDomain_t;

#endif
