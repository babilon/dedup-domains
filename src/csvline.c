/**
 * csvline.c
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
#include <stdio.h>
#include "csvline.h"

static const char CSV_DELIMITER = ',';
static const char CSV_INIT_ALLOC_COLS = 7;

/**
 * Returns a CsvColView for the indicated column index in the given
 * CsvLineView. The internals point to within the given CsvLineView.
 *
 * Returned value is valid until the given CsvLineView is modified by
 * init_CsvLineView(), free_CsvLineView(), or update_CsvLineView().
 */
CsvColView_t get_CsvColView(CsvLineView_t const *lv, size_len_t idx)
{
    CsvColView_t cv;

    ASSERT(lv);

    cv.idx = idx;
    cv.data = NULL;
    cv.len = lv->lengths[idx];

    if(cv.len > 0)
    {
        ADD_CC;
        cv.data = lv->data[idx];
    }

    ADD_CC;
    return cv;
}

/**
 * Returns true if the given CsvLineView is null. Otherwise returns false.
 */
bool null_CsvLineView(CsvLineView_t const *lv)
{
    ASSERT(lv);

    ADD_CC;
    return !lv->data;
}

/**
 * Releases the allocated memory held by the given CsvLineView and sets the
 * entire struct to zero.
 */
void free_CsvLineView(CsvLineView_t *lv)
{
    ASSERT(lv);

    free(lv->data);
    free(lv->lengths);
#ifdef COLLECT_DIAGNOSTICS
    if(lv->count_reallocs)
    {
        LOG_DIAG("CsvLineView", lv, "guts realloc'ed %lu times to final size of %lu with %lu used.\n\n",
                (size_t)lv->count_reallocs, (size_t)lv->cols_alloc, (size_t)lv->max_used);
    }
    else if(!lv->lengths)
    {
        // not an interesting statistic
    }
    else
    {
        LOG_DIAG("CsvLineView", lv, "guts were never realloc'ed beyond initial size of %lu with %lu used.\n\n",
                (size_t)lv->cols_alloc, (size_t)lv->max_used);
    }
#endif
#ifdef USE_MEMSET
    memset(lv, 0, sizeof(CsvLineView_t));
#else
    lv->data = NULL;
    lv->lengths = 0;
    lv->cols_alloc = 0;
    lv->cols_used = 0;
#ifdef COLLECT_DIAGNOSTICS
    lv->count_reallocs = 0;
    lv->max_used = 0;
#endif
#endif
    ADD_CC;
}

/**
 * Initializes a CsvLineView to be suitable for use by update_CsvLineView().
 * After calling this, null_CsvLineView() will return false and
 * free_CsvLineView() must be called with the instance to release memory.
 */
void init_CsvLineView(CsvLineView_t *lv)
{
    ASSERT(lv);

#ifdef USE_MEMSET
    memset(lv, 0, sizeof(CsvLineView_t));
#else
    lv->cols_used = 0;
#ifdef COLLECT_DIAGNOSTICS
    lv->max_used = 0;
    lv->count_reallocs = 0;
#endif
#endif

    lv->cols_alloc = CSV_INIT_ALLOC_COLS;

    // allocate enough for as many columns as may exist
    // update output with accurate number of columns in array
    lv->data = malloc(sizeof(char*) * lv->cols_alloc);
    if(!lv->data)
    {
        exit(EXIT_FAILURE);
    }

    lv->lengths = malloc(sizeof(size_len_t) * lv->cols_alloc);
    if(!lv->lengths)
    {
        exit(EXIT_FAILURE);
    }
    ADD_CC;
}

/**
 * Internal function.
 *
 * After a CsvLineView has been initialized, this call will add the given string
 * represented by 'begin' and 'end' to the given CsvLineView.
 */
static void set_CsvLineView(CsvLineView_t *const lv, char const *begin, char const *end)
{
    ASSERT(lv);
    ASSERT(begin);
    ASSERT(end);

    if(lv->cols_used >= lv->cols_alloc)
    {
        lv->cols_alloc++;
        lv->cols_alloc++;

        const char **tmp = realloc(lv->data, sizeof(char*) * lv->cols_alloc);
        if(tmp)
        {
            lv->data = tmp;
            tmp = NULL;
            ADD_CC;
        }
        else
        {
            free(lv->data);
            exit(EXIT_FAILURE);
        }

        size_len_t *tmpS = realloc(lv->lengths, sizeof(size_len_t) * lv->cols_alloc);
        if(tmpS)
        {
            lv->lengths = tmpS;
            tmp = NULL;
            ADD_CC;
        }
        else
        {
            free(lv->lengths);
            exit(EXIT_FAILURE);
        }
#ifdef COLLECT_DIAGNOSTICS
        lv->count_reallocs++;
#endif
        ADD_CC;
    }

    lv->data[lv->cols_used] = begin;
    lv->lengths[lv->cols_used] = end - begin;
    lv->cols_used++;
#ifdef COLLECT_DIAGNOSTICS
    if(lv->cols_used > lv->max_used)
        lv->max_used = lv->cols_used;
#endif

    ADD_CC;
}

