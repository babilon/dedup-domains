/**
 * main.c
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
#include "dedupdomains.h"
#include "csvline.h"
#include "domain.h"
#include "uthash.h"
#include "domaintree.h"
#include "rw_pfb_csv.h"
#include "pfb_prune.h"
#include "inputargs.h"
#include "test.h"
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>

extern void set_DomainInfo_array_size(int v);
extern void set_realloc_DomainInfo_size(int v);

int main(int argc, char *const * argv)
{
#ifdef RELEASE
    // smoke alarm for release builds. verify assert('invariant') is non-fatal.
    ASSERT(false);
    assert(false);
#endif

    input_args_t flags;
    init_input_args(&flags);

    if(!parse_input_args(argc, argv, &flags))
    {
        exit(EXIT_FAILURE);
    }

    if(flags.override_buffersize)
    {
        if(!silent_mode(&flags))
        {
            open_logfile(&flags);
            fprintf(flags.outFile, "NOTE: Overriding initial buffer size to %d\n",
                    flags.initial_buffer_size);
            close_logfile(&flags);
        }
        set_DomainInfo_array_size(flags.initial_buffer_size);
    }

    if(flags.override_reallocsize)
    {
        if(!silent_mode(&flags))
        {
            open_logfile(&flags);
            fprintf(flags.outFile, "NOTE: Overriding realloc buffer size to %d\n",
                    flags.realloc_buffer_size);
            close_logfile(&flags);
        }
        set_realloc_DomainInfo_size(flags.realloc_buffer_size);
    }

    if(!silent_mode(&flags))
    {
        open_logfile(&flags);
        fprintf(flags.outFile, "Prune duplicate entries from the following files:\n");
        for(size_t i = 0; i < flags.num_files; i++)
        {
            fprintf(flags.outFile, "  %s\n", flags.filenames[i]);
        }

        fprintf(flags.outFile, "Prune duplicate entries from all '*%s' files in %s and write to '*%s' files\n",
                flags.inp_ext, flags.directory, flags.out_ext);
        close_logfile(&flags);
    }

#ifdef BUILD_TESTS
    if(flags.runtests)
    {
        extern void run_tests();
        run_tests();
        return 0;
    }
#endif

    assert(flags.num_files);

    const bool use_shared_buffer = flags.use_shared_buffer;

    // iterate over the input arguments and create context for each one
    pfb_contexts_t contexts = pfb_init_contexts(flags.num_files, flags.out_ext,
            flags.filenames);

    free_input_args(&flags);

    assert(contexts.begin_context->dt);
    assert(!contexts.begin_context->dt[0]);

    // Effectively, the DomainTree is held by the collection of contexts. this
    // allows the contexts to be the root of the data structure. after the tree
    // is served it's purpose, the pointers to the DomainTree should be nil.
    for(pfb_context_t *c = contexts.begin_context; c != contexts.end_context; c++)
    {
        // every context references the same dt. only have to null the place holder
        // for one of the contexts.
        assert(c->dt == contexts.begin_context->dt);
        assert(!c->dt[0]);
    }

    assert(contexts.begin_context->dt);
    assert(contexts.begin_context->dt[0] == NULL);

    // open the files to verify all files can be read. open output files to
    // verify those can be written.
    pfb_read_csv(&contexts);

    // confirm that reading didn't bork a pointer
    assert(contexts.begin_context->dt);
    assert(contexts.begin_context->dt[0]);

    // confirm DomainTree is not borked after reading.
    for(pfb_context_t *c = contexts.begin_context; c != contexts.end_context; c++)
    {
        assert(c->dt == contexts.begin_context->dt);
        assert(c->dt[0] == contexts.begin_context->dt[0]);
    }

    // the tree has the full set of DomainInfo. allocate a chunk of space to
    // hold DomainInfo and grow as needed. move DomainInfo over to the new
    // array. free the tree as items move to the array.
    //
    // the number of contexts is expected to be small enough to not be a problem.
    //
    // create an ArrayDomain_t for each context to collect the domains for each
    // FILE to be written to.
    ArrayDomainInfo_t array_di;
    init_ArrayDomainInfo(&array_di, pfb_len_contexts(&contexts));
    array_di.begin_pfb_context = contexts.begin_context;

    pfb_consolidate(contexts.begin_context->dt, &array_di);

    // DomainTree is free'd during above consolidation
    assert(contexts.begin_context->dt);
    assert(!contexts.begin_context->dt[0]);

    // write all unique domains to respective output files
    pfb_write_csv(&contexts, &array_di, use_shared_buffer);

    free_ArrayDomainInfo(&array_di);
    pfb_free_contexts(&contexts);

    assert(!contexts.begin_context);
    assert(!contexts.end_context);
    assert(!array_di.begin_pfb_context);

#ifdef COLLECT_DIAGNOSTICS
    printf("Collected %lu unique domains.\n", collected_domains_counter);
#endif

    return 0;
}
