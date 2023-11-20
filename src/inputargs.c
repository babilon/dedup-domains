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
#include "inputargs.h"
#include "version.nogit.h"
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <dirent.h>

#define ELOG_IFARGS(args, fmt, ...) do { \
    open_logfile(args); \
    fprintf(get_elogFile(args), fmt, ##__VA_ARGS__); \
    close_logfile(args); \
} while(0)

#ifdef BUILD_TESTS
#define TESTS_PRINTF
#endif

#define PATH_SEP_CHAR '/'

extern char* outputfilename(const char *input, const char *ext);

bool open_logfile(input_args_t *iargs)
{
    assert(iargs && "tests only");
    if(iargs->log_fname)
    {
        iargs->outFile = fopen(iargs->log_fname, "ab");
        if(!iargs->outFile)
        {
            iargs->outFile = stdout;
            fprintf(stderr, "ERROR: Unable to open %s for append writing.\n", iargs->log_fname);
        }
        else
        {
            iargs->errFile = iargs->outFile;
            ADD_CC;
            return true;
        }
    }
    ADD_CC;

    return false;
}

void close_logfile(input_args_t *iargs)
{
    assert(iargs && "tests only");
    if(iargs->log_fname)
    {
        ASSERT(iargs->outFile);
        ASSERT(iargs->errFile);
        ASSERT(iargs->outFile != stdout);
        ASSERT(iargs->errFile != stderr);
        fclose(iargs->outFile);
        iargs->outFile = stdout;
        iargs->errFile = stderr;
        ADD_CC;
    }
    ADD_CC;
}

FILE *get_logFile(input_args_t *iargs)
{
    assert(iargs && "tests only");
#ifdef BUILD_TESTS
    UNUSED(iargs);
    return stdout;
#else
    return iargs->outFile == NULL ? stdout : iargs->outFile;
#endif
}

static FILE *get_elogFile(input_args_t *iargs)
{
    assert(iargs && "tests only");
#ifdef BUILD_TESTS
    UNUSED(iargs);
    return stderr;
#else
    return iargs->errFile == NULL ? stderr : iargs->errFile;
#endif
}

typedef struct globalLog
{
    const char *log_fname;
    FILE *file;
} globalLog_t;

static globalLog_t *global_errLog = NULL;
static globalLog_t *global_stdLog = NULL;

void open_globalErrLog()
{
    if(global_errLog)
    {
        if(global_errLog->log_fname && global_errLog->file == NULL)
        {
            global_errLog->file = fopen(global_errLog->log_fname, "ab");
            if(!global_errLog->file)
            {
                fprintf(stderr, "ERROR: Unable to open %s for append writing.\n", global_errLog->log_fname);
            }
            ADD_CC;
        }
        ADD_CC;
    }
    ADD_CC;
}

void close_globalErrLog()
{
    if(global_errLog && global_errLog->log_fname && global_errLog->file &&
            global_errLog->file != stderr)
    {
        fclose(global_errLog->file);
        global_errLog->file = NULL;
        ADD_CC;
    }
    ADD_CC;
}

FILE *get_globalErrLog()
{
    if(global_errLog && global_errLog->log_fname && global_errLog->file)
    {
        ADD_CC;
        return global_errLog->file;
    }

    ADD_CC;
    return stderr;
}

void free_globalErrLog()
{
    if(global_errLog)
    {
        free(global_errLog);
        global_errLog = NULL;
    }
}

void open_globalStdLog()
{
    if(global_stdLog)
    {
        if(global_stdLog->log_fname && global_stdLog->file == NULL)
        {
            global_stdLog->file = fopen(global_stdLog->log_fname, "ab");
            if(!global_stdLog->file)
            {
                fprintf(stderr, "ERROR: Unable to open %s for append writing.\n", global_stdLog->log_fname);
            }
            ADD_CC;
        }
        ADD_CC;
    }
    ADD_CC;
}

