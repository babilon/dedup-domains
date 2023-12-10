/**
 * codecoverage.c
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
#include "codecoverage.h"
#include "uthash.h"
#include <stdio.h>

#ifndef CODECOVERAGE
void print_lineshit() {}
#else

typedef struct ccline
{
	void *linenum;
	const char *filename;
	UT_hash_handle hh;
} ccline_t;

typedef struct ccfile
{
	void *filename;
	ccline_t *lines;
	UT_hash_handle hh;
} ccfile_t;

static ccfile_t *ccft = NULL;
static bool calledonce = false;

void add_coverage(unsigned long linenum, const char *filename)
{
	ccfile_t *_ccf = NULL;
	ccline_t *_ccl = NULL;

	if(calledonce && ccft == NULL)
	{
		assert(ccft);
	}

	calledonce = true;

	void *fptr = (char*)filename;
	HASH_FIND_PTR(ccft, &fptr, _ccf);
	if(!_ccf)
	{
		_ccf = malloc(sizeof(ccfile_t));
		_ccf->filename = (void*)filename;
		_ccf->lines = NULL;
		HASH_ADD_PTR(ccft, filename, _ccf);
	}

	HASH_FIND_PTR(ccft, &filename, _ccf);
	assert(_ccf);

	void *lptr = (void*)(size_t)linenum;
	HASH_FIND_PTR(_ccf->lines, &lptr, _ccl);
	if(!_ccl)
	{
		_ccl = malloc(sizeof(ccline_t));
		_ccl->linenum = (void*)(size_t)linenum;
		_ccl->filename = filename;
		HASH_ADD_PTR(_ccf->lines, linenum, _ccl);
	}
}

int by_str(const ccfile_t *a, const ccfile_t *b)
{
	return strcmp(a->filename, b->filename);
}

int by_linenum(const ccline_t *a, const ccline_t *b)
{
	return a->linenum - b->linenum;
}

void print_lineshit()
{
	ccfile_t *cur, *tmp;
	ccline_t *curl, *tmpl;
	FILE *f;

	// 'comm' requires sorted lines; use 'sort' command instead of sorting
	// the hash table to control how lines are sorted.
	//HASH_SORT(ccft, by_str);

	f = fopen("lineshit.raw", "wb");
	if(f)
	{
		HASH_ITER(hh, ccft, cur, tmp)
		{
			//HASH_SORT(cur->lines, by_linenum);
			HASH_ITER(hh, cur->lines, curl, tmpl)
			{
				fprintf(f, "%s %lu\n", (char const*)curl->filename, (size_t)curl->linenum);
				HASH_DEL(cur->lines, curl);
				free(curl);
			}

			HASH_DEL(ccft, cur);
			free(cur);
		}
	}

	fclose(f);
}

#endif
