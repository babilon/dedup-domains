/**
 * pfb_context.h
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
#ifndef PFB_CONTEXT_H
#define PFB_CONTEXT_H
#include "carry_over.h"

/**
 * Holds file name context information for a single file to be processed. A
 * shared pointer to the single DomainTree_t lives here in an array of size 1.
 * This struct is initialized as an array of size # of files to be processed.
 */
typedef struct pfb_context
{
	FILE *in_file;
	FILE *out_file;
	char *in_fname;
	char *out_fname;
	struct DomainTree **dt;
	/**
	 * Line numbers in 'in_fname' to carry over without modification. These are
	 * not inserted into the DomainTree. They are not omitted by any rules.
	 * Typically these refer to regex lines in the original file. Tracking these
	 * here preserves their original order in the input file. These are carried
	 * until pfb_consolidate().
	 */
	carry_over_t co;
	struct pfb_regexes *regs;
} pfb_context_t;

typedef struct pfb_contexts
{
	pfb_context_t *begin_context;
	pfb_context_t *end_context;
} pfb_contexts_t;

extern struct pfb_contexts pfb_init_contexts(size_t alloc_contexts, const char *out_ext,
		char *const * argv);

extern size_t pfb_len_contexts(struct pfb_contexts *cs);
extern void pfb_free_contexts(struct pfb_contexts *cs);

extern void pfb_close_context(struct pfb_context *c);
extern void pfb_open_context(struct pfb_context *c, bool append_output);
extern void pfb_flush_out_context(struct pfb_context *c);

#endif
