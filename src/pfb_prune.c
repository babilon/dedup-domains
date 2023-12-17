/**
 * pfb_prune.c
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
#include "domaintree.h"
#include "domaininfo.h"
#include "arraydomaininfo.h"
#include "pfb_context.h"
#include "contextdomain.h"
#include "domain.h"
#include "csvline.h"
#include "rw_pfb_csv.h"
#include "contextpair.h"
#include "pfb_prune.h"
#include "matchstrength.h"
#include <limits.h>

static size_t INITIAL_ARRAY_DOMAIN_INFO = 100000;
static size_t REALLOC_ARRAY_DOMAIN_INFO = 4096;

void set_DomainInfo_array_size(int v)
{
	if(v > 0)
	{
		INITIAL_ARRAY_DOMAIN_INFO = v;
		ADD_CC;
	}
	else
	{
		ELOG_STDERR("WARNING: ignoring user specified buffer size %d; using default.\n", v);
	}
}

void set_realloc_DomainInfo_size(int v)
{
	if(v > 0)
	{
		REALLOC_ARRAY_DOMAIN_INFO = v;
		ADD_CC;
	}
	else
	{
		ELOG_STDERR("WARNING: ignoring user specified realloc size %d; using default.\n", v);
	}
}

#ifdef COLLECT_DIAGNOSTICS
size_t collected_domains_counter = 0;
#endif

char* pfb_strdup(const char *in, size_len_t in_size)
{
	if(!in)
	{
		ELOG_STDERR("Input string must be non-nil\n");
		return NULL;
	}

	if(!in_size)
	{
		ELOG_STDERR("Input string must be non-empty\n");
		return NULL;
	}

	char *out_str = malloc(sizeof(char) * in_size + 1);

	if(out_str)
	{
		memcpy(out_str, in, in_size);
		out_str[in_size] = '\0';
		ADD_CC;
	}

	ADD_CC;
	return out_str;
}

char* outputfilename(const char *input, const char *ext)
{
	if(!input || !ext)
	{
		ELOG_STDERR("Input filename and extension must be non-nil\n");
		ADD_CC;
		return NULL;
	}

	if(!*input || !*ext)
	{
		ELOG_STDERR("Input filename and extension must be non-empty\n");
		ADD_CC;
		return NULL;
	}

	const char *walker, *period = NULL;
	for(walker = input; *walker; walker++)
	{
		if(*walker == '.')
		{
			period = walker;
			ADD_CC;
		}
		ADD_CC;
	}

	char *output = NULL;

	if(!period)
	{
		// walker will be at the nil terminator
		period = walker;
		ADD_CC;
	}

	size_t len = period - input + strlen(ext);
	ASSERT(len);
	output = malloc(sizeof(char) * len + 1);
	memcpy(output, input, period - input);
	memcpy(output + (period - input), ext, strlen(ext));
	*(output + len) = '\0'; // line 83
	ADD_CC;

	return output;
}

static MatchStrength_t get_csvline_match(CsvLineView_t *lv)
{
	ASSERT(lv);

	if(lv->cols_used >= 7)
	{
		const CsvColView_t cv_6 = get_CsvColView(lv, 6);

		// ISO/IEC 9899:201x Committee Draft — April 12, 2011 N1570
		// 5.2 Environmental considerations
		// 5.2.1 Character sets
		//
		// In both the source and execution basic character sets, the value of
		// each character after 0 in the above list of decimal digits shall be
		// one greater than the value of the previous.
		if(cv_6.len > 1)
		{
			// undefined / unsupported.
			ELOG_STDERR("WARNING: line has bogus unsupported value in column 7: %.*s\n",
					(int)cv_6.len, cv_6.data);
			// flag line bogus to skip further processing
			ADD_CC;
			return MATCH_BOGUS;
		}
		else
		{
			ADD_CC;
			return cv_6.data[0] - '0';
		}
	}
	else
	{
		ADD_CC;
		return MATCH_WEAK;
	}
}

static void pfb_insert(PortLineData_t const *const pld, pfb_context_t *pfbc,
		void *context)
{
	ASSERT(pld);
	ASSERT(pfbc);
	ASSERT(pfbc->in_file);
	ASSERT(pfbc->out_file);
	ASSERT(pfbc->dt);
	ASSERT(context);

	ContextPair_t *pc = context;
	CsvLineView_t *lv = pc->lv;
	DomainView_t *dv = pc->dv;

	ASSERT(lv);
	ASSERT(dv);

	update_CsvLineView(lv, pld->data);

	const CsvColView_t cv_1 = get_CsvColView(lv, 1);

	const MatchStrength_t ms = get_csvline_match(lv);
	if(ms == MATCH_REGEX)
	{
		// add the line information to list for direct carry over to the final
		// list.
		insert_carry_over(&pfbc->co, pld->linenumber);
		ADD_CC;
	}
	else
	{
		ASSERT(!null_DomainView(dv));
		if(!update_DomainView(dv, cv_1.data, cv_1.len))
		{
			ELOG_STDERR("ERROR: failed to update DomainView; possibly garbage input. insert skipped.\n");
		}
		else
		{
			// DomainView is valid only during an insert.
			dv->match_strength = ms;
			dv->context = pfbc;
			dv->linenumber = pld->linenumber;

			insert_DomainTree(pfbc->dt, dv);
			ADD_CC;
		}
	}
	ADD_CC;
}

size_t pfb_len_contexts(pfb_contexts_t *cs)
{
	ASSERT(cs);
	ADD_CC;
	return (cs->end_context - cs->begin_context);
}

pfb_contexts_t pfb_init_contexts(size_t alloc_contexts, const char *out_ext,
		char *const * argv)
{
	ASSERT(out_ext);
	ASSERT(argv);

	pfb_contexts_t cs;

	if(!alloc_contexts)
	{
#ifdef USE_MEMSET
		memset(&cs, 0, sizeof(cs));
#else
		cs.begin_context = NULL;
		cs.end_context = NULL;
#endif
		ADD_CC;
		return cs;
	}

	// using calloc for the included 'memset' to zero the DomainView instance.
	cs.begin_context = calloc(alloc_contexts, sizeof(pfb_context_t));
	// an array of size 1 to hold the one DomainTree and share across all
	// contexts.
	cs.begin_context->dt = calloc(1, sizeof(DomainTree_t*));
	// every context references the same dt. only have to null the place holder
	// for one of the contexts.
	ASSERT(cs.begin_context->dt[0] == NULL);

	cs.end_context = cs.begin_context + alloc_contexts;

	for(pfb_context_t *c = cs.begin_context; c != cs.end_context; c++, argv++)
	{
		ASSERT(!c->in_file);
		ASSERT(!c->out_file);
		c->dt = cs.begin_context->dt;
		ASSERT(*argv);
		c->in_fname = pfb_strdup(*argv, strlen(*argv));
		c->out_fname = outputfilename(c->in_fname, out_ext);
		init_carry_over(&c->co);
		ADD_CC;
	}

	ADD_CC;
	return cs;
}

void pfb_open_context(pfb_context_t *c, bool append_output)
{
	ASSERT(c);
	ASSERT(c->in_fname);
	ASSERT(c->out_fname);

	if(c->in_file)
	{
		ELOG_STDERR("ERROR: INPUT file is already open: %s\n",
				c->in_fname);
		ASSERT(false && "ERROR: INPUT file is already open");
		return;
	}

	if(c->out_file)
	{
		ELOG_STDERR("ERROR: OUTPUT file is already open: %s\n",
				c->in_fname);
		ASSERT(false && "ERROR: OUTPUT file is already open");
		return;
	}

	c->in_file = fopen(c->in_fname, "rb");
	if(!c->in_file)
	{
		ELOG_STDERR("ERROR: failed to open file for reading in binary mode: %s\n",
				c->in_fname);
		ASSERT(false && "ERROR: failed to open file for reading in binary mode");
		return;
	}

	c->out_file = fopen(c->out_fname, append_output ? "ab" : "wb");
	if(!c->out_file)
	{
		ELOG_STDERR("ERROR: failed to open file for writing in binary mode: %s\n",
				c->out_fname);
		ASSERT(false && "ERROR: failed to open file for writing in binary mode");
		return;
	}

	ADD_CC;
}

void pfb_flush_out_context(pfb_context_t *c)
{
	ASSERT(c);
	if(!c->out_file)
	{
		ELOG_STDERR("ERROR: ignoring call to flush on a nil file pointer: %s\n",
				c->in_fname);
		return;
	}

	fflush(c->out_file);
	ADD_CC;
}

void pfb_close_context(pfb_context_t *c)
{
	ASSERT(c);

	if(c->in_file)
	{
		fclose(c->in_file);
		c->in_file = NULL;
		ADD_CC;
	}

	if(c->out_file)
	{
		fclose(c->out_file);
		c->out_file = NULL;
		ADD_CC;
	}

	//free_DomainView(&c->dv);

	ADD_CC;
}

/**
 * Free all context collectively. There is no "pfb_free_context" because the set
 * of them share a pointer to DomainTree and only the first context has a
 * pointer to the DomainTree_t*. Yes, double pointer.
 */
