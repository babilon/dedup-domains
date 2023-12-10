/**
 * domaintree.h
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
#ifndef DOMAIN_TREE_H
#define DOMAIN_TREE_H
#include "dedupdomains.h"
#include "uthash.h"

typedef struct DomainTree
{
	// malloc'ed string for tld. not null terminated.
	char *tld;
	// domain segments are at most 63 bytes
	uchar len;

	struct DomainInfo *di;
	struct DomainTree *child;
	struct DomainTree *parent;
	UT_hash_handle hh;
} DomainTree_t;

struct DomainView;
extern DomainTree_t* insert_DomainTree(DomainTree_t **dt, struct DomainView *dv);

extern void free_DomainTree(DomainTree_t **root);

extern void transfer_DomainInfo(DomainTree_t **root,
		void(*collector)(struct DomainInfo **di, void *context), void *context);

extern void visit_DomainTree(DomainTree_t *root,
		void(*visitor_func)(struct DomainInfo const *di, void *context),
		void *context);
extern void print_DomainTree(DomainTree_t *root);

#define DT_PARENT(dt) \
	((dt) == NULL ? NULL : (dt)->parent)

#endif
