/**
 * domaintree.c
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
#include "domaintree.h"
#include "domaininfo.h"
#include "domain.h"

/**
 * Create and initialize a DomainInfo_t. Returned pointer must be freed with
 * free_DomainInfo().
 */
static DomainInfo_t* init_DomainInfo()
{
    DomainInfo_t *di = malloc(sizeof(DomainInfo_t));

#ifdef USE_MEMSET
    memset(di, 0, sizeof(DomainInfo_t));
#else
    di->context = NULL;
#ifdef BUILD_TESTS
    di->fqd = NULL;
#endif
    di->len = 0;
    di->match_strength = MATCH_WEAK;
#endif

    // all domains are alive until regex trim is introduced.
    di->alive = true;

    ADD_CC;
    return di;
}

void free_DomainInfo(DomainInfo_t **di)
{
    if(*di)
    {
#if defined(BUILD_TESTS) || defined(REGEX_ENABLED)
        free((*di)->fqd);
#endif
        ADD_CC;
    }
    free(*di);

    *di = NULL;
    ADD_CC;
}

/**
 * Return a malloc'ed DomainInfo_t that contains a copy of the data from the
 * given DomainView_t. The returned pointer must be freed with
 * free_DomainInfo().
 */
DomainInfo_t *convert_DomainInfo(DomainView_t *dv)
{
    DomainInfo_t *di = init_DomainInfo();

#if defined(BUILD_TESTS) || defined(REGEX_ENABLED)
    // fqd is necessary for tests where it is used to verify behaviors
    // fqd is necessary for regex prune
    di->fqd = malloc(sizeof(char) * dv->len);
    memcpy(di->fqd, dv->fqd, dv->len);
#endif
    di->len = dv->len;
    di->match_strength = dv->match_strength;
    di->context = dv->context;
    di->linenumber = dv->linenumber;

    ADD_CC;
    return di;
}

static void free_DomainTreePtr(DomainTree_t **dt)
{
    free((*dt)->tld);
    free(*dt);
    *dt = NULL;
    ADD_TCC;
}

/**
 * This being extern allows outside sources use it in a callback to visit every
 * item in their own tree/list/struct to delete the DomainInfo held. And keeps
 * the free_DomainInfo hidden to within here.
 *
 * This being static .. keeps it local to this as it is a callback and the void*
 * is bogus to outsiders for the sake of free() calls. it's also ignored by this
 * particular callback.
 */
static void DomainInfo_deleter(DomainInfo_t **di, void *ignored)
{
    UNUSED(ignored);
    free_DomainInfo(di);
    ASSERT(!*di);
    ADD_TCC;
}

/**
 * Visits every leaf of the tree depth first and calls the given collector
 * passing the DomainInfo of that leaf along with the given context. Then frees
 * the internal structures. The collector owns all DomainInfo instances once
 * held by the tree.
 *
 * The DomainTree is free'd after this operation and the given root is NIL.
 */
void transfer_DomainInfo(DomainTree_t **root,
        void(*collector)(DomainInfo_t **di, void *context), void *context)
{
    ASSERT(root);
    if(*root == NULL)
    {
        ADD_CC;
        return;
    }

    DomainTree_t *current = NULL, *tmp = NULL;
    HASH_ITER(hh, *root, current, tmp)
    {
        // must visit each child
        transfer_DomainInfo(&(*root)->child, collector, context);

        HASH_DEL(*root, current);

        // XXX currently all DomainInfo_t remain alive. with regex, some may be
        // marked dead. this may not be free'ing those DomainInfo.
        // regex might prune for each context. if every FILE is handled
        // separately and all domains are already consolidated into the main
        // array, then regex pruning can delete or kill (alive=false) what is
        // not useful.
        if(current->di/* && current->di->alive*/)
        {
            ASSERT(current->di->alive);
            ASSERT(current->di->linenumber != 0);
            collector(&(current->di), context);
            ASSERT(!current->di);
        }
        ASSERT(!current->di);
        free_DomainTreePtr(&current);
        ADD_CC;
    }
    ADD_CC;

    *root = NULL;
}