void pfb_free_contexts(pfb_contexts_t *cs)
{
	ASSERT(cs);

	if(cs->begin_context)
	{
		// only the first context has the allocation for the one DomainTree_t.
		// free the one and only DomainTree. Once freed and set nil, all other
		// contexts will safely free a nil pointer.
		if(cs->begin_context->dt && cs->begin_context->dt[0])
		{
			// unexpected though not harmful situation.
			ELOG_STDERR("WARNING: domain tree is still set on context pfb_free_context()\n");

			free_DomainTree(&cs->begin_context->dt[0]);
			cs->begin_context->dt[0] = NULL;
			ADD_CC;
		}
		free(cs->begin_context->dt);
		cs->begin_context->dt = NULL;

		for(pfb_context_t *c = cs->begin_context; c < cs->end_context; c++)
		{
			pfb_close_context(c);
			free(c->in_fname);
			free(c->out_fname);
			free_carry_over(&c->co);
			ADD_CC;
		}

		free(cs->begin_context);
		ADD_CC;
	}
	cs->begin_context = NULL;
	cs->end_context = NULL;
	ADD_CC;
}

/**
 * Provides a callback that is specific to handling reading the CSV file
 * pfBlockerNG produces and adds appropriate entries to the DomainTree.
 */
void pfb_read_csv(pfb_contexts_t *cs)
{
	ASSERT(cs);
	ASSERT(cs->begin_context);
	ASSERT(cs->begin_context != cs->end_context);

	CsvLineView_t lv;
	// if this is multi-threaded, will need one lv per thread and break the loop
	// apart for each thread. then find a way to insert into hash table across
	// multiple threads.
	init_CsvLineView(&lv);

	DomainView_t dv;
	init_DomainView(&dv);

	ContextPair_t pc;
	pc.lv = &lv;
	pc.dv = &dv;

	for(pfb_context_t *c = cs->begin_context; c < cs->end_context; c++)
	{
		printf("Reading %s...\n", c->in_fname);
		// output file may not exist; if it does, this will ovewrite it.
		pfb_open_context(c, false);
		read_pfb_csv(c, pfb_insert, &pc);
		pfb_close_context(c);
		ADD_CC;
	}

	free_CsvLineView(&lv);
	free_DomainView(&dv);

	ADD_CC;
}

void init_ArrayDomainInfo(ArrayDomainInfo_t *array_di, const size_t alloc_contexts)
{
	ASSERT(array_di);
	ASSERT(alloc_contexts > 0);
	ASSERT(INITIAL_ARRAY_DOMAIN_INFO > 0);

	if(alloc_contexts == 0)
	{
		ELOG_STDERR("ERROR: request to allocate zero context elements.\n");
		return;
	}

	if(INITIAL_ARRAY_DOMAIN_INFO == 0)
	{
		ELOG_STDERR("ERROR: request to allocate zero DomainInfo elements.\n");
		return;
	}

	if(alloc_contexts > UINT_MAX)
	{
		ELOG_STDERR("WARNING: allocating over UINT_MAX context elements.\n");
	}

	if(INITIAL_ARRAY_DOMAIN_INFO > UINT_MAX)
	{
		ELOG_STDERR("WARNING: allocating more than UINT_MAX DomainInfo elements.\n");
	}

	array_di->begin_pfb_context = NULL;

	array_di->cd = malloc(sizeof(ContextDomain_t) * alloc_contexts);
	if(!array_di->cd)
	{
		exit(EXIT_FAILURE);
	}

#ifdef USE_MEMSET
	memset(array_di->cd, 0, sizeof(ContextDomain_t) * alloc_contexts);
#endif

	array_di->len_cd = alloc_contexts;

	for(size_t i = 0; i < alloc_contexts; i++)
	{
		array_di->cd[i].linenumbers = malloc(sizeof(linenumber_t) * INITIAL_ARRAY_DOMAIN_INFO);
		if(!array_di->cd[i].linenumbers)
		{
			exit(EXIT_FAILURE);
		}

		array_di->cd[i].next_idx = 0;
		array_di->cd[i].alloc_linenumbers = INITIAL_ARRAY_DOMAIN_INFO;
#ifdef COLLECT_DIAGNOSTICS
		array_di->cd[i].count_realloc_linenumbers = 0;
#endif
		ADD_CC;
	}
	ADD_CC;
}

void free_ArrayDomainInfo(ArrayDomainInfo_t *array_di)
{
	ASSERT(array_di);

	for(size_t i = 0; i < array_di->len_cd; i++)
	{
		ASSERT(array_di->cd);
		free(array_di->cd[i].linenumbers);
#ifdef COLLECT_DIAGNOSTICS
		LOG_DIAG("ContextDomain's 'linenumbers'", &(array_di->cd[i]),
				"realloc'ed %u times to final size of %u with %u used.\n",
				(uint)array_di->cd[i].count_realloc_linenumbers,
				(uint)array_di->cd[i].alloc_linenumbers,
				(uint)array_di->cd[i].next_idx);
		LOG_DIAG_CONT("Input file: %s\n\n",
				array_di->begin_pfb_context[i].in_fname);
#endif
		ADD_CC;
	}

	free(array_di->cd);

#ifdef USE_MEMSET
	memset(array_di, 0, sizeof(ArrayDomainInfo_t));
#else
	array_di->cd = NULL;
	array_di->len_cd = 0;
	array_di->begin_pfb_context = NULL;
#endif
	ADD_CC;
}