void close_globalStdLog()
{
    if(global_stdLog && global_stdLog->log_fname && global_stdLog->file &&
            global_stdLog->file != stderr)
    {
        fclose(global_stdLog->file);
        global_stdLog->file = NULL;
        ADD_CC;
    }
    ADD_CC;
}

FILE *get_globalStdLog()
{
    if(global_stdLog && global_stdLog->log_fname && global_stdLog->file)
    {
        ADD_CC;
        return global_stdLog->file;
    }

    ADD_CC;
    return stdout;
}

void free_globalStdLog()
{
    if(global_stdLog)
    {
        free(global_stdLog);
        global_stdLog = NULL;
    }
}

static void free_filenames_array(char ***filenames, size_t num_files)
{
    for(size_t i = 0; i < num_files; i++)
    {
        free((*filenames)[i]);
        (*filenames)[i] = NULL;
        ADD_CC;
    }
    ADD_CC;
}

static void append_filename_array(char ***filenames, size_t *num_files,
        char *entry)
{
    ASSERT(filenames);
    ASSERT(num_files);
    ASSERT(entry);
    ASSERT(*entry);
    ASSERT(entry[0] != '\0');

    // not allowing an empty string
    if(entry[0] == '\0')
    {
        ELOG_STDERR("WARNING: Ignoring empty filename.\n");
        return;
    }

    (*num_files)++;
    char **new_files = realloc(*filenames, sizeof(char*) * *num_files);
    if(new_files)
    {
        *filenames = new_files;
        (*filenames)[*num_files - 1] = entry;
        ADD_CC;
    }
    else
    {
        ELOG_STDERR("ERROR: Failed to realloc memory for filenames[]\n");
        exit(EXIT_FAILURE);
    }
    ADD_CC;
}

static void add_filename(input_args_t *iargs, char *entry)
{
    append_filename_array(&iargs->filenames, &iargs->num_files, entry);
    ADD_CC;
}

void init_input_args(input_args_t *iargs)
{
    memset(iargs, 0, sizeof(input_args_t));
    iargs->use_shared_buffer = true;
    iargs->inp_ext = ".fat";
    iargs->out_ext = ".txt";
    iargs->outFile = stdout;
    iargs->errFile = stderr;
    ADD_CC;
}

void free_input_args(input_args_t *iargs)
{
    if(iargs->dir_flag)
    {
        free_filenames_array(&iargs->filenames, iargs->num_files);
        ADD_CC;
    }

    free(iargs->filenames);
    iargs->filenames = NULL;

    memset(iargs, 0, sizeof(input_args_t));
    ADD_CC;
}

/**
 * Returns true if friendly diagnostic messages should be suppressed, i.e., to
 * NOT print supplemental information.
 */
bool silent_mode(const input_args_t *iargs)
{
    ADD_CC;
    return (iargs->silent_flag && !iargs->log_flag);
}

/**
 * getopt() dictates const-signature of 'argv'.
 */
