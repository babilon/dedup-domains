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
#include "carry_over.h"
#include "domain.h"

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
     * Transitional payload during pfb_insert(). Carries CsvLineView data
     * through insert_DomainTree() for insertion (if unique) into the
     * DomainTree. Initialized once. Free'ed in pfb_close_context().
     */
    DomainView_t dv;
    /**
     * Line numbers in 'in_fname' to carry over without modification. These are
     * not inserted into the DomainTree. They are not omitted by any rules.
     * Typically these refer to regex lines in the original file. Tracking these
     * here preserves their original order in the input file. These are carried
     * until pfb_consolidate().
     */
    carry_over_t co;
} pfb_context_t;

typedef struct pfb_contexts
{
    pfb_context_t *begin_context;
    pfb_context_t *end_context;
} pfb_contexts_t;

typedef struct ContextDomain
{
#ifdef REGEX_ENABLED
    // regex trimming might be done during consolidation or after consolidation.
    // the full domain is necessary to check against regex. regexes may be found
    // after reading some or all of the files.
    struct DomainInfo **di;
#else
    // only need the line numbers when writing to final output.
    linenumber_t *linenumbers;
#endif
    size_len_t next_idx; // length and the next spot to insert.
    size_len_t alloc_linenumbers; // total space allocated/available

#ifdef COLLECT_DIAGNOSTICS
    size_len_t count_realloc_linenumbers;
#endif
} ContextDomain_t;

typedef struct ArrayDomainInfo
{
    // one of these for each pfb_context_t
    ContextDomain_t *cd;
    // number of ContextDomain allocated
    // same size as pfb_contexts_t::end_context - pfb_contexts_t::begin_context
    size_len_t len_cd;

    // used to calculate the index into 'cd'. the pfb_context_t array and the
    // 'cd' array are the same size.  this is here because collect_DomainInfo()
    // will have a DomainInfo with a context pointer and will use it to
    // calculate the index into the 'cd' array.
    pfb_context_t *begin_pfb_context;
} ArrayDomainInfo_t;

extern pfb_contexts_t pfb_init_contexts(size_t alloc_contexts, const char *out_ext,
        char *const * argv);

extern size_t pfb_len_contexts(pfb_contexts_t *cs);
extern void pfb_free_contexts(pfb_contexts_t *cs);

extern void pfb_close_context(pfb_context_t *c);
extern void pfb_open_context(pfb_context_t *c, bool append_output);
extern void pfb_flush_out_context(pfb_context_t *c);

extern void pfb_consolidate(struct DomainTree **root, ArrayDomainInfo_t *array_di);
extern void pfb_read_csv(pfb_contexts_t *cs);
extern void pfb_write_csv(pfb_contexts_t *cs, ArrayDomainInfo_t *array_di, bool);

extern void init_ArrayDomainInfo(ArrayDomainInfo_t *array_di, size_t alloc_contexts);
extern void free_ArrayDomainInfo(ArrayDomainInfo_t *array_di);

#ifdef COLLECT_DIAGNOSTICS
extern size_t collected_domains_counter;
#endif

#endif
