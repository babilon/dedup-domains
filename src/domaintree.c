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
	di->match_strength = MATCH_NOTSET;
#ifdef BUILD_TESTS
	di->fqd = NULL;
	di->len = 0;
#endif
#endif
	return di;
}

void free_DomainInfo(DomainInfo_t **di)
{
	if(*di)
	{
#if defined(BUILD_TESTS)
		free((*di)->fqd);
#endif
	}
	free(*di);

	*di = NULL;
}

/**
 * Return a malloc'ed DomainInfo_t that contains a copy of the data from the
 * given DomainView_t. The returned pointer must be freed with
 * free_DomainInfo().
 */
DomainInfo_t *convert_DomainInfo(DomainView_t *dv)
{
	DomainInfo_t *di = init_DomainInfo();

#if defined(BUILD_TESTS)
	// fqd is necessary for tests where it is used to verify behaviors
	// fqd is necessary for regex prune
	di->fqd = malloc(sizeof(char) * dv->len);
	di->len = dv->len;
	memcpy(di->fqd, dv->fqd, dv->len);
#endif
	di->match_strength = dv->match_strength;
	di->context = dv->context;
	di->linenumber = dv->linenumber;

	return di;
}

static void free_DomainTreePtr(DomainTree_t **dt)
{
	free((*dt)->tld);
	free(*dt);
	*dt = NULL;
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
		return;
	}

	DomainTree_t *current = NULL, *tmp = NULL;
	HASH_ITER(hh, *root, current, tmp)
	{
		// must visit each child
		transfer_DomainInfo(&(*root)->child, collector, context);

		HASH_DEL(*root, current);

		if(current->di)
		{
			ASSERT(current->di->match_strength > MATCH_NOTSET);
			ASSERT(current->di->linenumber != 0);
			collector(&(current->di), context);
			ASSERT(!current->di);
		}
		ASSERT(!current->di);
		free_DomainTreePtr(&current);
	}

	*root = NULL;
}

/**
 * Delete the tree starting at the given root.
 *
 *			.google.com : nil-di
 *			^ free all items below this
 *		 abc.google.com : nil-di
 *		 ^
 * blarg.abc.google.com : di
 * clobr.abc.google.com : di
 * ^
 *		 www.google.com : nil-di
 *		 ^
 * blarg.www.google.com : di
 * ^
 */
void free_DomainTree(DomainTree_t **root)
{
	transfer_DomainInfo(root, DomainInfo_deleter, NULL);
	ASSERT(!*root);
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
}

