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
#ifndef INPUTARGS_H
#define INPUTARGS_H
#include "dedupdomains.h"

typedef struct input_args
{
    /**
     * 'b' command line option to use shared buffer. only valid when NOT using
     * threads.
     */
    bool use_shared_buffer;

    /**
     * 's' set to false to print or log file (if provided) diagnostics and
     * progress.
     */
    bool silent_flag;

    /**
     * 'i' set to true and specify an initial internal buffer size to use for
     * domain consolidation.
     */
    bool override_buffersize;
    /**
     * when override_buffersize is true, set this value to the initial buffer
     * size.
     */
    int initial_buffer_size;

    /**
     * 'r' set to true and specify the number of elements to increase buffer
     * size.
     */
    bool override_reallocsize;
    /**
     * when override_reallocsize is true, set this value to the number of
     * elements to increase the internal buffer size.
     */
    int realloc_buffer_size;

#ifdef BUILD_TESTS
    /**
     * 't' set to true to run internal unit tests.
     */
    bool runtests;
#endif

    /**
     * 'l' set to true and specify a log file to write diagnostics and progress
     * to a log file.
     */
    bool log_flag;
    /**
     * when log_flag is true, specify a file to append diagnostics and progress
     * to.
     */
    char *log_fname;

    /**
     * 'd' set to true and specify a directory to read .fat files and write .txt
     * files.
     */
    bool dir_flag;
    /**
     * when dir_flag is true, specify a directory to read .fat files and write
     * .txt files.
     */
    char *directory;

    /**
     * 'o' set to true and specify the file extension for the pruned output
     * files.
     */
    bool out_ext_flag;
    /**
     * when out_ext_flag is true, specify a file extension for the pruned output
     * files. default is ".txt"
     */
    char const *out_ext;

    /**
     * 'x' set to true and specify the file extension for the fat input files.
     */
    bool inp_ext_flag;
    /**
     * when inp_ext_flag is true, specify a file extension for the fat input
     * files. default is ".fat"
     */
    char const *inp_ext;

    /**
     * FILE for writing diagnostics and progress to. default is stdout.
     */
    FILE *outFile;
    FILE *errFile;

    char ** filenames;
    size_t num_files;

    bool errLog_flag;
    char const *errLog_fname;

} input_args_t;

extern void init_input_args(input_args_t *flags);
extern void free_input_args(input_args_t *flags);
extern bool silent_mode(const input_args_t *flags);
extern bool parse_input_args(int argc, char * const* argv, input_args_t *flags);

extern bool open_logfile(input_args_t *flags);
extern FILE *get_logFile(input_args_t *iargs);
extern void close_logfile(input_args_t *flags);

#define LOG_IFARGS(args, fmt, ...) do { \
    if(!silent_mode(args)) { \
        open_logfile(args); \
        fprintf(get_logFile(args), fmt, ##__VA_ARGS__); \
        close_logfile(args); \
    } \
} while(0)

#endif
