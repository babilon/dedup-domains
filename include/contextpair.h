/**
 * contextpair.h
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
#ifndef CONTEXTPAIR_H
#define CONTEXTPAIR_H
typedef struct ContextPair
{
	union {
	void *c1;
	void *lv;
	};

	union {
	void *c2;
	/**
	 * Transitional payload during pfb_insert(). Carries CsvLineView data
	 * through insert_DomainTree() for insertion (if unique) into the
	 * DomainTree. Initialized once. Free'ed in pfb_close_context().
	 */
	void *dv;
	void *nlc;
	};
} ContextPair_t;
#endif