/**
 * Delete the tree starting at the given root.
 *
 *          .google.com : nil-di
 *          ^ free all items below this
 *       abc.google.com : nil-di
 *       ^
 * blarg.abc.google.com : di
 * clobr.abc.google.com : di
 * ^
 *       www.google.com : nil-di
 *       ^
 * blarg.www.google.com : di
 * ^
 */
void free_DomainTree(DomainTree_t **root)
{
    transfer_DomainInfo(root, DomainInfo_deleter, NULL);
    ASSERT(!*root);
    ADD_CC;
}

/**
 * Replace the given DomainTree's DomainInfo with one described by the given
 * DomainView. Creates a new DomainInfo from the given DomainView and assigns to
 * the given entry. The old DomainInfo is freed before overwriting. No
 * modifications are done to the DomainTree's structure.
 */
static void replace_DomainInfo(DomainTree_t *entry, DomainView_t *dv)
{
    free_DomainInfo(&entry->di);
    entry->di = convert_DomainInfo(dv);
    ADD_CC;
}

static DomainTree_t *init_DomainTree(DomainTree_t *parent,
        SubdomainView_t const *sdv)
{
    // the DomainTree_t instance must be memset since it is inserted into a
    // UT_hash. alternate options available such as defining a hash function and
    // comparison. default requires all padding be zero'ed.
    DomainTree_t *ndt = calloc(1, sizeof(DomainTree_t));

    ndt->len = sdv->len;
    ndt->parent = parent;

    ndt->tld = malloc(sizeof(char) * sdv->len);
    memcpy(ndt->tld, sdv->data, sdv->len);

    ADD_CC;
    return ndt;
}

/**
 * Internal to the construction of the tree. though it could be used outside to
 * initialize the tree.
 */
static DomainTree_t* ctor_DomainTree(DomainTree_t **dt, DomainViewIter_t *it,
        SubdomainView_t *sdv)
{
    ASSERT(dt);
    ASSERT(it);
    ASSERT(sdv);

    DomainTree_t *ndt = NULL, *parent = DT_PARENT(*dt);

    // (0) enters with 'com' or 'net' or 'org' a TLD
    do
    {
        // (0) enters with 'com' or 'net' or 'org' a TLD
        // (1) create entry for 'google'
        // (2) create entry for 'www'
        ndt = init_DomainTree(parent, sdv);
        ASSERT(ndt);

        // (0) NULL dt is OK it means the HASH table is empty
        // (0) add 'com' to the HASH table next to 'org', 'net', etc.
        // (1) add 'google' to NEW HASH table [(0) 'dt->child']
        // (2) add 'www' to NEW HASH table [(1) 'dt->child']
#ifndef RELEASE
        DEBUG_MODE {
            //for debug/testing phase
            DomainTree_t *unexpected_existing = NULL;
            HASH_FIND(hh, *dt, ndt->tld, ndt->len, unexpected_existing);
            assert(!unexpected_existing);
        }
#endif

        HASH_ADD_KEYPTR(hh, *dt, ndt->tld, ndt->len, ndt);

#ifndef RELEASE
        DEBUG_MODE {
            unsigned cnt = HASH_COUNT(*dt);
            printf("\tafter hash add, dt has %lu %.*s\n", (size_t)cnt, (int)(*dt)->len, (*dt)->tld);
            DomainTree_t *rt = *dt, *t = NULL, *tmp;
            HASH_ITER(hh, rt, t, tmp) {
                printf("t %.*s\n", (int)t->len, t->tld);
            }
        }
#endif
        // dt will be the next HASH table to insert items into. it is OK to be
        // NULL. it means empty HASH.
        // (0) dt will be NULL; this is a new entry for subdomains of 'com'
        // (1) dt will be NULL; this is a new entry for subdomains of
        // 'google.com'
        // (2) dt will be NULL; this is new entry for subdomains of 'www'
        dt = &(ndt->child);
        // (0) parent will be 'com'
        // (1) parent will be 'google'
        // (2) parent will be 'www'
        parent = ndt;

        ADD_CC;
    // (1) set sdv to 'google' in 'www.google.com'
    // (2) set sdv to 'www' in 'www.google.com'
    // (3) set sdv to <NULL>; end of iteration
    } while(next_DomainView(it, sdv));

    // (3) sdv is NULL; prev_sdv is 'www'
    // need to set the DomainInfo for 'www' held at ndt
    ASSERT(ndt);
    ndt->di = convert_DomainInfo(it->dv);

    ADD_CC;
    return ndt;
}