/**
 * Parses a given null-terminated string and modifies a given initialized
 * CsvLineView accordingly. The input string must remain valid and unmodified
 * for the lifetime of the given CsvLineView or until this is called again with
 * a different null-terminated string.
 *
 * After this call, all existing CsvColView's are invalid and must not be used
 * other than to discard, e.g., memset(cv, 0, sizeof(CsvLineView)); is safe.
 */
bool update_CsvLineView(CsvLineView_t *lv, char const *input_line)
{
    char const *c, *prev;
    ASSERT(lv);

    if(input_line == NULL || *input_line == '\0')
    {
        ADD_CC;
        return false;
    }

    lv->cols_used = 0;

    prev = c = input_line;

    while(*c)
    {
        if( *c == CSV_DELIMITER )
        {
            set_CsvLineView(lv, prev, c);

            c++;
            prev = c;
            ADD_CC;
        }
        else
        {
            ADD_CC;
            c++;
        }
        ADD_CC;
    }

    set_CsvLineView(lv, prev, c);

    ADD_CC;
    return true;
}

#ifdef BUILD_TESTS
/**
 * For unit tests only.
 */
static CsvLineView_t parse_CsvLine(char const *input_line)
{
    CsvLineView_t lv;

    init_CsvLineView(&lv);

    update_CsvLineView(&lv, input_line);

    ADD_CC;
    return lv;
}

void print_CsvLineView(CsvLineView_t const *const lv)
{
    if( null_CsvLineView(lv) )
    {
        ELOG_STDERR("WARNING: CsvLineView is null in %s\n", __FUNCTION__);
        return;
    }

    printf("cols used: %lu\n", (size_t)lv->cols_used);
    printf("cols alloc: %lu\n", (size_t)lv->cols_alloc);
    for(size_t i = 0; i < lv->cols_used; i++)
    {
        printf("lengths[i=%lu]=%lu\n", i, (size_t)lv->lengths[i]);
        printf("the data: '%.*s'\n", (int)lv->lengths[i], lv->data[i]);
    }
}

static void test_null_CsvLine()
{
    CsvLineView_t lv;
    memset(&lv, 1, sizeof(CsvLineView_t));

    assert(!null_CsvLineView(&lv));

    memset(&lv, 0, sizeof(CsvLineView_t));

    assert(null_CsvLineView(&lv));

    init_CsvLineView(&lv);

    assert(!null_CsvLineView(&lv));

    free_CsvLineView(&lv);

    assert(null_CsvLineView(&lv));
}

static void test_free_CsvLine()
{
    CsvLineView_t lv, lv_zero;
    memset(&lv, 0, sizeof(CsvLineView_t));
    memset(&lv_zero, 0, sizeof(CsvLineView_t));

    // free on a nil pointer is legal
    free_CsvLineView(&lv);

    // free didn't change the struct
    assert(!memcmp(&lv_zero, &lv, sizeof(CsvLineView_t)));
}

static void test_init_CsvLine()
{
    CsvLineView_t lv, lv_zero;
    memset(&lv, 0xf, sizeof(CsvLineView_t));
    memset(&lv_zero, 0, sizeof(CsvLineView_t));

    init_CsvLineView(&lv);

    assert(lv.cols_used == 0);
    // at least one 'block' of memory allocated to guarantee malloc returns a
    // non-nil pointer to valid memory. malloc of size zero is undefined on some
    // systems. the initialize is intended to prepare the struct for usage and
    // requires a free_CsvLineView() call afterwards.
    assert(CSV_INIT_ALLOC_COLS > 0);
    assert(lv.cols_alloc == CSV_INIT_ALLOC_COLS);
    assert(lv.data);
    assert(lv.lengths);

    // assign non-zero value to emulate there is data in an allocated cell.
    lv.cols_used = 1;

    free_CsvLineView(&lv);

    assert(lv.cols_used == 0);
    assert(lv.cols_alloc == 0);
    assert(!lv.data);
    assert(!lv.lengths);

    assert(null_CsvLineView(&lv));

#ifdef USE_MEMSET
    assert(!memcmp(&lv_zero, &lv, sizeof(CsvLineView_t)));
#endif
}

#define ASSERT_COL_VALUE(IDX, VALUE) \
    expected = VALUE; \
    assert(lv.lengths[IDX] == strlen(expected)); \
    assert(!memcmp(lv.data[IDX], expected, strlen(expected)))