static void resize_ContextLinenumbers(ContextDomain_t *cd, size_t count)
{
	ASSERT(cd);
	ASSERT(count > 0);
	cd->alloc_linenumbers += count;
#ifdef COLLECT_DIAGNOSTICS
	cd->count_realloc_linenumbers++;
#endif
	linenumber_t *tmp = realloc(cd->linenumbers, sizeof(linenumber_t) * cd->alloc_linenumbers);
	if(tmp)
	{
		cd->linenumbers = tmp;
		tmp = NULL;
		ADD_CC_SINGLE;
	}
	else
	{
		free(cd->linenumbers);
		cd->linenumbers = NULL;
		ELOG_STDERR("ERROR: failed to realloc linenumbers array\n");
		exit(EXIT_FAILURE);
	}
	ADD_CC;
}

static void transfer_carry_over(ContextDomain_t *cd, pfb_context_t *pfbc)
{
	ASSERT(cd);
	ASSERT(pfbc);
	const linenumber_t count = len_carry_over(&pfbc->co);

	if(cd->next_idx + count > cd->alloc_linenumbers)
	{
		ASSERT(count > 0);
		resize_ContextLinenumbers(cd, (cd->next_idx + count - cd->alloc_linenumbers) );
		ADD_CC;
	}

	transfer_linenumbers(cd->linenumbers + cd->next_idx, &pfbc->co);
	cd->next_idx += count;

	ADD_CC;
}

/**
 * Move DomainInfo's to the FILE context specific array.
 */
static void collect_DomainInfo(DomainInfo_t **di, void *context)
{
	ASSERT(di);
	ASSERT(*di);
	ASSERT(context);

	ArrayDomainInfo_t *array_di = (ArrayDomainInfo_t*)context;

	ASSERT(array_di->begin_pfb_context);
	ASSERT(array_di->cd);

	// get begining of array of ContextDomain_t.
	ContextDomain_t *cd = array_di->cd;

	// move to context desired
	size_t idx = (pfb_context_t*)(*di)->context - array_di->begin_pfb_context;
	ASSERT(idx < array_di->len_cd);

	// move to desired ContextDomain_t
	cd += idx;

	if(cd->next_idx >= cd->alloc_linenumbers)
	{
		ASSERT(cd->next_idx < cd->alloc_linenumbers + REALLOC_ARRAY_DOMAIN_INFO);
		resize_ContextLinenumbers(cd, REALLOC_ARRAY_DOMAIN_INFO);
		ADD_CC;
	}

	// steal the DomainInfo from the DomainTree.
	// add the DomainInfo to the flat array referenced by context.
	ASSERT(cd->next_idx < cd->alloc_linenumbers);
	ASSERT((*di)->linenumber != 0);
	cd->linenumbers[cd->next_idx++] = (*di)->linenumber;
	free_DomainInfo(di);

#ifdef COLLECT_DIAGNOSTICS
	collected_domains_counter++;
#endif

	ASSERT(*di == NULL);
	ADD_CC;
}

int sort_LineNumbers(void const *a, void const *b)
{
	ADD_CC_SINGLE;
	// no line numbers in the 'used' section should be zero
	ASSERT(*(linenumber_t*)a != 0);
	ASSERT(*(linenumber_t*)b != 0);
	return *(linenumber_t*)a - *(linenumber_t*)b;
}

/**
 * Move all DomainInfo into a flat array of DomainInfo. Sort the DomainInfo
 * based on line number.
 */
void pfb_consolidate(DomainTree_t **root_dt, ArrayDomainInfo_t *array_di)
{
	ASSERT(root_dt);
	ASSERT(array_di);
	ASSERT(array_di->len_cd > 0);
	ASSERT(array_di->cd);
	for(size_t i = 0; i < array_di->len_cd; i++)
	{
		ASSERT(array_di->cd->linenumbers);
	}

	// 1. consolidate DomainInfo into a flat array
	// 2. qsort DomainInfo by linenumber; ignore the file info.
	// 3. iterate the list of DomainInfo and write each one to the respective
	// file. pfb_context_t will have the file name and each entry has a pointer
	// to the appropriate context along with linenumber. with the line numbers
	// in acending order, the file read can be done once and written to the new
	// files.
	//
	// ideally one context passed to the callback and that'll hold all array
	// information. it will also need to have all contexts to calculate the
	// index into the array.
#ifdef COLLECT_DIAGNOSTICS
	ASSERT(collected_domains_counter == 0);
#endif
	transfer_DomainInfo(root_dt, collect_DomainInfo, array_di);
#ifdef COLLECT_DIAGNOSTICS
	ASSERT(collected_domains_counter != 0);
#endif

	// tree is free'd
	ASSERT(!*root_dt);

	for(size_t i = 0; i < array_di->len_cd; i++)
	{
		// transfer into 'linenumbers' from array_di->begin_pfb_context + i
		// similar signature to memmove and memcpy:
		// void *destination, void *source, size_t num
		transfer_carry_over(&array_di->cd[i], &array_di->begin_pfb_context[i]);
	}

	// this could be done on separate threads.
	for(size_t i = 0; i < array_di->len_cd; i++)
	{
#ifdef DEBUG
		DEBUG_PRINTF("sorting. before sort the line numbers in order are:\n");
		for(size_t l = 0; l < array_di->cd[i].next_idx; l++)
		{
			DEBUG_PRINTF("\t%d\n", array_di->cd[i].linenumbers[l]);
		}
#endif
		ASSERT(i < array_di->len_cd);
		qsort(array_di->cd[i].linenumbers, array_di->cd[i].next_idx,
				sizeof(linenumber_t), sort_LineNumbers);

#ifdef DEBUG
		DEBUG_PRINTF("sorted. after sort the line numbers in order are:\n");
		for(size_t l = 0; l < array_di->cd[i].next_idx; l++)
		{
			DEBUG_PRINTF("\t%d\n", array_di->cd[i].linenumbers[l]);
		}
#endif
		ADD_CC;
	}
	ADD_CC;
}

/**
 * Thread safe. Each thread works with a given context and reads the DomainInfo
 * from the shared array and writes to the file assigned to the thread.
 */