static bool leaf_DomainTree(DomainTree_t const *dt)
{
    ASSERT(dt);
    // HASH_CNT(<hh>, <obj>)
    // even if 'child' is NULL, HASH_COUNT has defined behavior and returns 0.
    ADD_CC;
    return HASH_COUNT(dt->child) == 0;
}

static DomainTree_t* insert_Domain(DomainTree_t **dt, DomainViewIter_t *it, SubdomainView_t *sdv)
{
    ASSERT(dt);
    ASSERT(sdv);
    ASSERT(sdv->data);
    ASSERT(sdv->len > 0);

    DomainTree_t *entry = NULL;
    HASH_FIND(hh, *dt, sdv->data, sdv->len, entry);
    if(entry)
    {
        // e.g., found a FQD with a DomainInfo
        // google.com
        // ^
        if(leaf_DomainTree(entry))
        {
            // is the existing leaf strong? if so, abort adding the requested
            // b/c it's the same strength or weaker and in both cases not
            // beneficial to the tree.
            if(entry->di->match_strength == MATCH_FULL)
            {
#ifndef RELEASE
                DEBUG_MODE {
                    // item is weaker; no insertion.
                    printf("[%s:%d] %s item is weaker; skip insert.\n", __FILE__, __LINE__, __FUNCTION__);
                    printf("\tsdv=%.*s\n", (int)sdv->len, sdv->data);
#ifdef BUILD_TESTS
                    printf("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
                    printf("\texisting match:%d\n", entry->di->match_strength);
                }
#endif
                ADD_CC;
                return NULL;
            }
            // else compare FQD for equality
            // entry: 'www.google.com' 0
            // it-dv:     'google.com' 1
            // 
            // entry: 'www.google.com' 0
            // it-dv: 'abc.google.com' 1
            //
            // entry: 'www.google.com' 0
            // it-dv: 'blarg.www.google.com' 1
            //
            // entry: 'www.google.com' 0
            // it-dv: 'www.google.com' 1
            if(entry->di->len == it->dv->len)
            {
#ifdef BUILD_TESTS
                // when the length is the same, the domain is equal b/c every
                // step to this point has found an entry in the next hash table.
                ASSERT(!memcmp(entry->di->fqd, it->dv->fqd, entry->di->len));
#endif
                if(it->dv->match_strength > entry->di->match_strength)
                {
                    replace_DomainInfo(entry, it->dv);
                    // expect zero children to delete at this level
                    ASSERT(!entry->child);
                    if(entry->child)
                    {
                        fprintf(stderr, "ALERT: unexpected child at %s:%d\n", __FILE__, __LINE__);
                        free_DomainTree(&entry->child);
                    }
#ifndef RELEASE
                    DEBUG_MODE {
                        printf("[%s:%d] %s replace existing entry with stronger match; inserted.\n", __FILE__, __LINE__, __FUNCTION__);
                        printf("\tsdv=%.*s\n", (int)sdv->len, sdv->data);
                        printf("\ttld=%.*s\n", (int)entry->len, entry->tld);
#ifdef BUILD_TESTS
                        printf("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
                    }
#endif
                    ADD_CC;
                    return entry;
                }
                else // not strong enough to override
                {
#ifndef RELEASE
                    DEBUG_MODE {
                        printf("[%s:%d] %s identical; skip insert.\n", __FILE__, __LINE__, __FUNCTION__);
                        printf("\tsdv=%.*s\n", (int)sdv->len, sdv->data);
                        printf("\ttld=%.*s\n", (int)entry->len, entry->tld);
#ifdef BUILD_TESTS
                        printf("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
                    }
#endif
                    ADD_CC;
                    return NULL;
                }
            }
            else
            {
                // the length check is necessary. it would be a case where
                // everything above the current label matches and the proposed
                // has some extra bit of information that makes it special.
#ifndef RELEASE
                DEBUG_MODE {
                    printf("di len=%lu\n", (size_t)entry->di->len);
                    printf("dv len=%lu\n", (size_t)it->dv->len);
                }
#endif
                ASSERT(entry->di->len < it->dv->len);
                ADD_CC;
            }
            //else differs; fall through to go deeper
            ADD_CC;
        }

        // else not a leaf; go deeper.
        if(next_DomainView(it, sdv))
        {
            ADD_CC;
            return insert_Domain(&(entry->child), it, sdv);
        }
        //else
        //{
        // at the end of the SubdomainView .. there are no more segments to
        // inspect. must consider to insert this sdv or drop it.
        if(entry->di == NULL)
        {
            if(it->dv->match_strength == MATCH_FULL)
            {
                // clear possible children
                free_DomainTree(&entry->child);
                ADD_CC;
            }
            // create entry for this DomainInfo.
            entry->di = convert_DomainInfo(it->dv);
#ifndef RELEASE
            DEBUG_MODE {
                printf("[%s:%d] %s next_tld is nil; cleared children and inserted.\n", __FILE__, __LINE__, __FUNCTION__);
#ifdef BUILD_TESTS
                printf("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
            }
#endif
            ADD_CC;
            return entry;
        }
        else // existing di; overwrite it.
        {
            if(entry->di->match_strength < it->dv->match_strength)
            {
                // clear possible children
                free_DomainTree(&entry->child);
                replace_DomainInfo(entry, it->dv);
#ifndef RELEASE
                DEBUG_MODE {
                    printf("[%s:%d] %s next_tld is nil; replace it, clear children and insert.\n", __FILE__, __LINE__, __FUNCTION__);
#ifdef BUILD_TESTS
                    printf("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
                }
#endif
                ADD_CC;
                return entry;
            }
            ADD_CC;
        }
        ADD_CC;
        //}
    }
    else // entry is NULL; no match found for 'com' in 'www.google.com'
    {
        // sdv is valid b/c next_tld is true
        // use 'entry' here since it's local, unused and NULL.
        // was 'ntd' to mean next top level domain
        ASSERT(!entry);
        entry = ctor_DomainTree(dt, it, sdv);
#ifndef RELEASE
        DEBUG_MODE {
            printf("[%s:%d] %s build tree; inserted. dt=%p ndt=%p\n", __FILE__, __LINE__, __FUNCTION__, *dt, entry);
            printf("\tsdv=%.*s\n", (int)sdv->len, sdv->data);
#ifdef BUILD_TESTS
            printf("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
        }
#endif
        ADD_CC;
        return entry;
    }

    ADD_CC;
    return NULL;
}

DomainTree_t* insert_DomainTree(DomainTree_t **dt, DomainView_t *dv)
{
    ASSERT(dt);
    ASSERT(dv);

    // mandate the match strength be set before inserting to communicate that
    // the tree insertion REQUIRES knowing this information or else it'll be a
    // bad day in hell to know why the matches are bogused. the insertion
    // evaluates this information.
    // should be impossible to get to without a programmer introduced error.
    if(dv->match_strength == MATCH_NOTSET)
    {
        fprintf(stderr, "ERROR: DomainView has uninitialized match_strength set; skip insertion.\n");
        ADD_CC;
        return NULL;
    }

    if(dv->match_strength == MATCH_BOGUS)
    {
        fprintf(stderr, "ALERT: DomainView has bogus match_strength set; skip insertion.\n");
        ADD_CC;
        return NULL;
    }

    DomainTree_t *blarg = NULL;
    DomainViewIter_t it = begin_DomainView(dv);

    SubdomainView_t sdv;
    if(next_DomainView(&it, &sdv))
    {
        blarg = insert_Domain(dt, &it, &sdv);
        ADD_CC;
    }

    ADD_CC;
    return blarg;
}

static void do_visit_DomainTree(DomainTree_t *root,
        void(*visitor_func)(DomainInfo_t const *di, void *context),
        void *context)
{
    ASSERT(visitor_func);

    if(root == NULL)
    {
        ADD_CC;
        return;
    }

    DomainTree_t *dt, *tmp;

    HASH_ITER(hh, root, dt, tmp)
    {
        // must visit each child
        do_visit_DomainTree(dt->child, visitor_func, context);

        if(dt->di && dt->di->alive)
        {
#ifdef BUILD_TESTS
            printf("DT: Visited %.*s\n", (int)dt->di->len, dt->di->fqd);
#endif
            (*visitor_func)(dt->di, context);
            ADD_CC;
        }
        ADD_CC;
    }

    ADD_CC;
}

void visit_DomainTree(DomainTree_t *root,
        void(*visitor_func)(DomainInfo_t const *di, void *context),
        void *context)
{
    ASSERT(visitor_func);

    do_visit_DomainTree(root, visitor_func, context);
    ADD_CC;
}

#ifdef BUILD_TESTS
static void print_di(DomainInfo_t const *di, void *context)
{
    UNUSED(context);
    printf("DT: Visited %.*s\n", (int)di->len, di->fqd);
}

/**
 * May be useful for debugging
 */
void print_DomainTree(DomainTree_t *root)
{
    visit_DomainTree(root, &print_di, NULL);
}

typedef struct TestTable
{
    char *str;
    UT_hash_handle hh;
} TestTable_t;

static TestTable_t *root_visited = NULL;

static void test_visitor(DomainInfo_t const *di, void *context)
{
    UNUSED(context);
    assert(di);

    TestTable_t *entry, *tmp;

    entry = malloc(sizeof(TestTable_t));
    memset(entry, 0, sizeof(TestTable_t));

    entry->str = malloc(sizeof(char) * di->len);
    memcpy(entry->str, di->fqd, di->len);

    // should not already exist when visiting
    HASH_FIND(hh, root_visited, di->fqd, di->len, tmp);
    assert(!tmp);

    if(!tmp)
    {
        printf("didn't find %.*s\n", (int)di->len, di->fqd);
        HASH_ADD_KEYPTR(hh, root_visited, di->fqd, di->len, entry);
    }
    else
        printf("found %.*s\n", (int)di->len, di->fqd);
    assert(entry);
}

#define INSERT_DOMAIN(value, strength, nilret) \
    update_DomainView(&dv, value, strlen(value)); \
    dv.linenumber = __LINE__; \
    dv.match_strength = strength; \
    ret = insert_DomainTree(&root, &dv); \
    printf("%p\n", ret); \
    printf("%d\n", !ret); \
    assert(!ret == !(nilret))
#define FREE_VISITED \
    HASH_ITER(hh, root_visited, t, tmp) { \
        HASH_DEL(root_visited, t); \
        free(t->str); \
        free(t); \
    } \
    assert(!root_visited)

static void test_duplicates()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("abc.www.somedomain.com", 1, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    // duplicate: skipped
    INSERT_DOMAIN("abc.www.somedomain.com", 1, false);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_prune3()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("abc.www.somedomain.com", 1, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    // obliterates the other one
    INSERT_DOMAIN("somedomain.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_prune2()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("www.somedomain.com", 1, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    // obliterates the other one
    INSERT_DOMAIN("somedomain.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_weak3()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("abc.www.somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    // obliterates the other one
    INSERT_DOMAIN("somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 2);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_weak2()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("www.somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    // obliterates the other one
    INSERT_DOMAIN("somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 2);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_unique_weak()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("abc.www.somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("www.somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 2);
    FREE_VISITED;

    INSERT_DOMAIN("go.abc.www.somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    INSERT_DOMAIN("somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 4);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_unique_weak2()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("go.abc.www.somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("www.somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 2);
    FREE_VISITED;

    INSERT_DOMAIN("abc.www.somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    INSERT_DOMAIN("somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 4);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_unique_weak_strong()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("abc.www.somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("www.somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 2);
    FREE_VISITED;

    INSERT_DOMAIN("go.abc.www.somedomain.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    INSERT_DOMAIN("somedomain.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_unique_weak_to_strong()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("go.abc.www.somedomain.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("abc.www.somedomain.com", 1, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("www.somedomain.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("somedomain.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_replace_weak_w_strong()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("abc.www.weak-w-strong.com", 0, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("abc.www.weak-w-strong.com", 1, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_uninitialized()
{
    DomainTree_t *root = NULL;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);
    dv.linenumber = __LINE__;

    update_DomainView(&dv, "abc.www.strong-o-weak.com",
            strlen("abc.www.strong-o-weak.com"));
    assert(!insert_DomainTree(&root, &dv));

    dv.match_strength = MATCH_BOGUS;
    assert(!insert_DomainTree(&root, &dv));

    dv.match_strength = MATCH_FULL;
    assert(insert_DomainTree(&root, &dv));
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_strong_over_weak()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("abc.www.strong-o-weak.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("abc.www.strong-o-weak.com", 0, false);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_e2e_discovered()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    /*
     * ,notlong.com,,0,samplebug,DNSBL_Compilation,1
     * ,www.somedomain.com,,0,samplebug,DNSBL_Compilation,0
     * ,somedomain.com,,0,samplebug,DNSBL_Compilation,0
     * ,01proxy.notlong.com,,0,samplebug,DNSBL_Compilation,1
     */

    INSERT_DOMAIN("notlong.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("www.somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 2);
    FREE_VISITED;

    INSERT_DOMAIN("somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    INSERT_DOMAIN("01proxy.notlong.com", 0, false);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_insert_stronger()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    INSERT_DOMAIN("cdn.lenzmx.com", 0, true);
    INSERT_DOMAIN("lenzmx.com", 0, true);
    INSERT_DOMAIN("lenzmx.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

static void test_e2e_discovered2()
{
    DomainTree_t *root = NULL, *ret;
    DomainView_t dv;
    TestTable_t *t, *tmp;
    assert(!root_visited);

    init_DomainView(&dv);

    /*
     * ,01proxy.notlong.com,,0,samplebug,DNSBL_Compilation,1
     * ,notlong.com,,0,samplebug,DNSBL_Compilation,1
     * ,www.somedomain.com,,0,samplebug,DNSBL_Compilation,0
     * ,somedomain.com,,0,samplebug,DNSBL_Compilation,0
     */
    INSERT_DOMAIN("01proxy.notlong.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("notlong.com", 1, true);

    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 1);
    FREE_VISITED;

    INSERT_DOMAIN("www.somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 2);
    FREE_VISITED;

    INSERT_DOMAIN("somedomain.com", 0, true);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    INSERT_DOMAIN("www.somedomain.com", 0, false);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    INSERT_DOMAIN("somedomain.com", 0, false);

    // one domain. it is itself unique.
    visit_DomainTree(root, &test_visitor, NULL);
    assert(HASH_COUNT(root_visited) == 3);
    FREE_VISITED;

    free_DomainView(&dv);
    free_DomainTree(&root);
}

#undef INSERT_DOMAIN

void info_DomainTree()
{
    printf("Sizeof DomainInfo_t: %lu\n", sizeof(DomainInfo_t));
    printf("Sizeof DomainTree_t: %lu\n", sizeof(DomainTree_t));
}

void test_DomainTree()
{
    test_duplicates();
    test_prune3();
    test_weak3();
    test_prune2();
    test_weak2();
    test_unique_weak();
    test_unique_weak2();
    test_unique_weak_strong();
    test_unique_weak_to_strong();
    test_replace_weak_w_strong();
    test_strong_over_weak();
    test_uninitialized();
    test_e2e_discovered();
    test_e2e_discovered2();
    test_insert_stronger();

    ADD_TCC;
}
#endif
