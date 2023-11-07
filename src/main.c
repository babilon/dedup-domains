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
#include "test.h"
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>

extern void set_DomainInfo_array_size(int v);
extern void set_realloc_DomainInfo_size(int v);
extern char* outputfilename(const char *input, const char *ext);

static void free_filenames_array(char ***filenames, size_t num_files)
{
    for(size_t i = 0; i < num_files; i++)
    {
        free((*filenames)[i]);
    }
    free(*filenames);
    filenames = NULL;
}

static bool append_filename_array(char ***filenames, size_t *num_files, char **entry)
{
    ASSERT(filenames);
    ASSERT(num_files);
    ASSERT(entry);
    ASSERT(*entry);
    ASSERT((*entry)[0] != '\0');

    // not allowing an empty string
    if((*entry)[0] == '\0')
        return false;

    (*num_files)++;
    char **new_files = realloc(*filenames, sizeof(char*) * *num_files);
    if(new_files)
    {
        *filenames = new_files;
        (*filenames)[*num_files - 1] = *entry;
        *entry = NULL;
        return true;
    }

    return false;
}

int main(int argc, char * const* argv)
{
    // command line option to use shared buffer. only valid when NOT using
    // threads.
    bool use_shared_buffer = true;

#ifdef RELEASE
    // smoke alarm for release builds. verify assert('invariant') is non-fatal.
    ASSERT(false);
    assert(false);
#endif

    bool silent_flag = false;
    bool override_buffersize = false;
    bool override_reallocsize = false;
    bool runtests = false;
    int initial_buffer_size = 0;
    int realloc_buffer_size = 0;

    bool dir_flag = false;
    char *directory = NULL;

    bool out_ext_flag = false;
    bool inp_ext_flag = false;

    char const *inp_ext = ".fat";
    char const *out_ext = ".txt";

    int errorFlag = 0;
    char opt;

    while((opt = getopt(argc, argv, ":si:r:b:td:x:o:")) != -1)
    {
        switch(opt)
        {
            case 's':
                silent_flag = true;
                break;
            case 'i':
                if(!override_buffersize)
                {
                    override_buffersize = true;
                    initial_buffer_size = atoi(optarg);
                }
                else
                {
                    fprintf(stderr, "Option -i (override initial DomainInfo buffer size) is expected at most once.\n");
                    errorFlag++;
                }
                break;
            case 'r':
                if(!override_reallocsize)
                {
                    override_reallocsize = true;
                    realloc_buffer_size = atoi(optarg);
                }
                else
                {
                    fprintf(stderr, "Option -r (override realloc DomainInfo buffer size) is expected at most once.\n");
                    errorFlag++;
                }
                break;
            case 't':
#ifndef BUILD_TESTS
                fprintf(stderr, "NOTICE: option -t (run built-in unit tests) will be ignored; binary was built without unit tests.\n");
#endif
                runtests = true;
                break;
            case 'd':
                dir_flag = true;
                directory = optarg;
                break;
            case 'x':
                // default to ".fat"
                inp_ext_flag = true;
                inp_ext = optarg;
                break;
            case 'o':
                // default to ".txt"
                out_ext_flag = true;
                out_ext = optarg;
                break;
            case ':':
                fprintf(stderr, "Option -%c requires an operand\n", optopt);
                errorFlag++;
                break;
            case '?':
            default:
                fprintf(stderr, "Usage: %s [-irt] [file1, file2, ...]\n", argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }

    if(errorFlag)
    {
        fprintf(stderr, "Usage: %s [-irt] [file1, file2, ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if(!silent_flag && override_buffersize)
    {
        fprintf(stdout, "NOTE: Overriding initial buffer size to %d\n", initial_buffer_size);
        set_DomainInfo_array_size(initial_buffer_size);
    }

    if(!silent_flag && override_reallocsize)
    {
        fprintf(stdout, "NOTE: Overriding realloc buffer size to %d\n", realloc_buffer_size);
        set_realloc_DomainInfo_size(realloc_buffer_size);
    }

    if(runtests)
    {
#ifdef BUILD_TESTS
        extern void run_tests();
        run_tests();
        return 0;
#endif
    }

    if(!dir_flag && optind == argc)
    {
        fprintf(stderr, "ERROR: Provide a directory with option -d <dir> OR at least one file name.\n");
        exit(EXIT_FAILURE);
    }

    if(dir_flag && optind != argc)
    {
        fprintf(stderr, "ERROR: Option -d <dir> and optional file names [file 1, file2, ...] are mutually exclusive.\n");
        exit(EXIT_FAILURE);
    }

    if(inp_ext_flag && (!inp_ext || inp_ext[0] != '.'))
    {
        fprintf(stderr, "ERROR: Input file extension must begin with a period.\n");
        exit(EXIT_FAILURE);
    }

    if(out_ext_flag && (!out_ext || out_ext[0] != '.'))
    {
        fprintf(stderr, "ERROR: Output file extension must begin with a period.\n");
        exit(EXIT_FAILURE);
    }

    char **filenames = NULL;
    size_t num_files = 0;

    if(dir_flag )
    {
        // checked and handled above.
        ASSERT(optind == argc);

        if(!silent_flag)
        {
            printf("Prune duplicate entries from all '*%s' files in %s and write to '*%s' files\n",
                    inp_ext, directory, out_ext);
        }

        struct stat s;
        if(stat(directory, &s) == 0)
        {
            if(!S_ISDIR(s.st_mode))
            {
                fprintf(stderr, "ERROR: Expected '%s' to be a directory\n", directory);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Unable to stat directory '%s'\n", directory);
            exit(EXIT_FAILURE);
        }

        struct dirent *direntry;
        DIR *dir = opendir(directory);
        if(!dir)
        {
            fprintf(stderr, "ERROR: Unable to open directory '%s'\n", directory);
            exit(EXIT_FAILURE);
        }

        while((direntry = readdir(dir)) != NULL)
        {
            struct stat s;

            const size_t len_dir = strlen(directory);
            const size_t len_entry = strlen(direntry->d_name);
            char *tmp = calloc(len_dir + len_entry + 1, sizeof(char));
            strcpy(tmp, directory);
            strcpy(tmp + len_dir, direntry->d_name);
            if(stat(tmp, &s) == 0 && S_ISREG(s.st_mode))
            {
                const char *period = strrchr(direntry->d_name, '.');
                if(period && *(period + 1))
                {
                    if(strcmp(period, inp_ext) == 0)
                    {
                        if(!silent_flag)
                        {
                            char *t = outputfilename(direntry->d_name, out_ext);
                            if(t)
                            {
                                printf("   READ: %s\n  WRITE: %s\n", direntry->d_name, t);
                            }
                            free(t);
                            printf("\n");
                        }

                        if(!append_filename_array(&filenames, &num_files, &tmp))
                        {
                            free_filenames_array(&filenames, num_files - 1);
                            closedir(dir);
                            exit(EXIT_FAILURE);
                        }
                    }
                    else if(strcmp(period, out_ext) == 0)
                    {
                        printf("WARNING: will overwrite %s\n\n", direntry->d_name);
                    }

                }
            }
            free(tmp);
        }

        if(closedir(dir) != 0)
        {
            free_filenames_array(&filenames, num_files);

            fprintf(stderr, "ERROR: Unable to close directory '%s'.\n", directory);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // checked and handled above.
        ASSERT(optind != argc);

        if(!silent_flag)
        {
            printf("Prune duplicate entries from the following files:\n");
        }

        for(int c = optind; c < argc; c++)
        {
            struct stat s;
            if(stat(argv[c], &s) == 0)
            {
                if(!S_ISREG(s.st_mode))
                {
                    fprintf(stderr, "ERROR: Expected a file: %s\n", argv[c]);
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                fprintf(stderr, "ERROR: Unable to stat input file name %s\n", argv[c]);
                exit(EXIT_FAILURE);
            }

            if(!silent_flag)
            {
                char *tmp = outputfilename(argv[c], out_ext);
                if(tmp)
                {
                    printf("   READ: %s\n  WRITE: %s\n", argv[c], tmp);
                }
                free(tmp);

                if(c + 1 < argc)
                    printf("\n");
            }
        }
    }

    ASSERT(argc > optind);
    const size_t alloc_contexts = num_files ? num_files : (size_t)(argc - optind);

    // iterate over the input arguments and create context for each one
    pfb_contexts_t contexts = pfb_init_contexts(alloc_contexts, out_ext,
            (const char* const*)(filenames ? filenames : argv + optind));

    free(filenames);

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