static bool do_parse_input_args(int argc, char * const* argv, input_args_t *iargs)
{
    int errorFlag = 0;
    char opt;

    // getopt(int, char * const *, char const *);
    while(errorFlag == 0 && (opt = getopt(argc, argv, ":vstbL:i:r:d:x:o:E:")) != -1)
    {
        switch(opt)
        {
            case 'v':
                printf("Version: %s\n", VERSIONID);
                break;
            case 's':
                iargs->silent_flag = true;
                ADD_CC;
                break;
            case 'L':
                iargs->log_flag = true;
                iargs->log_fname = optarg;
                ADD_CC;
                break;
            case 'E':
                iargs->errLog_flag = true;
                iargs->errLog_fname = optarg;
                ADD_CC;
                break;
            case 'i': // # elements for initial alloc buffer
                if(!iargs->override_buffersize)
                {
                    iargs->override_buffersize = true;
                    iargs->initial_buffer_size = atoi(optarg);
                    ADD_CC;
                }
                else
                {
                    ELOG_IFARGS(iargs, "Option -i (override initial DomainInfo buffer size) is expected at most once.\n");
                    errorFlag++;
                    ADD_CC;
                }
                break;
            case 'r': // # elements for realloc buffers
                if(!iargs->override_reallocsize)
                {
                    iargs->override_reallocsize = true;
                    iargs->realloc_buffer_size = atoi(optarg);
                    ADD_CC;
                }
                else
                {
                    ELOG_IFARGS(iargs, "Option -r (override realloc DomainInfo buffer size) is expected at most once.\n");
                    errorFlag++;
                    ADD_CC;
                }
                break;
            case 't':
#ifndef BUILD_TESTS
                ELOG_IFARGS(iargs, "NOTICE: option -t (run built-in unit tests) will be ignored; binary was built without unit tests.\n");
#else
                iargs->runtests = true;
                ADD_TCC;
#endif
                break;
            case 'd':
                iargs->dir_flag = true;
                iargs->directory = optarg;
                ADD_CC;
                break;
            case 'x':
                // default to ".fat"
                iargs->inp_ext_flag = true;
                iargs->inp_ext = optarg;
                ADD_CC;
                break;
            case 'o':
                // default to ".txt"
                iargs->out_ext_flag = true;
                iargs->out_ext = optarg;
                ADD_CC;
                break;
            case ':':
                ELOG_IFARGS(iargs, "Option -%c requires an operand\n", optopt);
                errorFlag++;
                ADD_CC;
                break;
            case '?':
            default:
                ELOG_IFARGS(iargs, "Usage: %s "
                        "[-vstb] "
                        "[-L <log file>] "
                        "[-E <errlog file>] "
                        "[-i <NUMBER>] "
                        "[-r <NUMBER>] "
                        "[-d <directory>] "
                        "[-x .<in ext>] "
                        "[-o .<out ext>] "
                        "[file1, file2, ...] "
                        "\n", argv[0]);
                errorFlag++;
                ADD_CC;
                break;
        }
    }

    if(errorFlag)
    {
        ADD_CC;
        return false;
    }

    if(iargs->log_flag)
    {
        if(!open_logfile(iargs))
        {
            errorFlag++;
            ADD_CC;
            return false;
        }
        close_logfile(iargs);

        struct stat s;
        if(stat(iargs->log_fname, &s) == 0)
        {
            if(!S_ISREG(s.st_mode))
            {
                ELOG_STDERR("ERROR: '%s' is not a regular file\n", iargs->log_fname);
                errorFlag++;
                return false;
            }
            ADD_CC;
        }
        else
        {
            ELOG_STDERR("ERROR: Unable to stat: %s\n", iargs->log_fname);
            errorFlag++;
            return false;
        }
        ADD_CC;

        global_stdLog = calloc(1, sizeof(globalLog_t));
        global_stdLog->log_fname = iargs->log_fname;
    }

    if(iargs->errLog_flag)
    {
        global_errLog = calloc(1, sizeof(globalLog_t));
        global_errLog->log_fname = iargs->errLog_fname;
    }

#ifdef BUILD_TESTS
    if(iargs->runtests)
    {
        ADD_TCC;
        return true;
    }
#endif

    if(!iargs->dir_flag && optind == argc)
    {
        ELOG_IFARGS(iargs, "ERROR: Provide a directory with option -d <dir> OR at least one file name.\n");
        errorFlag++;
        ADD_CC;
        return false;
    }

    if(iargs->dir_flag && optind != argc)
    {
        ELOG_IFARGS(iargs, "ERROR: Option -d <dir> and optional file names [file 1, file2, ...] are mutually exclusive.\n");
        errorFlag++;
        ADD_CC;
        return false;
    }

    if(iargs->inp_ext_flag && (!iargs->inp_ext || iargs->inp_ext[0] != '.'))
    {
        ELOG_IFARGS(iargs, "ERROR: Input file extension must begin with a period.\n");
        errorFlag++;
        ADD_CC;
        return false;
    }

    if(iargs->out_ext_flag && (!iargs->out_ext || iargs->out_ext[0] != '.'))
    {
        ELOG_IFARGS(iargs, "ERROR: Output file extension must begin with a period.\n");
        errorFlag++;
        ADD_CC;
        return false;
    }

    ADD_CC;
    return true;
}

