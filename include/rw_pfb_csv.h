/**
 * rw_pfb_csv.h
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
#ifndef RW_PFB_CSV_H
#define RW_PFB_CSV_H
#include "dedupdomains.h"

typedef struct PortLineData
{
	// null terminated string
	char const *data;
	// length of the null terminated string to avoid strlen(x) calculations
	size_t len;
	// line number in the input file
	linenumber_t linenumber;
} PortLineData_t;

/**
 * Holds a copy of line data. Guaranteed to be null terminated and correspond to
 * a line in the file that ended in \r and/or \n. The data will not contain \r
 * or \n.
 */
typedef struct LineData
{
	char *buffer;
	char *pos;
	size_t len;
	size_t alloc;
	linenumber_t linenumber;
} LineData_t;

typedef struct NextLineContext
{
	linenumber_t *linenumbers;
	linenumber_t *begin_array;
	linenumber_t next_linenumber;
	size_len_t len;
} NextLineContext_t;

extern size_t default_buffer_len();
extern size_t get_max_line_len();

struct pfb_context;
struct ContextDomain;

extern void init_NextLineContext(NextLineContext_t *nlc, struct ContextDomain *cd);

extern void read_pfb_csv(struct pfb_context *,
		void(*do_stuff)(PortLineData_t const *const plv, struct pfb_context *,
			void *), void *context);
extern int read_pfb_line(struct pfb_context *, linenumber_t *nextline,
		void *shared_buffer, size_t buffer_size,
		void(*do_stuff)(PortLineData_t const *const plv, struct pfb_context *,
			void *), void *context);
extern void write_pfb_csv(PortLineData_t const *const pld, struct pfb_context *);
extern void write_pfb_csv_callback(PortLineData_t const *const pld,
		struct pfb_context *, void *);
#endif