void pfb_write_csv(pfb_contexts_t *cs, ArrayDomainInfo_t *array_di,
		bool use_shared_buffer)
{
	ASSERT(cs);
	ASSERT(cs->begin_context);
	ASSERT(cs->begin_context != cs->end_context);

	ASSERT(array_di);
	ASSERT(array_di->begin_pfb_context == cs->begin_context);
	ASSERT(array_di->len_cd == (uintptr_t)(cs->end_context - cs->begin_context));

	// array of DomainTree_t* of length 1 and free'd in pfb_free_context().
	// make sure that this is called w/o a DomainTree. otherwise it may indicate
	// a step skipped.
	ASSERT(!*((pfb_context_t*)array_di->begin_pfb_context)->dt);

	void *shared_buffer = NULL;

	// optionally use a shared buffer or not. not available for multi-threaded
	// runs. used by the read function. multi-thread can use a shared buffer for
	// each thread.
	if(use_shared_buffer)
	{
		shared_buffer = malloc(sizeof(char) * default_buffer_len());
		ADD_CC;
	}

	CsvLineView_t lv;
	// if this is multi-threaded, will need one lv per thread and break the loop
	// apart for each thread. then find a way to insert into hash table across
	// multiple threads.
	init_CsvLineView(&lv);

	for(pfb_context_t *c = cs->begin_context; c != cs->end_context; c++)
	{
		// index into parallel array of context associated
		// DomainInfo/linenumbers
		const size_t i = c - cs->begin_context;

		// can walk this array of line numbers.
		NextLineContext_t nlc;
		ASSERT(i < array_di->len_cd);
		init_NextLineContext(&nlc, &array_di->cd[i]);

		// by now, the initial pass and write of regex (if any) is done. now
		// open the output file in append mode to preserve regexes.
		pfb_open_context(c, true);
		// skip lines until the next_idx line to be read is zero
		if(nlc.next_linenumber != 0)
		{
			read_pfb_line(c, &nlc.next_linenumber, shared_buffer,
					default_buffer_len(), write_pfb_csv_callback, &nlc);
			ADD_CC;
		}
		pfb_close_context(c);
		ADD_CC;
	}

	free_CsvLineView(&lv);

	if(use_shared_buffer)
	{
		free(shared_buffer);
		ADD_CC;
	}

	ADD_CC;
}

#ifdef BUILD_TESTS
static void test_pfb_free_contexts()
{
	pfb_contexts_t pfbcs, pfbcs_zero;
	memset(&pfbcs, 0, sizeof(pfb_contexts_t));
	memset(&pfbcs_zero, 0, sizeof(pfb_contexts_t));

	pfb_free_contexts(&pfbcs);
	// verify free did not change anything
	assert(!memcmp(&pfbcs, &pfbcs_zero, sizeof(pfb_contexts_t)));
	ADD_TCC;
}

static void test_pfb_close_context()
{
	pfb_context_t pfbc;
	memset(&pfbc, 0, sizeof(pfb_context_t));

	// close on zero'ed context is safe
	pfb_close_context(&pfbc);

	char *const argv_i[] = {"FileInput_1.txt", "Zoo_2.txt", "Blarg.txt"};
	char *const *argi = argv_i;

	pfb_contexts_t pfbcs = pfb_init_contexts(3, ".work", argv_i);

	for(pfb_context_t *c = pfbcs.begin_context; c != pfbcs.end_context; c++, ++argi)
	{
		pfb_close_context(c);
		ADD_TCC;
	}

	pfb_free_contexts(&pfbcs);
	ADD_TCC;
}

static void test_pfb_init_contexts()
{
	char *const argv_i[] = {"FileInput_1.txt", "Zoo_2.txt", "Blarg.txt"};
	char *const argv_o[] = {"FileInput_1.work", "Zoo_2.work", "Blarg.work"};

	pfb_contexts_t pfbcs, pfbcs_zero;
	memset(&pfbcs, 1, sizeof(pfb_contexts_t));
	memset(&pfbcs_zero, 0, sizeof(pfb_contexts_t));

	// init with zero elements.
	pfbcs = pfb_init_contexts(0, ".lame", argv_i);
	// verify nothing allocated
	assert(!memcmp(&pfbcs, &pfbcs_zero, sizeof(pfb_contexts_t)));

	memset(&pfbcs, 1, sizeof(pfb_contexts_t));
	pfbcs = pfb_init_contexts(3, ".work", argv_i); // here !!

	assert(pfbcs.begin_context);
	assert(pfbcs.end_context);
	assert(pfbcs.end_context - pfbcs.begin_context == 3);
	assert(pfbcs.begin_context->dt);
	assert(pfbcs.begin_context->dt[0] == NULL);

	char *const *argi = argv_i;
	char *const *argo = argv_o;
	for(pfb_context_t *c = pfbcs.begin_context; c != pfbcs.end_context;
			c++, ++argi)
	{
		printf("argi=%s\n", *argi);
		printf("argv=%s\n", *argo);
		printf("in f: %s\n", c->in_fname);
		printf("ou f: %s\n", c->out_fname);
		printf("in*f: %p\n", c->in_file);
		printf("ou*f: %p\n", c->out_file);
		assert(c->in_fname);
		assert(c->out_fname);
		assert(!c->in_file);
		assert(!c->out_file);
		assert(c->dt == pfbcs.begin_context->dt);
		assert(c->dt[0] == NULL);
		assert(!strcmp(c->in_fname, *argi));
		assert(!strcmp(c->out_fname, *argo));
		argo++;
	}

	pfb_free_contexts(&pfbcs);

	ADD_TCC;
}

static void test_pfb_len_contexts()
{
	char *const argi[] = {"FileInput_1.txt", "Zoo_2.txt", "Blarg.txt"};

	pfb_contexts_t pfbcs, pfbcs_zero;
	memset(&pfbcs, 0, sizeof(pfb_contexts_t));
	memset(&pfbcs_zero, 0, sizeof(pfb_contexts_t));

	// get length of zero'ed struct is safe and lenght is 0
	assert(pfb_len_contexts(&pfbcs) == 0);

	// bogus: length of 0 with otherwise valid args is length 0
	pfbcs = pfb_init_contexts(0, ".lame", argi);
	assert(pfb_len_contexts(&pfbcs) == 0);
	// nothing is allocated

	pfbcs = pfb_init_contexts(1, ".lame", argi);
	assert(pfb_len_contexts(&pfbcs) == 1);
	pfb_free_contexts(&pfbcs);

	pfbcs = pfb_init_contexts(3, ".lame", argi);
	assert(pfb_len_contexts(&pfbcs) == 3);
	pfb_free_contexts(&pfbcs);

	ADD_TCC;
}

#define ASSERT_OPEN_CONTEXT_IDX(idx) do { \
	c = &pfbcs.begin_context[idx]; \
	pfb_open_context(&pfbcs.begin_context[idx], false); \
	assert(c->in_file); \
	assert(c->out_file); \
	p_in_file[idx] = c->in_file; \
	p_out_file[idx] = c->out_file; \
} while(0)
#define ASSERT_CONTEXT_IDX(idx, expect_v) do { \
	assert(!p_in_file[idx] == !expect_v); \
	assert(!p_out_file[idx] == !expect_v); \
	assert(p_in_file[idx] == pfbcs.begin_context[idx].in_file); \
	assert(p_out_file[idx] == pfbcs.begin_context[idx].out_file); \
	assert(!strcmp(pfbcs.begin_context[idx].in_fname, argv_i[idx])); \
	assert(!strcmp(pfbcs.begin_context[idx].out_fname, argv_o[idx])); \
} while(0)
#define ASSERT_CLOSE_CONTEXT_IDX(idx) do {\
	c = &pfbcs.begin_context[idx]; \
	pfb_close_context(c); \
	p_in_file[idx] = c->in_file; \
	p_out_file[idx] = c->out_file; \
} while(0)