static void log_action(const char *fname, input_args_t *iargs)
{
    if(silent_mode(iargs))
    {
        ADD_CC;
        return;
    }

    char *tmp = outputfilename(fname, iargs->out_ext);
    if(tmp)
    {
        LOG_IFARGS(iargs, "   READ: %s\n  WRITE: %s\n", fname, tmp);
        ADD_CC;
    }
    free(tmp);
    ADD_CC;
}

static int sort_filenames(void const *a, void const *b)
{
    char *a_fn = *(char**)a;
    char *b_fn = *(char**)b;
    return strcmp(a_fn, b_fn);
}

static bool read_dir_filenames(input_args_t *iargs)
{
    ASSERT(iargs);
    ASSERT(iargs->dir_flag);
    ASSERT(iargs->directory);

    struct stat s;
    if(stat(iargs->directory, &s) == 0)
    {
        if(!S_ISDIR(s.st_mode))
        {
            ELOG_IFARGS(iargs, "ERROR: Expected '%s' to be a directory\n",
                    iargs->directory);
            ADD_CC;
            return false;
        }
        ADD_CC;
    }
    else
    {
        ELOG_IFARGS(iargs, "ERROR: Unable to stat directory '%s'\n",
                iargs->directory);
        ADD_CC;
        return false;
    }

    struct dirent *direntry;
    DIR *dir = opendir(iargs->directory);
    if(!dir)
    {
        ELOG_IFARGS(iargs, "ERROR: Unable to open directory '%s'\n",
                iargs->directory);
        //ADD_CC; // unlikely
        return false;
    }

    while((direntry = readdir(dir)) != NULL)
    {
        struct stat s;

        const size_t len_dir = strlen(iargs->directory);
        const size_t len_entry = strlen(direntry->d_name);

        // null terminator plus 1 IFF no trailing '/'
        const uint extra = (iargs->directory[len_dir - 1] != PATH_SEP_CHAR);

        char *tmp = calloc(len_dir + len_entry + 1 + extra, sizeof(char));

        strcpy(tmp, iargs->directory);

        if(extra)
        {
            tmp[len_dir] = PATH_SEP_CHAR;
            ADD_CC;
        }

        strcpy(tmp + len_dir + extra, direntry->d_name);

        if(stat(tmp, &s) == 0 && S_ISREG(s.st_mode))
        {
            const char *period = strrchr(direntry->d_name, '.');
            if(period && *(period + 1))
            {
                if(strcmp(period, iargs->inp_ext) == 0)
                {
                    LOG_IFARGS(iargs, "Found regular file with matching input extension: %s\n", tmp);
                    // tmp is moved to filenames[].
                    add_filename(iargs, tmp);
                    tmp = NULL;

                    log_action(direntry->d_name, iargs);
                    ADD_CC;
                }
                else if(!silent_mode(iargs) && strcmp(period, iargs->out_ext) == 0)
                {
                    LOG_IFARGS(iargs, "Found regular file with matching output extension: %s\n"
                                      "WARNING: will overwrite %s\n", tmp, direntry->d_name);
                    ADD_CC;
                }
            }
        }
        free(tmp);
        ADD_CC;
    }

    if(closedir(dir) != 0)
    {
        ELOG_IFARGS(iargs, "WARNING: Unable to close directory '%s'.\n",
                iargs->directory);
        //ADD_CC; // unlikely
    }

    qsort(iargs->filenames, iargs->num_files, sizeof(char*), sort_filenames);

    ADD_CC;
    return true;
}

/**
 * parse_input_args() dictates the const-signature of 'argv'.
 */