static DomainTree_t *init_DomainTree(SubdomainView_t const *sdv)
{
	// the DomainTree_t instance must be memset since it is inserted into a
	// UT_hash. alternate options available such as defining a hash function and
	// comparison. default requires all padding be zero'ed.
	DomainTree_t *ndt = calloc(1, sizeof(DomainTree_t));

	ndt->len = sdv->len;

	ndt->tld = malloc(sizeof(char) * sdv->len);
	memcpy(ndt->tld, sdv->data, sdv->len);

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

	DomainTree_t *ndt = NULL;

	// (0) enters with 'com' or 'net' or 'org' a TLD
	do
	{
		// (0) enters with 'com' or 'net' or 'org' a TLD
		// (1) create entry for 'google'
		// (2) create entry for 'www'
		ndt = init_DomainTree(sdv);
		ASSERT(ndt);

		// (0) NULL dt is OK it means the HASH table is empty
		// (0) add 'com' to the HASH table next to 'org', 'net', etc.
		// (1) add 'google' to NEW HASH table [(0) 'dt->child']
		// (2) add 'www' to NEW HASH table [(1) 'dt->child']
#ifndef RELEASE
		//for debug/testing phase
		DomainTree_t *unexpected_existing = NULL;
		HASH_FIND(hh, *dt, ndt->tld, ndt->len, unexpected_existing);
		ASSERT(!unexpected_existing);
#endif

		HASH_ADD_KEYPTR(hh, *dt, ndt->tld, ndt->len, ndt);

#if 0
		DEBUG_PRINTF("\tafter hash add, dt has %lu %.*s\n", (size_t)HASH_COUNT(*dt), (int)(*dt)->len, (*dt)->tld);
		DomainTree_t *rt = *dt, *t = NULL, *tmp;
		HASH_ITER(hh, rt, t, tmp) {
			DEBUG_PRINTF("t %.*s\n", (int)t->len, t->tld);
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

	// (1) set sdv to 'google' in 'www.google.com'
	// (2) set sdv to 'www' in 'www.google.com'
	// (3) set sdv to <NULL>; end of iteration
	} while(next_DomainView(it, sdv));

	// (3) sdv is NULL; prev_sdv is 'www'
	// need to set the DomainInfo for 'www' held at ndt
	ASSERT(ndt);
	ASSERT(it->dv->match_strength > MATCH_NOTSET);
	ndt->di = convert_DomainInfo(it->dv);

	return ndt;
}

static DomainTree_t* replace_if_stronger(DomainTree_t *entry, DomainView_t *dv)
{
	ASSERT(entry);
	ASSERT(dv);

	ASSERT(dv->match_strength > MATCH_NOTSET);
	ASSERT(dv->match_strength != MATCH_REGEX);

	if(entry->di == NULL || dv->match_strength > entry->di->match_strength)
	{
		replace_DomainInfo(entry, dv);

		ASSERT(entry->di);
		if(entry->di->match_strength == MATCH_FULL)
		{
			free_DomainTree(&entry->child);
		}
		DEBUG_PRINTF("[%s:%d] %s replace existing entry with stronger match; inserted.\n", __FILE__, __LINE__, __FUNCTION__);
		DEBUG_PRINTF("\ttld=%.*s\n", (int)entry->len, entry->tld);
#ifdef BUILD_TESTS
		DEBUG_PRINTF("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
		return entry;
	}
	else // not strong enough to override
	{
		DEBUG_PRINTF("[%s:%d] %s identical; skip insert.\n", __FILE__, __LINE__, __FUNCTION__);
		DEBUG_PRINTF("\ttld=%.*s\n", (int)entry->len, entry->tld);
#ifdef BUILD_TESTS
		ASSERT(entry->di);
		DEBUG_PRINTF("\tfqd=%.*s\n", (int)entry->di->len, entry->di->fqd);
#endif
	}

	return NULL;
}

static bool leaf_DomainTree(DomainTree_t const *dt)
{
	ASSERT(dt);
	// HASH_CNT(<hh>, <obj>)
	// even if 'child' is NULL, HASH_COUNT has defined behavior and returns 0.
	return HASH_COUNT(dt->child) == 0;
}

static DomainTree_t* insert_Domain(DomainTree_t **root_dt, DomainView_t *dv)
{
	ASSERT(root_dt);

	DomainTree_t **dt = root_dt;
	DomainTree_t *entry = NULL;

	DomainViewIter_t it = begin_DomainView(dv);
	SubdomainView_t sdv;

	while(next_DomainView(&it, &sdv))
	{
		ASSERT(dt);
		ASSERT(sdv.data);
		ASSERT(sdv.len > 0);
		HASH_FIND(hh, *dt, sdv.data, sdv.len, entry);

		if(!entry)
		{
			entry = ctor_DomainTree(dt, &it, &sdv);
			ASSERT(entry);
			ASSERT(entry->di);
			ASSERT(entry->di->match_strength > MATCH_NOTSET);
			return entry;
		}

		if(leaf_DomainTree(entry))
		{
			ASSERT(entry->di);
			ASSERT(entry->di->match_strength > MATCH_NOTSET);
			ASSERT(entry->di->match_strength != MATCH_REGEX);
			if(entry->di->match_strength == MATCH_FULL)
			{
				return NULL;
			}
		}

		dt = &entry->child;
	}

	// if 'entry' is null, then 'dv' is garbage, i.e., not a domain.
	ASSERT(entry);
	entry = replace_if_stronger(entry, dv);
	return entry;
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
		ELOG_STDERR("ERROR: DomainView has uninitialized match_strength set; skip insertion.\n");
		return NULL;
	}

	if(dv->match_strength == MATCH_BOGUS)
	{
		ELOG_STDERR("ALERT: DomainView has bogus match_strength set; skip insertion.\n");
		return NULL;
	}

	DomainTree_t *entry = insert_Domain(dt, dv);

	return entry;
}

static void do_visit_DomainTree(DomainTree_t *root,
		void(*visitor_func)(DomainInfo_t const *di, void *context),
		void *context)
{
	ASSERT(visitor_func);

	if(root == NULL)
	{
		return;
	}

	DomainTree_t *dt, *tmp;

	HASH_ITER(hh, root, dt, tmp)
	{
		// must visit each child
		do_visit_DomainTree(dt->child, visitor_func, context);

		if(dt->di)
		{
			DEBUG_PRINTF("DT: Visited strength=%d label=%.*s\n", dt->di->match_strength, (int)dt->len, dt->tld);
#ifdef BUILD_TESTS
			DEBUG_PRINTF("DT: Visited fqd=%.*s\n", (int)dt->di->len, dt->di->fqd);
#endif
			(*visitor_func)(dt->di, context);
		}
	}

}

void visit_DomainTree(DomainTree_t *root,
		void(*visitor_func)(DomainInfo_t const *di, void *context),
		void *context)
{
	ASSERT(visitor_func);

	do_visit_DomainTree(root, visitor_func, context);
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
}
#endif