static void test_pfb_open_context()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt",
							"tests/unit_pfb_prune/Zoo_2.txt",
							"tests/unit_pfb_prune/Blarg.txt"};

	char *const argv_o[] = {"tests/unit_pfb_prune/FileInput_1.work",
							"tests/unit_pfb_prune/Zoo_2.work",
							"tests/unit_pfb_prune/Blarg.work"};

	pfb_contexts_t pfbcs = pfb_init_contexts(3, ".work", argv_i);

	void *p_in_file[] = {NULL, NULL, NULL};
	void *p_out_file[] = {NULL, NULL, NULL};

	pfb_context_t *c = NULL;

	ASSERT_OPEN_CONTEXT_IDX(0);
	ASSERT_CONTEXT_IDX(0, true);
	ASSERT_CONTEXT_IDX(1, false);
	ASSERT_CONTEXT_IDX(2, false);

	ASSERT_OPEN_CONTEXT_IDX(1);
	ASSERT_CONTEXT_IDX(0, true);
	ASSERT_CONTEXT_IDX(1, true);
	ASSERT_CONTEXT_IDX(2, false);

	ASSERT_OPEN_CONTEXT_IDX(2);
	ASSERT_CONTEXT_IDX(0, true);
	ASSERT_CONTEXT_IDX(1, true);
	ASSERT_CONTEXT_IDX(2, true);

	ASSERT_CLOSE_CONTEXT_IDX(2);
	ASSERT_CONTEXT_IDX(0, true);
	ASSERT_CONTEXT_IDX(1, true);
	ASSERT_CONTEXT_IDX(2, false);

	ASSERT_CLOSE_CONTEXT_IDX(0);
	ASSERT_CONTEXT_IDX(0, false);
	ASSERT_CONTEXT_IDX(1, true);
	ASSERT_CONTEXT_IDX(2, false);

	ASSERT_CLOSE_CONTEXT_IDX(1);
	ASSERT_CONTEXT_IDX(0, false);
	ASSERT_CONTEXT_IDX(1, false);
	ASSERT_CONTEXT_IDX(2, false);

	ASSERT_OPEN_CONTEXT_IDX(0);

	ASSERT_CONTEXT_IDX(0, true);
	ASSERT_CONTEXT_IDX(1, false);
	ASSERT_CONTEXT_IDX(2, false);

	for(pfb_context_t *c = pfbcs.begin_context; c != pfbcs.end_context; ++c)
	{
		pfb_close_context(c);
	}

	pfb_free_contexts(&pfbcs);
	ADD_TCC;
}

void test_pfb_flush_context()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt",
							"tests/unit_pfb_prune/Zoo_2.txt",
							"tests/unit_pfb_prune/Blarg.txt"};

	pfb_contexts_t pfbcs = pfb_init_contexts(1, ".work", argv_i);

	pfb_context_t *c = pfbcs.begin_context;

	// flush unopened file is stupid. and safe.
	pfb_flush_out_context(c);

	pfb_open_context(c, false);
	// flush open file is safe
	pfb_flush_out_context(c);

	pfb_close_context(c);
	// flush a close file is stupid. and safe.
	pfb_flush_out_context(c);

	pfb_free_contexts(&pfbcs);
	ADD_TCC;
}

static void test_free_ArrayDi()
{
	ArrayDomainInfo_t array_di, array_di_zero;
	memset(&array_di, 0, sizeof(ArrayDomainInfo_t));
	memset(&array_di_zero, 0, sizeof(ArrayDomainInfo_t));

	free_ArrayDomainInfo(&array_di);

	assert(!memcmp(&array_di, &array_di_zero, sizeof(ArrayDomainInfo_t)));
	ADD_TCC;
}

static void test_init_ArrayDi()
{
	ArrayDomainInfo_t array_di, array_di_zero;
	memset(&array_di, 1, sizeof(ArrayDomainInfo_t));
	memset(&array_di_zero, 0, sizeof(ArrayDomainInfo_t));

	init_ArrayDomainInfo(&array_di, 1);

	assert(!array_di.begin_pfb_context);

	assert(array_di.cd);
	assert(array_di.len_cd == 1);
	assert(array_di.cd[0].linenumbers);
	assert(array_di.cd[0].next_idx == 0);
	assert(array_di.cd[0].alloc_linenumbers == INITIAL_ARRAY_DOMAIN_INFO);

	free_ArrayDomainInfo(&array_di);

	assert(!array_di.cd);
	assert(!array_di.len_cd);
	assert(!array_di.begin_pfb_context);

	ADD_TCC;
}

static void test_init_ArrayDi_size()
{
	ArrayDomainInfo_t array_di, array_di_zero;
	memset(&array_di, 1, sizeof(ArrayDomainInfo_t));
	memset(&array_di_zero, 0, sizeof(ArrayDomainInfo_t));

	init_ArrayDomainInfo(&array_di, 3);

	assert(!array_di.begin_pfb_context);

	assert(array_di.cd);
	assert(array_di.len_cd == 3);

	free_ArrayDomainInfo(&array_di);
	ADD_TCC;
}

#define SIZEOF(arr) (sizeof(arr) / sizeof(*arr))

typedef struct ExpectedReadPfbCsvData
{
	bool visited;
	const uint linenumber;
	const size_t len;
	const char *data;
} ExpectedReadPfbCsvData_t;

static ExpectedReadPfbCsvData_t expected_array[] = {
	{.visited=false, 1, .len=29, .data="this is a line that never end"},
	{.visited=false, 2, .len=29, .data="someone started writing it an"},
	{.visited=false, 3, .len=27, .data="This is a story of a lovely"},
	{.visited=false, 4, .len=345, .data="Ab minima ratione in unde perspiciatis est velit molestias! Aut aperiam nisi et incidunt nulla ut veritatis enim eos ratione consequatur? Qui asperiores nihil est adipisci consectetur ut optio voluptatem nam Quis deserunt ea rerum aperiam aut dignissimos esse aut corporis earum! Qui reprehenderit facilis et itaque sint ut voluptatem voluptate?"},
	{.visited=false, 5, .len=10, .data="          "},
	{.visited=false, 6, .len=78, .data="Lose eyes get fat shew. Winter can indeed letter oppose way change tended now."},
	{.visited=false, 7, .len=72, .data="²°°¶²¹³æ·±¹¸±²¸¹³¹¹²²¹±°­¸êëæì»ìë÷éæ"},
	{.visited=false, 8, .len=74, .data="So is improve my charmed picture exposed adapted demands. Received had end"},
	{.visited=false, 9, .len=71, .data="produced prepared diverted strictly off man branched. Known ye money so"},
	{.visited=false, 10, .len=5, .data="large"},
	{.visited=false, 11, .len=15, .data="wheredoesthisgo"},
	{.visited=false, 12, .len=79, .data="decay voice there to. Preserved be mr cordially incommode as an. He doors quick"},
	{.visited=false, 13, .len=66, .data="child an point at. Had share vexed front least style off why him. "},
};

static size_t lazy_index = 0;
static bool callback_was_hit = false;

static void test_cb1_read_pfb_csv(PortLineData_t const *const pld, pfb_context_t *pc,
		void *context)
{
	UNUSED(pc);
	ExpectedReadPfbCsvData_t *expected = (ExpectedReadPfbCsvData_t*)context;
	size_t expected_len = SIZEOF(expected_array);
	assert(lazy_index < expected_len);

	printf("____begin____\n");
	printf("lazyidx  #=%lu\n", lazy_index);
	printf("expected #=%u\n", expected[lazy_index].linenumber);
	printf("pld line #=%u\n", pld->linenumber);
	printf("expect len=%lu\n", expected[lazy_index].len);
	printf("pld len   =%lu\n", pld->len);
	printf("pld data D>%.*s<\n", (int)(pld->len < 60 ? pld->len : 60), pld->data);
	printf("----END----\n");

	assert(pld->linenumber > 0);
	assert(pld->linenumber == expected[lazy_index].linenumber);
	assert(expected[lazy_index].len == pld->len);
	assert(!memcmp(expected[lazy_index].data, pld->data, pld->len));

	assert(!expected[lazy_index].visited);
	expected[lazy_index].visited = true;

	// global counter incremented before processing to be one based
	lazy_index++;
	callback_was_hit = true;
}