static bool read_argv_filenames(int argc, char *const *argv, input_args_t *iargs)
{
    ASSERT(iargs);
    ASSERT(optind != argc);

    for(int c = optind; c < argc; c++)
    {
        struct stat s;
        if(stat(argv[c], &s) == 0)
        {
            if(!S_ISREG(s.st_mode))
            {
                ELOG_IFARGS(iargs, "ERROR: Expected a file: %s\n", argv[c]);
                ADD_CC;
                return false;
            }
        }
        else
        {
            ELOG_IFARGS(iargs, "ERROR: Unable to stat input file name %s\n",
                    argv[c]);
            ADD_CC;
            return false;
        }

        log_action(argv[c], iargs);

        add_filename(iargs, argv[c]);
        ADD_CC;
    }

    if(optind < argc)
    {
        iargs->num_files = argc - optind;
        ADD_CC;
    }
    else
    {
        ELOG_IFARGS(iargs, "ERROR: Missing arguments.\n");
        //ADD_CC; // unlikely
        return false;
    }

    ADD_CC;
    return true;
}

/**
 * do_parse_input_args() dictates the const-signature of 'argv'.
 */
bool parse_input_args(int argc, char *const * argv, input_args_t *iargs)
{
    if(!do_parse_input_args(argc, argv, iargs))
    {
        ADD_CC;
        return false;
    }

#ifdef BUILD_TESTS
    if(iargs->runtests)
    {
        ADD_TCC;
        return true;
    }
#endif

    if(iargs->dir_flag)
    {
        ASSERT(argc == optind);
        ADD_CC;
        return read_dir_filenames(iargs);
    }
    else
    {
        ADD_CC;
        return read_argv_filenames(argc, argv, iargs);
    }
}

#ifdef BUILD_TESTS
static void check_initial_input_args(input_args_t *args)
{
    assert(args->use_shared_buffer == true);

    assert(!args->silent_flag);

    assert(!args->inp_ext_flag);
    assert(!args->out_ext_flag);
    assert(strcmp(args->inp_ext, ".fat") == 0);
    assert(strcmp(args->out_ext, ".txt") == 0);

    assert(args->filenames == NULL);
    assert(args->num_files == 0);
    assert(args->outFile == stdout);
    assert(args->errFile = stderr);
    ADD_TCC;
}

static void test_init_input_args()
{
    input_args_t args;
    init_input_args(&args);

    check_initial_input_args(&args);
    ADD_TCC;
}

static void test_free_input_args()
{
    input_args_t args, args_zero;

    memset(&args_zero, 0, sizeof(input_args_t));

    init_input_args(&args);

    free_input_args(&args);

    assert(memcmp(&args, &args_zero, sizeof(input_args_t)) == 0);
    free_input_args(&args);
    assert(memcmp(&args, &args_zero, sizeof(input_args_t)) == 0);

    init_input_args(&args);
    check_initial_input_args(&args);
    ADD_TCC;
}

static void test_silent_mode()
{
    input_args_t args;
    init_input_args(&args);

    // NOT silent by default
    assert(!silent_mode(&args));

    // set silent flag
    args.silent_flag = true;
    assert(silent_mode(&args));

    free_input_args(&args);
    init_input_args(&args);

    // NOT silent by default
    assert(!silent_mode(&args));
    args.log_flag = true;
    assert(!silent_mode(&args));
    ADD_TCC;
}

#define TC_DPIA(numargs, argsin, argsout, expect_ret) do { \
    free_input_args(&argsout); \
    init_input_args(&argsout); \
    optind = 1; \
    fprintf(stdout, "Parse %d args for prog: %s\n", numargs, argsin[0]); \
    assert(expect_ret == do_parse_input_args(numargs, argsin, &argsout)); \
} while(0)

