/**
 * csvline.h
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
#ifndef csvline_h
#define csvline_h
#include "dedupdomains.h"

typedef struct CsvLineView
{
    char const **data;
    size_len_t *lengths;
    uchar cols_used;
    uchar cols_alloc;
#ifdef COLLECT_DIAGNOSTICS
    size_len_t count_reallocs;
    size_len_t max_used;
#endif
} CsvLineView_t;

/**
 * A view into a string that is allocated elsewhere. To avoid a copy. Temporary
 * view. May not be null terminated.
 */
typedef struct CsvColView
{
    char const *data;
    size_len_t len;
    size_len_t idx;
} CsvColView_t;

extern bool update_CsvLineView(CsvLineView_t *lv, char const *input_line);
extern bool null_CsvLineView(CsvLineView_t const *lv);
extern void init_CsvLineView(CsvLineView_t *lv);
extern void free_CsvLineView(CsvLineView_t *lv);

extern CsvColView_t get_CsvColView(CsvLineView_t const *lv, size_len_t idx);
extern void print_CsvLineView(CsvLineView_t const *const lv);

#endif