static void test_read_pfb_csv()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_2.txt",
							"tests/unit_pfb_prune/Zoo_2.txt",
							"tests/unit_pfb_prune/Blarg.txt"};

	/*
	char *const argv_o[] = {"tests/unit_pfb_prune/FileInput_2.bench",
							"tests/unit_pfb_prune/Zoo_2.work",
							"tests/unit_pfb_prune/Blarg.work"};
							*/


	pfb_contexts_t pfbcs = pfb_init_contexts(1, ".bench", argv_i);
	pfb_open_context(pfbcs.begin_context, false);

	callback_was_hit = false;
	read_pfb_csv(pfbcs.begin_context, test_cb1_read_pfb_csv,
			expected_array);

	for(size_t i = 0; i < SIZEOF(expected_array); i++)
	{
		assert(expected_array[i].visited);
	}

	pfb_free_contexts(&pfbcs);

	assert(callback_was_hit);
}

static void test_cb2_read_pfb_csv(PortLineData_t const *const pld, pfb_context_t *pc,
		void *context)
{
	UNUSED(pc);
	size_t *linesread = context;
	(*linesread)++;
	if(*linesread == 101)
	{
		char *expect = malloc(101);
		memset(expect, 'a', 100);
		expect[100] = '\0';
		assert(pld->len == 100);
		assert(!memcmp(expect, pld->data, 101));
		callback_was_hit = true;
		free(expect);
	}
}

/**
 * Read a large file that ends w/o a newline. The last line is a line and
 * should be intact.
 *
 * Found a bug.
 */
static void test_read_pfb_csv_NONEWLINE()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/gen_read_pfb_csv_NONEWLINE_input.txt"};

	FILE *f_out = fopen(argv_i[0], "wb");

	const size_t LINE_LENGTH = 1032;

	char *block = malloc(LINE_LENGTH);
	memset(block, 'a', LINE_LENGTH);
	block[LINE_LENGTH - 1] = '\n';
	for(size_t i = 0; i < 100; i++)
	{
		fwrite(block, sizeof(char), LINE_LENGTH, f_out);
	}

	// looking for the last line. that 101 lines were read and the last line is
	// "whole" i.e. not missing data.
	fwrite(block, sizeof(char), 100, f_out);
	// explicitly not writing a newline

	free(block);

	fclose(f_out);


	pfb_contexts_t pfbcs = pfb_init_contexts(1, ".bench", argv_i);
	pfb_open_context(pfbcs.begin_context, false);

	size_t linesread = 0;
	callback_was_hit = false;
	read_pfb_csv(pfbcs.begin_context, test_cb2_read_pfb_csv,
			&linesread);

	assert(linesread == 101);
	assert(callback_was_hit);

	pfb_free_contexts(&pfbcs);
}

static void test_cb3_read_pfb_csv(PortLineData_t const *const pld, pfb_context_t *pc,
		void *context)
{
	UNUSED(pc);
	UNUSED(context);
	const size_t LINE_LENGTH = get_max_line_len();
	size_t *linesread = context;
	(*linesread)++;
	char *expect = malloc(LINE_LENGTH + 1);
	memset(expect, 'b', LINE_LENGTH);
	expect[LINE_LENGTH] = '\0';
	assert(pld->len == LINE_LENGTH);
	assert(!memcmp(expect, pld->data, LINE_LENGTH + 1));
	callback_was_hit = true;
	free(expect);
}

/**
 * Read a large that has lines longer than the "acceptable" line length.
 */
static void test_read_pfb_csv_LONGLINE()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/gen_read_pfb_csv_LONGLINE_input.txt"};

	FILE *f_out = fopen(argv_i[0], "wb");

	const size_t LINE_LENGTH = get_max_line_len() + 10;

	// just a bit over the max line length for the few cases where a long line
	// can throw a wrench into the mix
	char *block = malloc(LINE_LENGTH);
	memset(block, 'b', LINE_LENGTH);
	block[LINE_LENGTH - 1] = '\n';
	for(size_t i = 0; i < 5; i++)
	{
		fwrite(block, sizeof(char), LINE_LENGTH, f_out);
	}

	free(block);

	fclose(f_out);

	pfb_contexts_t pfbcs = pfb_init_contexts(1, ".bench", argv_i);
	pfb_open_context(pfbcs.begin_context, false);

	size_t linesread = 0;
	callback_was_hit = false;
	read_pfb_csv(pfbcs.begin_context, test_cb3_read_pfb_csv,
			&linesread);
	assert(linesread == 5);

	assert(callback_was_hit);

	pfb_free_contexts(&pfbcs);
}

static void test_cb4_read_pfb_csv(PortLineData_t const *const pld, pfb_context_t *pc,
		void *context)
{
	UNUSED(pc);
	const size_t LINE_LENGTH = 100;
	size_t *linesread = context;
	(*linesread)++;
	char *expect = malloc(LINE_LENGTH + 1);
	memset(expect, 'c', LINE_LENGTH);
	expect[LINE_LENGTH] = '\0';
	assert(pld->len == LINE_LENGTH);
	assert(!memcmp(expect, pld->data, LINE_LENGTH + 1));
	callback_was_hit = true;
	free(expect);
}

/**
 * Found a bug with a file that starts with a serious of newlines and has
 * content at the end of the file. no newline at the end.
 */
static void test_read_pfb_csv_ONELINE()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/gen_read_pfb_csv_ONELINE_input.txt"};

	FILE *f_out = fopen(argv_i[0], "wb");

	const size_t LINE_LENGTH = 100;

	char *block = malloc(LINE_LENGTH);
	memset(block, 'c', LINE_LENGTH);
	for(size_t i = 0; i < 10; i++)
	{
		fwrite("\n\r\n\r\r\r\n", sizeof(char), 7, f_out);
	}
	fwrite(block, sizeof(char), LINE_LENGTH, f_out);
	// explicitly not writing a newline

	free(block);
	fclose(f_out);

	pfb_contexts_t pfbcs = pfb_init_contexts(1, ".bench", argv_i);
	pfb_open_context(pfbcs.begin_context, false);

	size_t linesread = 0;
	callback_was_hit = false;
	read_pfb_csv(pfbcs.begin_context, test_cb4_read_pfb_csv,
			&linesread);

	assert(linesread == 1);
	assert(callback_was_hit);

	pfb_free_contexts(&pfbcs);
}