static void test_do_parse_input_args()
{
    input_args_t args;
    init_input_args(&args);

    char *argsin1[] = {"prog.real", "-t"};
    TC_DPIA(2, argsin1, args, true);
    assert(args.runtests);
    assert(!args.override_reallocsize);
    assert(args.realloc_buffer_size == 0);
    assert(!args.override_buffersize);
    assert(args.initial_buffer_size == 0);
    assert(!silent_mode(&args));

    char *argsin2[] = {"prog2.real", "-s", "-d", "./tests/001_inputs"};
    TC_DPIA(4, argsin2, args, true);
    assert(!args.runtests);
    assert(!args.override_reallocsize);
    assert(args.realloc_buffer_size == 0);
    assert(!args.override_buffersize);
    assert(args.initial_buffer_size == 0);
    assert(args.dir_flag);
    // file names are parsed later
    assert(!args.filenames);
    assert(args.num_files == 0);
    assert(strcmp(args.directory, "./tests/001_inputs") == 0);
    assert(silent_mode(&args));
    assert(args.outFile == stdout);

    char *argsin3[] = {"prog3.real", "-s", "-d"};
    TC_DPIA(3, argsin3, args, false);

    char *argsin4[] = {"prog4.real", "-t", "-i44"};
    TC_DPIA(3, argsin4, args, true);
    assert(args.override_buffersize);
    assert(args.initial_buffer_size == 44);
    assert(!args.override_reallocsize);
    assert(args.realloc_buffer_size == 0);

    char *argsin5[] = {"prog5.real", "-t", "-r51"};
    TC_DPIA(3, argsin5, args, true);
    assert(args.override_reallocsize);
    assert(args.realloc_buffer_size == 51);
    assert(!args.override_buffersize);
    assert(args.initial_buffer_size == 0);

    char *argsin6[] = {"prog6.real", "-x.nothere", "file1"};
    TC_DPIA(3, argsin6, args, true);
    assert(args.inp_ext_flag);
    assert(strcmp(args.inp_ext, ".nothere") == 0);
    assert(!args.out_ext_flag);
    assert(strcmp(args.out_ext, ".txt") == 0);

    char *argsin7[] = {"prog7.real", "-o", ".overrideout", "file2"};
    TC_DPIA(4, argsin7, args, true);
    assert(args.out_ext_flag);
    assert(strcmp(args.out_ext, ".overrideout") == 0);
    assert(!args.inp_ext_flag);
    assert(strcmp(args.inp_ext, ".fat") == 0);

    char *argsin8[] = {"prog8.real", "-o", ".overrideout", "-d", "bonus dir", "-x", ".fake"};
    TC_DPIA(7, argsin8, args, true);
    assert(args.out_ext_flag);
    assert(strcmp(args.out_ext, ".overrideout") == 0);
    assert(args.inp_ext_flag);
    assert(strcmp(args.inp_ext, ".fake") == 0);
    assert(args.dir_flag);
    assert(strcmp(args.directory, "bonus dir") == 0);

    char *argsin9[] = {"prog9.real", "-o", ".overrideout", "-d"};
    TC_DPIA(4, argsin9, args, false);
    assert(args.out_ext_flag);
    assert(strcmp(args.out_ext, ".overrideout") == 0);
    assert(!args.dir_flag);

    char *argsin10[] = {"prog10.real", "-o", ".overrideout", "-x", ".pruned"};
    TC_DPIA(5, argsin10, args, false);
    assert(args.out_ext_flag);
    assert(strcmp(args.out_ext, ".overrideout") == 0);
    assert(args.inp_ext_flag);
    assert(strcmp(args.inp_ext, ".pruned") == 0);

    char *argsin11[] = {"prog11.real", "-d", "overrideout", "file1", "file2"};
    TC_DPIA(5, argsin11, args, false);
    assert(args.dir_flag);

    char *argsin12[] = {"prog12.real", "-x", "overrideout", "-o", ".pp", "file1", "file2"};
    TC_DPIA(7, argsin12, args, false);
    assert(args.inp_ext_flag);
    assert(args.out_ext_flag);
    assert(strcmp(args.inp_ext, "overrideout") == 0);
    assert(strcmp(args.out_ext, ".pp") == 0);

    char *argsin13[] = {"prog13.real", "-x", ".input", "-o", "output", "file1", "file2"};
    TC_DPIA(7, argsin13, args, false);
    assert(args.inp_ext_flag);
    assert(args.out_ext_flag);
    assert(strcmp(args.inp_ext, ".input") == 0);
    assert(strcmp(args.out_ext, "output") == 0);

    free_input_args(&args);
    ADD_TCC;
}