static void test_parse_CsvLine()
{
    CsvLineView_t lv, lv_zero;
    memset(&lv_zero, 0, sizeof(CsvLineView_t));

    char const *expected = NULL;
    lv = parse_CsvLine(",www.000free.us,,0,ccan_StevenBlack_hosts,DNSBL_Compilation,0");

    assert(lv.cols_used == 7);
    assert(lv.cols_alloc >= 7);
    //assert(lv.linenumber == 20);

    assert(lv.lengths);
    assert(lv.data);

    assert(lv.lengths[0] == 0);
    assert(lv.lengths[2] == 0);

    ASSERT_COL_VALUE(1, "www.000free.us");
    ASSERT_COL_VALUE(3, "0");
    ASSERT_COL_VALUE(4, "ccan_StevenBlack_hosts");
    ASSERT_COL_VALUE(5, "DNSBL_Compilation");
    ASSERT_COL_VALUE(6, "0");

    free_CsvLineView(&lv);
}

static void test_update_CsvLine()
{
    CsvLineView_t lv;

    init_CsvLineView(&lv);

    const char *input_1 = ",www.000free.us,,0,ccan_StevenBlack_hosts,DNSBL_Compilation";
    const char *input_2 = "Col A,Col B,Col C";
    const char *input_3 = "Col 0,Col 1,Col 2,Col 3,Col 4,Col 5,Col 6,Col 7,Col 8,Col 9";
    const char *input_nil = NULL;
    const char *input_empty = "";
    const char *expected;

    assert(!update_CsvLineView(&lv, input_empty));
    assert(!update_CsvLineView(&lv, input_nil));

    assert(update_CsvLineView(&lv, input_1));

    assert(lv.cols_used == 6);
    assert(lv.cols_alloc >= 6);
    assert(lv.data);
    assert(lv.lengths);

    assert(update_CsvLineView(&lv, input_2));

    assert(lv.cols_used == 3);
    // fewer used columns does not realloc to a smaller number
    assert(lv.cols_alloc >= 6);
    assert(lv.data);
    assert(lv.lengths);

    ASSERT_COL_VALUE(0, "Col A");
    ASSERT_COL_VALUE(1, "Col B");
    ASSERT_COL_VALUE(2, "Col C");


    assert(update_CsvLineView(&lv, input_3));

    assert(lv.cols_used == 10);
    // fewer used columns does not realloc to a smaller number
    assert(lv.cols_alloc >= 10);
    assert(lv.data);
    assert(lv.lengths);

    ASSERT_COL_VALUE(0, "Col 0");
    ASSERT_COL_VALUE(1, "Col 1");
    ASSERT_COL_VALUE(2, "Col 2");
    ASSERT_COL_VALUE(3, "Col 3");
    ASSERT_COL_VALUE(4, "Col 4");
    ASSERT_COL_VALUE(5, "Col 5");
    ASSERT_COL_VALUE(6, "Col 6");
    ASSERT_COL_VALUE(7, "Col 7");
    ASSERT_COL_VALUE(8, "Col 8");
    ASSERT_COL_VALUE(9, "Col 9");

    free_CsvLineView(&lv);
}

#undef ASSERT_COL_VALUE

void test_get_CsvColView()
{

#define ASSERT_COL_VIEW(IDX, VALUE) \
    cv = get_CsvColView(&lv, (IDX)); \
    expected = VALUE; \
    assert(cv.idx == (IDX)); \
    assert(cv.len == strlen(expected)); \
    assert(!memcmp(cv.data, expected, strlen(expected)))

    CsvLineView_t lv;
    CsvColView_t cv;
    const char *expected = NULL;

    lv = parse_CsvLine("Col 0,Col 1,Col 2,Col 3,Col 4,Col 5,Col 6,Col 7,Col 8,Col 9,");

    ASSERT_COL_VIEW(3, "Col 3");

    ASSERT_COL_VIEW(0, "Col 0");
    ASSERT_COL_VIEW(9, "Col 9");

    // an empty column can be retrieved; the data is nil.
    cv = get_CsvColView(&lv, 10);
    assert(cv.len == 0);
    assert(cv.data == NULL);
    assert(cv.idx == 10);

    free_CsvLineView(&lv);

#undef ASSERT_COL_VIEW
}

void info_csvline()
{
    printf("Sizeof CsvLineView: %lu\n", sizeof(CsvLineView_t));
    printf("Sizeof CsvColView: %lu\n", sizeof(CsvColView_t));
}

void test_csvline()
{
    test_null_CsvLine();
    test_free_CsvLine();
    test_init_CsvLine();
    test_parse_CsvLine();
    test_update_CsvLine();
    test_get_CsvColView();
}
#endif