static void test_read_pfb_csv_NEWLINES()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/gen_read_pfb_csv_NEWLINES_input.txt"};

	FILE *f_out = fopen(argv_i[0], "wb");

	for(size_t i = 0; i < 1000; i++)
	{
		fwrite("\n\r\n\r\r\r\n\r\r\r\r\r", sizeof(char), 12, f_out);
	}

	fclose(f_out);

	pfb_contexts_t pfbcs = pfb_init_contexts(1, ".bench", argv_i);
	pfb_open_context(pfbcs.begin_context, false);

	size_t linesread = 0;
	callback_was_hit = false;
	read_pfb_csv(pfbcs.begin_context, test_cb4_read_pfb_csv,
			&linesread);

	assert(linesread == 0);
	assert(!callback_was_hit);

	pfb_free_contexts(&pfbcs);
}

static void test_cb5_read_pfb_csv(PortLineData_t const *const pld, pfb_context_t *pc,
		void *context)
{
	UNUSED(pc);
	size_t *linesread = context;
	(*linesread)++;

	assert(pld->len == 6);

	if(*linesread == 1)
		assert(!strncmp(pld->data, "line 1", 6));
	else if(*linesread == 2)
		assert(!strncmp(pld->data, "line 2", 6));
	else
		// not supposed to have more than 2 lines
		assert(false);

	callback_was_hit = true;
}

static void test_read_pfb_csv_TWOBETWEEN()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/gen_read_pfb_csv_TWOBETWEEN_input.txt"};

	FILE *f_out = fopen(argv_i[0], "wb");

	for(size_t i = 0; i < 1000; i++)
	{
		fwrite("\n\r\n\r\r\r\n\r\r\r\r\r", sizeof(char), 12, f_out);
	}

	fwrite("line 1", sizeof(char), 6, f_out);

	for(size_t i = 0; i < 1000; i++)
	{
		fwrite("\n\r\n\r\r\r\n\r\r\r\r\r", sizeof(char), 12, f_out);
	}

	fwrite("line 2", sizeof(char), 6, f_out);

	for(size_t i = 0; i < 1000; i++)
	{
		fwrite("\n\r\n\r\r\r\n\r\r\r\r\r", sizeof(char), 12, f_out);
	}

	fclose(f_out);

	pfb_contexts_t pfbcs = pfb_init_contexts(1, ".bench", argv_i);
	pfb_open_context(pfbcs.begin_context, false);

	size_t linesread = 0;
	callback_was_hit = false;
	read_pfb_csv(pfbcs.begin_context, test_cb5_read_pfb_csv,
			&linesread);

	assert(linesread == 2);
	assert(callback_was_hit);

	pfb_free_contexts(&pfbcs);
}

static void test_pfb_strdup()
{
	// empty string is bogus; return NULL
	assert(!pfb_strdup("", 0));

	char *dup = NULL;

	const char *inp = "what is here";

	dup = pfb_strdup(inp, strlen(inp));
	assert(dup);
	// include null terminator in comparison
	assert(!memcmp(dup, inp, 13));

	free(dup);
}

/**
 * Found several issues.
 */
static void test_outputfilename()
{
	const char *input, *ext;
	char *dup = NULL;

	assert(!outputfilename(NULL, "ignored"));
	assert(!outputfilename("ignored", NULL));

	assert(!outputfilename("", ".nil"));
	assert(!outputfilename("input", ""));

	input = "funky.txt";
	ext = ".work";
	dup = outputfilename(input, ext);
	assert(!memcmp("funky.work", dup, 11));
	free(dup);

	input = "Long Filename Short Ext.txt";
	ext = ".LONGEXT";
	dup = outputfilename(input, ext);
	assert(!memcmp("Long Filename Short Ext.LONGEXT", dup, 31));
	free(dup);

	input = "No_period_file";
	ext = ".period";
	dup = outputfilename(input, ext);
	assert(!memcmp("No_period_file.period", dup, 21));
	free(dup);

	input = "Period.here";
	ext = "noext";
	dup = outputfilename(input, ext);
	assert(!memcmp("Periodnoext", dup, 12));
	free(dup);

	input = "LongIn.filename";
	ext = ".txt";
	dup = outputfilename(input, ext);
	assert(!memcmp("LongIn.txt", dup, 11));
	free(dup);

	input = "Append_";
	ext = "the_ext";
	dup = outputfilename(input, ext);
	assert(!memcmp("Append_the_ext", dup, 15));
	free(dup);

	input = "filename.fat.txt";
	ext = ".pruned";
	dup = outputfilename(input, ext);
	assert(!memcmp("filename.fat.pruned", dup, 20));
	free(dup);

	input = "filename.txt.fat.pruned";
	ext = ".pruned.sorted";
	dup = outputfilename(input, ext);
	assert(!memcmp("filename.txt.fat.pruned.sorted", dup, 31));
	free(dup);

	input = "filename.txt.";
	ext = ".wat";
	dup = outputfilename(input, ext);
	assert(!memcmp("filename.txt.wat", dup, 17));
	free(dup);

	input = "filename.txt..";
	ext = ".wat";
	dup = outputfilename(input, ext);
	assert(!memcmp("filename.txt..wat", dup, 17));
	free(dup);

	ADD_TCC;
}

/**
 * Write a set of lines read via "read_pfb_csv".
 * Verify lines end with \n even when input is \r or a mix of \r\n.
 */
static void test_write_pfb_csv()
{
	ADD_TCC;
}

void info_pfb_prune()
{
	printf("Sizeof pfb_context_t: %lu\n", sizeof(pfb_context_t));
	printf("Sizeof pfb_contexts_t: %lu\n", sizeof(pfb_contexts_t));
	printf("Sizeof ContextDomain_t: %lu\n", sizeof(ContextDomain_t));
	printf("Sizeof ArrayDomainInfo_t: %lu\n", sizeof(ArrayDomainInfo_t));
	ADD_TCC;
}

static void test_pfb_insert()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt"};
	pfb_contexts_t pfbc = pfb_init_contexts(1, ".work", argv_i);

	// opens the output file for writing which will wipe it of anything that
	// might be in it.
	pfb_open_context(pfbc.begin_context, false);

	PortLineData_t pld;
	// bogus value for 'match_strength' will not insert to DomainTree and will
	// not write to disk.
	pld.data = "something,that,has,many,columns,breaking,pfb_insert,wildly";
	pld.linenumber = 10;
	pld.len = strlen(pld.data);

	CsvLineView_t lv_context;
	init_CsvLineView(&lv_context);

	DomainView_t dv_context;
	init_DomainView(&dv_context);

	ContextPair_t context;
	context.lv = &lv_context;
	context.dv = &dv_context;

	pfb_insert(&pld, pfbc.begin_context, &context);

	assert(pfbc.begin_context->dt[0] == NULL);

	free_CsvLineView(&lv_context);
	free_DomainView(&dv_context);
	pfb_free_contexts(&pfbc);

	char *buffer = malloc(sizeof(char) * 100);
	FILE *check_output = fopen("tests/unit_pfb_prune/FileInput_1.work", "rb");
	size_t read = fread(buffer, sizeof(char), 100, check_output);
	fclose(check_output);
	free(buffer);

	assert(read == 0);

	ADD_TCC;
}