#define TC_RDIR(numargs, argsin, argsout, expect_ret, numfiles) do { \
    TC_DPIA(numargs, argsin, argsout, true); \
    assert(expect_ret == read_dir_filenames(&argsout)); \
    assert(strcmp(argsout.directory, argsin[2]) == 0); \
    assert(argsout.num_files == numfiles); \
    assert((argsout.filenames == NULL) == (numfiles == 0)); \
} while(0)

static void test_read_dir()
{
    input_args_t args;
    init_input_args(&args);

    char *dirarg1[] = {"dir1.real", "-d", "./tests/001_inputs"};
    TC_RDIR(3, dirarg1, args, true, 13);

    // silent mode
    char *dirargs[] = {"dirs.real", "-d", "./tests/001_bench_pointer", "-s", "-x", ".txt"};
    TC_RDIR(6, dirargs, args, true, 13);

    char *dirarg2[] = {"dir2.real", "-d", "./tests/001_inputs/"};
    TC_RDIR(3, dirarg2, args, true, 13);

    char *dirarg3[] = {"dir3.real", "-d", "./tests/001_inputs.notadir"};
    TC_RDIR(3, dirarg3, args, false, 0);

    char *dirarg4[] = {"dir4.real", "-d", "./tests/001_inputs/acan_oisd_ABP.fat"};
    TC_RDIR(3, dirarg4, args, false, 0);

    char *dirarg5[] = {"dir5.real", "-d", "./tests/001_inputs/", "-x", ".jmm"};
    TC_RDIR(5, dirarg5, args, true, 0);

    char *dirarg6[] = {"dir6.real", "-d", "./tests/001_inputs/", "-x", ".bigin", "-o", ".fat"};
    TC_RDIR(7, dirarg6, args, true, 4);

    char *dirarg7[] = {"dir7.real", "-d", "./tests/001_inputs/", "-x", ".tea"};
    TC_RDIR(5, dirarg7, args, true, 1);

    free_input_args(&args);
    ADD_TCC;
}

static void test_duped_args()
{
    input_args_t args;
    init_input_args(&args);

    char *arg1[] = {"duped1.real", "-d", "./tests/001_inputs", "-i", "100", "-r", "500", "-x", ".xyz", "-i", "10"};
    TC_DPIA(11, arg1, args, false);

    char *arg2[] = {"duped2.real", "-d", "./tests/001_inputs", "-r", "100", "-i", "500", "-r", "20", "-o", ".wat"};
    TC_DPIA(11, arg2, args, false);

    char *arg3[] = {"duped3.real", "-d", "./tests/001_inputs", "-$", "-i", "500", "-r", "20", "-o", ".wat"};
    TC_DPIA(11, arg3, args, false);

    free_input_args(&args);
    ADD_TCC;
}

static void test_log_args()
{
    input_args_t args;
    init_input_args(&args);

    char *arg1[] = {"dlog1.real", "-d", "./tests/001_inputs", "-L", "./tests/tmp.log"};
    TC_DPIA(5, arg1, args, true);
    assert(args.log_flag);
    assert(strcmp(args.log_fname, "./tests/tmp.log") == 0);

    char *arg2[] = {"dlog2.real", "-d", "./tests/001_inputs", "-L", "./tests/"};
    TC_DPIA(5, arg2, args, false);
    assert(args.log_flag);
    assert(strcmp(args.log_fname, "./tests/") == 0);

    free_input_args(&args);
    ADD_TCC;
}