static void test_pfb_insert_0()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt"};
	pfb_contexts_t pfbc = pfb_init_contexts(1, ".work", argv_i);

	pfb_open_context(pfbc.begin_context, false);

	PortLineData_t pld;
	pld.data = ",something,that,has,many,columns,0,pfb_insert,wildly";
	pld.linenumber = 10;
	pld.len = strlen(pld.data);

	CsvLineView_t lv_context;
	init_CsvLineView(&lv_context);

	DomainView_t dv_context;
	init_DomainView(&dv_context);

	ContextPair_t context;
	context.lv = &lv_context;
	context.dv = &dv_context;

	pfb_insert(&pld, pfbc.begin_context, &context);

	assert(pfbc.begin_context->dt[0]);
	assert(pfbc.begin_context->dt[0]->di);
	assert(pfbc.begin_context->dt[0]->di->linenumber == 10);
	assert(pfbc.begin_context->dt[0]->match_strength == MATCH_WEAK);
	assert(pfbc.begin_context->dt[0]->di->context == pfbc.begin_context);

	free_CsvLineView(&lv_context);
	free_DomainView(&dv_context);
	pfb_free_contexts(&pfbc);
	ADD_TCC;

	char *buffer = malloc(sizeof(char) * 100);
	FILE *check_output = fopen("tests/unit_pfb_prune/FileInput_1.work", "rb");
	size_t read = fread(buffer, sizeof(char), 100, check_output);
	fclose(check_output);
	free(buffer);

	assert(read == 0);
}

static void test_pfb_insert_1()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt"};
	pfb_contexts_t pfbc = pfb_init_contexts(1, ".work", argv_i);

	pfb_open_context(pfbc.begin_context, false);

	PortLineData_t pld;
	pld.data = ",something,that,has,many,columns,1,pfb_insert,wildly";
	pld.linenumber = 11;
	pld.len = strlen(pld.data);

	CsvLineView_t lv_context;
	init_CsvLineView(&lv_context);

	DomainView_t dv_context;
	init_DomainView(&dv_context);

	ContextPair_t context;
	context.lv = &lv_context;
	context.dv = &dv_context;

	pfb_insert(&pld, pfbc.begin_context, &context);

	assert(pfbc.begin_context->dt[0]);
	assert(pfbc.begin_context->dt[0]->di);
	assert(pfbc.begin_context->dt[0]->di->linenumber == 11);
	assert(pfbc.begin_context->dt[0]->match_strength == MATCH_FULL);
	assert(pfbc.begin_context->dt[0]->di->context == pfbc.begin_context);

	free_CsvLineView(&lv_context);
	free_DomainView(&dv_context);
	pfb_free_contexts(&pfbc);
	ADD_TCC;

	char *buffer = malloc(sizeof(char) * 100);
	FILE *check_output = fopen("tests/unit_pfb_prune/FileInput_1.work", "rb");
	size_t read = fread(buffer, sizeof(char), 100, check_output);
	fclose(check_output);
	free(buffer);

	assert(read == 0);
}

static void test_pfb_insert_2()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt"};
	pfb_contexts_t pfbc = pfb_init_contexts(1, ".work", argv_i);

	pfb_open_context(pfbc.begin_context, false);

	PortLineData_t pld;
	// column 7 is REGEX_MATCH which means insert regex into cached and carry
	// over.
	pld.data = ",something,that,has,many,columns,2,pfb_insert,wildly";
	pld.linenumber = 10;
	pld.len = strlen(pld.data);

	CsvLineView_t lv_context;
	init_CsvLineView(&lv_context);

	DomainView_t dv_context;
	init_DomainView(&dv_context);

	ContextPair_t context;
	context.lv = &lv_context;
	context.dv = &dv_context;

	pfb_insert(&pld, pfbc.begin_context, &context);

	assert(!pfbc.begin_context->dt[0]);

	// always carry over regex lines and preserve their original location
	assert(len_carry_over(&pfbc.begin_context->co) == 1);

	free_CsvLineView(&lv_context);
	free_DomainView(&dv_context);
	pfb_free_contexts(&pfbc);
	ADD_TCC;

	char *buffer = malloc(sizeof(char) * 100);
	FILE *check_output = fopen("tests/unit_pfb_prune/FileInput_1.work", "rb");
	size_t read = fread(buffer, sizeof(char), 100, check_output);
	fclose(check_output);

	// always carry over regex lines; nothing written.
	assert(read == 0);

	free(buffer);
}

static void test_pfb_insert_fewcols()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt"};
	pfb_contexts_t pfbc = pfb_init_contexts(1, ".work", argv_i);

	pfb_open_context(pfbc.begin_context, false);

	PortLineData_t pld;
	pld.data = ",something,";
	pld.linenumber = 12;
	pld.len = strlen(pld.data);

	CsvLineView_t lv_context;
	init_CsvLineView(&lv_context);

	DomainView_t dv_context;
	init_DomainView(&dv_context);

	ContextPair_t context;
	context.lv = &lv_context;
	context.dv = &dv_context;

	pfb_insert(&pld, pfbc.begin_context, &context);

	assert(pfbc.begin_context->dt[0]);
	assert(pfbc.begin_context->dt[0]->di);
	assert(pfbc.begin_context->dt[0]->di->linenumber == 12);
	assert(pfbc.begin_context->dt[0]->match_strength == MATCH_WEAK);
	assert(pfbc.begin_context->dt[0]->di->context == pfbc.begin_context);

	free_CsvLineView(&lv_context);
	free_DomainView(&dv_context);
	pfb_free_contexts(&pfbc);
	ADD_TCC;

	char *buffer = malloc(sizeof(char) * 100);
	FILE *check_output = fopen("tests/unit_pfb_prune/FileInput_1.work", "rb");
	size_t read = fread(buffer, sizeof(char), 100, check_output);
	fclose(check_output);
	free(buffer);

	assert(read == 0);
}

static void cb_do_nothing(PortLineData_t const *const pld, pfb_context_t *pc,
		void *context)
{
	UNUSED(pld);
	UNUSED(pc);
	UNUSED(context);
	assert(false && "not expected to call this callback");
}

static void test_pfb_read_line_ZERO()
{
	char *const argv_i[] = {"tests/unit_pfb_prune/FileInput_1.txt"};
	pfb_contexts_t pfbc = pfb_init_contexts(1, ".work", argv_i);

	pfb_open_context(pfbc.begin_context, false);

	linenumber_t nextline = 0;
	const int ret = read_pfb_line(pfbc.begin_context, &nextline, NULL, 100, cb_do_nothing, NULL);
	assert(ret == 0);

	pfb_free_contexts(&pfbc);
}

void test_pfb_prune()
{
	test_pfb_strdup();
	test_outputfilename();
	test_pfb_free_contexts();
	test_pfb_init_contexts();
	test_pfb_len_contexts();
	test_pfb_close_context();
	test_pfb_open_context();
	test_pfb_flush_context();
	test_free_ArrayDi();
	test_init_ArrayDi();
	test_init_ArrayDi_size();
	test_read_pfb_csv();
	test_read_pfb_csv_NONEWLINE();
	test_read_pfb_csv_LONGLINE();
	test_read_pfb_csv_ONELINE();
	test_read_pfb_csv_NEWLINES();
	test_read_pfb_csv_TWOBETWEEN();
	test_pfb_read_line_ZERO();
	test_pfb_insert();
	test_write_pfb_csv();
	test_pfb_insert_0();
	test_pfb_insert_1();
	test_pfb_insert_2();
	test_pfb_insert_fewcols();
	ADD_TCC;
}
#endif