#define TC_RFILE(numargs, argsin, argsout, expect_ret, numfiles) do { \
    TC_DPIA(numargs, argsin, argsout, true); \
    assert(expect_ret == read_argv_filenames(numargs, argsin, &argsout)); \
    if(expect_ret) { \
        assert(argsout.num_files == numfiles); \
        assert(argsout.filenames); \
    } \
} while(0)
static void test_read_argvfiles()
{
    input_args_t args;
    init_input_args(&args);

    char *arg1[] = {"files1.real", "./tests/001_inputs/EasyList_Chinese.fat"};
    TC_RFILE(2, arg1, args, true, 1);
    assert(strcmp(args.filenames[0], "./tests/001_inputs/EasyList_Chinese.fat") == 0);

    char *arg2[] = {"files2.real", "./tests/001_inputs/NotHere.fat"};
    TC_RFILE(2, arg2, args, false, 0);

    char *arg3[] = {"files3.real", "./tests/001_inputs/"};
    TC_RFILE(2, arg3, args, false, 1);

    char *arg4[] = {"files4.real", "./tests/001_inputs/EasyList_Chinese.fat", "./tests/001_inputs/EasyList_Chinese.txt"};
    TC_RFILE(3, arg4, args, true, 2);
    assert(strcmp(args.filenames[0], "./tests/001_inputs/EasyList_Chinese.fat") == 0);
    assert(strcmp(args.filenames[1], "./tests/001_inputs/EasyList_Chinese.txt") == 0);

    free_input_args(&args);
    ADD_TCC;
}

#define TC_PI(numargs, argsin, argsout, expect_ret) do { \
    free_input_args(&argsout); \
    init_input_args(&argsout); \
    optind = 1; \
    fprintf(stdout, "Parse %d args for prog: %s\n", numargs, argsin[0]); \
    assert(expect_ret == parse_input_args(numargs, argsin, &argsout)); \
} while(0)

static void test_parse_input()
{
    input_args_t args;
    init_input_args(&args);

    char *arg1[] = {"files1.real", "./tests/001_inputs/EasyList_Chinese.fat"};
    TC_PI(2, arg1, args, true);

    char *arg2[] = {"files2.real", "-d", "./tests/001_inputs/"};
    TC_PI(3, arg2, args, true);

    char *arg3[] = {"files3.real", "-d", "./tests/001_inputs/", "-r", "10", "-o", "notext"};
    TC_PI(7, arg3, args, false);

    free_input_args(&args);
    ADD_TCC;
}

static void test_errLog()
{
    // static initialized global variable
    assert(!global_errLog);
    assert(get_globalErrLog() == stderr);

    input_args_t args;
    init_input_args(&args);

    // not affected by input_args_t
    assert(!global_errLog);
    assert(get_globalErrLog() == stderr);

    char *arg1[] = {"files1.real", "-E", "./tests/test_errout.log", "./tests/001_inputs/EasyList_Chinese.fat"};
    TC_PI(4, arg1, args, true);

    assert(args.errLog_flag);
    assert(args.errLog_fname);
    assert(0 == strcmp(args.errLog_fname, "./tests/test_errout.log"));

    free_input_args(&args);

    // flags reset after free
    assert(!args.errLog_flag);
    assert(!args.errLog_fname);

    // if errLog_flag was set, this is initialized non-nil.
    assert(global_errLog);
    assert(0 == strcmp(global_errLog->log_fname, "./tests/test_errout.log"));
    assert(!global_errLog->file);
    assert(get_globalErrLog() == stderr);

    open_globalErrLog();

    assert(0 == strcmp(global_errLog->log_fname, "./tests/test_errout.log"));
    assert(global_errLog->file);
    assert(global_errLog->file != stderr);

    ELOG_STDERR("TEST: this is a test\n");

    close_globalErrLog();
    assert(global_errLog);
    assert(!global_errLog->file);
    assert(get_globalErrLog() == stderr);

    free_input_args(&args);
    free(global_errLog);
    ADD_TCC;
}

void test_input_args()
{
    printf("Running tests for input argument parsing..\n");
    test_init_input_args();
    test_free_input_args();
    test_silent_mode();
    test_do_parse_input_args();
    test_read_dir();
    test_duped_args();
    test_log_args();
    test_errLog();
    test_read_argvfiles();
    test_parse_input();
    printf("OK.\n");
    ADD_TCC;
}
#endif
