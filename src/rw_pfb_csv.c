/**
 * rw_pfb_csv.c
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
#include "rw_pfb_csv.h"
#include "contextdomain.h"
#include "pfb_context.h"
#include "pfb_prune.h"

// 4096 is *probably* a safe sane reasonable default.
static const size_t READ_BUFFER_SIZE = 4096;

// what is a reasonable length for a line in csv file?
static const size_t MAX_ACCEPTABLE_LINE_LENGTH = READ_BUFFER_SIZE * 0.5;
static const size_t MAX_ALLOC_LINE = MAX_ACCEPTABLE_LINE_LENGTH + 1; // for null terminator
																	 //
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

size_t default_buffer_len()
{
	return READ_BUFFER_SIZE;
}

size_t get_max_line_len()
{
	return MAX_ACCEPTABLE_LINE_LENGTH;
}

static void init_LineData(LineData_t *ld)
{
	ASSERT(ld);

#ifdef USE_MEMSET
	memset(ld, 0, sizeof(LineData_t));
#else
	ld->len = 0;
	ld->linenumber = 0;
#endif

	static const size_t initial_size = 100;

	ld->buffer = malloc(sizeof(char) * initial_size);
	if(!ld->buffer)
		exit(EXIT_FAILURE);

	ld->pos = ld->buffer;
	ld->alloc = initial_size;

	ADD_CC;
}

static void free_LineData(LineData_t *ld)
{
	ASSERT(ld);

	free(ld->buffer);
#ifdef USE_MEMSET
	memset(ld, 0, sizeof(LineData_t));
#else
	ld->buffer = NULL;
	ld->pos = NULL;
	ld->len = 0;
	ld->alloc = 0;
	ld->linenumber = 0;
#endif

	ADD_CC;
}

static void reset_LineData(LineData_t *ld)
{
	ASSERT(ld);
	ld->pos = ld->buffer;
	ld->len = 0;

	ADD_CC;
}

/**
 * Need to shift the buffer contents or use a circular buffer and also able to
 * add to the buffer or pause and return later. the buffer can remain at a fixed
 * size for the duration of reading. might be a tuneable thing.
 *
 * Most efficient is going to be raw array increment. not a complex circular
 * buffer that has overloaded operators. able to return to the filling is ideal.
 * save the location of LineData_t and fill in the holes on re-entrance.
 */
static bool load_LineData(char const *buffer, char const *end_buffer,
		char const **pos_buffer, LineData_t *ld, bool skipline )
{
	ASSERT(buffer);
	ASSERT(end_buffer);
	ASSERT(pos_buffer);
	ASSERT(ld);

	// invalid to call w/o a span of bytes to process
	ASSERT(end_buffer - buffer);
	// could lead to big problems if too much buffer is given
	ASSERT((size_t)(end_buffer - buffer) <= READ_BUFFER_SIZE);

	char const *c = buffer;

	// the input buffer is maxed to the buffer size read from disk. the maximum
	// length will be more than the acceptable line length.
	while(c != end_buffer && *c != '\n' && *c != '\r')
	{
		c++;
		ADD_CC;
	}

	// update position of output buffer. the next line or next block of data
	// will begin with this position.
	*pos_buffer = c;

	// this will be at most READ_BUFFER_SIZE which is well below the maximum a
	// size_t can hold.
	size_t len = c - buffer;

	// newline found before end of input buffer.
	const bool found_newline = (ld->len + len) && (c != end_buffer);

	// +1 for null terminator
	size_t request_alloc = ld->len + len + 1;
	if(request_alloc >= MAX_ALLOC_LINE)
	{
		ELOG_STDERR("WARNING: excessive line length. truncating characters after %lu. requested: %lu\n",
				MAX_ACCEPTABLE_LINE_LENGTH, request_alloc);
		request_alloc = MAX_ALLOC_LINE;
		// fix length to the max supported ignoring the stuff after
		len = request_alloc - ld->len - 1;
		ADD_CC;
	}

	// when skipping lines of no interest, at minimum, the "length" of the line
	// data must be updated even if the buffer is nil. it's important to not
	// read the buffer despite the length. this length is a factor in
	// determining whether a newline was found, to increment the line counter.
	ld->len += len;

	if(skipline)
	{
		ADD_CC;
		// true indicates a \n or \r was read
		// false indicates more reading necessary.
		return found_newline;
	}

	if(request_alloc > ld->alloc)
	{
		// hopefully the file is not hog wild and has universal long lines.
		const size_t offset = ld->pos - ld->buffer;
		char *tmp = realloc(ld->buffer, sizeof(char) * request_alloc);
		if(tmp)
		{
			ld->buffer = tmp;
			ld->pos = ld->buffer + offset;
			ld->alloc = request_alloc;
			ADD_CC;
		}
		else
		{
			ELOG_STDERR("ERROR: memory reallocation failed.\n");
			return found_newline;
		}
		ADD_CC;
	}

	// null terminator is excluded from the "length" of the string.  don't copy
	// the extra byte from 'buffer'. it might be out of bounds.
	// only copy the number of bytes read in this iteration.  this is appending
	// data.
	memcpy(ld->pos, buffer, len);
	// if more to add, start at 'pos'
	ld->pos += len;

	// null terminator for easy consumption
	if(found_newline)
	{
		*ld->pos = '\0';
		ADD_CC;
	}

	ADD_CC;
	// true indicates a \n or \r was read
	// false indicates more reading necessary.
	return found_newline;
}

/**
 * This is given an iterator to a list of line numbers; when 'next' returns
 * false, then this will terminate and if necessary break out of loops.
 *
 * When 'nextline' is NOT zero and NOT LINENUMBER_MAX, this will skip lines
 * until it has read ONE line before the line number that is requested.
 *
 * @param pfbc Context for FILE to be read/written.
 *
 * @param nextline Pointer to a storage location holding the next line number to
 * be read. LINENUMBER_MAX indicates read and call 'do_stuff' callback for ALL
 * lines. Zero is the invalid line number and indicates no more lines to be read
 * or as an initial value, do not read any lines. All other values are treated
 * as line numbers in the file and only when the matching line number is read
 * will this call 'do_stuff' callback with the provided context and the line
 * read.
 *
 * @param shared_buffer Optional externally managed buffer that this can utilize
 * as a temporary buffer. If NULL, allocates and frees a buffer within.
 *
 * @param buffer_size Size of optionally provided buffer; otherwise number of
 * bytes to read from file.
 *
 * @param do_stuff Callback that is expected to increment 'nextline'.
 *
 * @param context Context for callback 'do_stuff'. It is expected to increment
 * 'nextline'.
 *
 * @return The number of lines read unless nextline is initially zero in which
 * case it returns 0.
 */
int read_pfb_line(pfb_context_t *pfbc,
		linenumber_t *nextline,
		void *shared_buffer, size_t buffer_size,
		void(*do_stuff)(PortLineData_t const *const pld, pfb_context_t *, void *),
		void *context)
{
	ASSERT(pfbc);
	ASSERT(nextline);
	ASSERT(buffer_size > 0);
	ASSERT(do_stuff);

	FILE *f = pfbc->in_file;
	ASSERT(f);
	// programmer error to not rewind before entering this function.
	assert(ftell(f) == 0 && "tests only");

	char **buffer;
	char *local_buffer;
	const char *pos_buffer, *end_buffer;

	// if shared_buffer is NULL, i.e., not using shared buffer, ensure a valid
	// pointer is available for a space to reference.
	buffer = &local_buffer;
	// if shared is not-nil, then the input buffer is used. otherwise the value
	// of shared_buffer is NULL and assigning NULL to local_buffer is safe.
	*buffer = (char*)shared_buffer;

	// 1 based gives opportunity to handle a situation where there are
	// no line numbers to read. maybe 0 is the value to specify when
	// no lines to read i.e. end of iterator. then it is one if statement
	// instead of 'did the iter return a value' and 'did the iter return a valid
	// line number' two checks vs 1.
	if(*nextline == 0)
	{
		ADD_CC;
		// only real reason is to indicate nothing read
		return 0;
	}

	if(!*buffer)
	{
		// a buffer for each function call allows this to be called by different
		// threads acting on different files.
		*buffer = malloc(sizeof(char) * buffer_size);
		if(!*buffer)
		{
			exit(EXIT_FAILURE);
		}
		ADD_CC;
	}

	// one LineData for the entire read operation. this is also tracking the
	// lines read IN this context.
	LineData_t ld;
	init_LineData(&ld);

	bool escape = false;
	do
	{
		const size_t read_count = fread(*buffer, sizeof(char), buffer_size, f);
		if(read_count > 0)
		{
			// reset buffer pointers to new 'end' and new current 'pos'
			end_buffer = *buffer + read_count;
			pos_buffer = *buffer;

			// idea is to read all data from the buffer before reading next
			// chunk. get as many LineData's as are available filled.
			do
			{
				// nextline = 1
				// ld.linenumber = 0
				//
				//	begins here
				//	v					v <-- ends here
				// >1| www.first.domain.com
				//  2| sec.domain.stuff.com
				//  3| 3rd.domain.com
				// (nextline - 1) > 0 = FALSE so .. keep line
				//
				// after reading, linenumber is 1
				//
				//	v
				//  1| www.first.domain.com
				// >2| sec.domain.stuff.com
				//  3| 3rd.domain.com
				bool newline = false;

				// read all lines: do not skip
				if(*nextline == LINENUMBER_MAX)
				{
					newline = load_LineData(pos_buffer,
							end_buffer, &pos_buffer, &ld, false);
				}
				// skip this line
				else if(*nextline - 1 > ld.linenumber)
				{
					newline = load_LineData(pos_buffer,
							end_buffer, &pos_buffer, &ld, true);
				}
				else // do not skip
				{
					newline = load_LineData(pos_buffer,
							end_buffer, &pos_buffer, &ld, false);
				}

				ASSERT(nextline != 0);

				if(newline)
				{
					ld.linenumber++;

					// LINENUMBER_MAX indicates read all lines: this is during
					// initial read to building the tree. OR when the current
					// line read is the next line of interest, such as during
					// the write phase.
					if(*nextline == LINENUMBER_MAX || ld.linenumber == *nextline)
					{
						// temporary struct to pass the necessary data to the
						// callback for CSV parsing or another line format.
						PortLineData_t pld;
						pld.data = ld.buffer;
						ASSERT(ld.linenumber != 0);
						pld.linenumber = ld.linenumber;
						pld.len = ld.len;

						do_stuff(&pld, pfbc, context);
						ADD_CC;
					}

					// the callback will modify 'nextline'. if the next line of
					// interest is 0, then escape this loop: there are no more
					// lines to read from the input file to write to the output.
					// '*nextline' will remain equal to LINENUMBER_MAX during
					// the initial phase to read all lines and build the tree.
					if(*nextline == 0)
					{
						escape = true;
						// terminates the inner loop
						pos_buffer = end_buffer;
						ADD_CC;
					}
					else
					{
						reset_LineData(&ld);
						ADD_CC;
					}
					ADD_CC;
				}

				// move past the newline characters
				while(pos_buffer != end_buffer && (*pos_buffer == '\r' || *pos_buffer == '\n'))
				{
					pos_buffer++;
					ADD_CC;
				}
				ADD_CC;
			} while(pos_buffer != end_buffer);
			ADD_CC;
		}
		ADD_CC;
	} while(feof(f) == 0 && !escape);

	// the last line. if the file ends w/o a newline, it's still a line.
	if(ld.len && !escape)
	{
		ld.linenumber++;
		*ld.pos = '\0';

		PortLineData_t pld;
		pld.data = ld.buffer;
		pld.linenumber = ld.linenumber;
		pld.len = ld.len;

		do_stuff(&pld, pfbc, context);

		ADD_CC;
	}

	size_t lines_read = ld.linenumber;
	free_LineData(&ld);

	// if using shared, local_buffer is equal to shared and free(nil) is handled
	// externally. if using local_buffer, free() is necessary.
	if(!shared_buffer)
	{
		free(local_buffer);
		ADD_CC;
	}
	ADD_CC;

	return lines_read;
}

static int writeline(FILE *f, PortLineData_t const *pld)
{
	ASSERT(f);
	ASSERT(pld);
	ASSERT(pld->data);
	ASSERT(pld->len);

	const size_t wrote = fwrite(pld->data, sizeof(char), pld->len, f);
	if( wrote != pld->len)
		return -1;

	if(fwrite("\n", sizeof(char), 1, f) != 1)
		return -2;

	ADD_CC;
	return 0; // A-OK
}

void init_NextLineContext(NextLineContext_t *nlc, ContextDomain_t *cd)
{
	memset(nlc, 0, sizeof(NextLineContext_t));

	nlc->linenumbers = cd->linenumbers;
	nlc->begin_array = nlc->linenumbers;
	nlc->len = cd->next_idx;
	nlc->next_linenumber = nlc->len ? nlc->linenumbers[0] : 0;
}

void write_pfb_csv_callback(PortLineData_t const *const pld, pfb_context_t *pfbc, void *context)
{
	ASSERT(pld);
	ASSERT(pfbc);
	ASSERT(context);

	write_pfb_csv(pld, pfbc);

	NextLineContext_t *nlc = (NextLineContext_t*)context;
	// previous line number was not zero
	ASSERT(nlc->next_linenumber != 0);

	ASSERT(nlc->begin_array);
	ASSERT(nlc->len);
	ASSERT(nlc->linenumbers != nlc->begin_array + nlc->len);
	// previous line number was not zero
	ASSERT(nlc->linenumbers[0] != 0);

	// get the next line number. it might be zero. which indicates end of lines
	// to be read.
	nlc->linenumbers++;

	// if at the end of the array, set 'nextline' to zero to indicate EOL
	if(nlc->linenumbers == nlc->begin_array + nlc->len)
	{
		nlc->next_linenumber = 0;
		ADD_CC;
	}
	else
	{
		// there are no more line numbers for this context to be processed.
		// update the output parameter. 0 means stop.
		nlc->next_linenumber = *nlc->linenumbers;
		ADD_CC;
	}

	ADD_CC;
}

void write_pfb_csv(PortLineData_t const *const pld, pfb_context_t *pfbc)
{
	ASSERT(pld);
	ASSERT(pfbc);
	ASSERT(pfbc->out_file);

	const int err = writeline(pfbc->out_file, pld);
	if(err)
	{
		ELOG_STDERR("ERROR (%d) while attempting to write line (%s) to '%s'\n",
				err, pld->data, pfbc->out_fname);
	}
	ADD_CC;
}

/**
 * Read initial CSV input file. Do not skip ANY lines. All lines are processed.
 */
void read_pfb_csv(pfb_context_t *pfbc,
		void(*do_stuff)(PortLineData_t const *const pld, pfb_context_t *, void *),
		void *context)
{
	linenumber_t nextline = LINENUMBER_MAX;
	read_pfb_line(pfbc, &nextline, NULL, READ_BUFFER_SIZE, do_stuff, context);
}

#ifdef BUILD_TESTS
static void test_free_LineData()
{
	LineData_t ld, ld_zero;
	memset(&ld, 0, sizeof(LineData_t));
	memset(&ld_zero, 0, sizeof(LineData_t));

	free_LineData(&ld);

	assert(!memcmp(&ld, &ld_zero, sizeof(LineData_t)));
}

static void test_init_LineData()
{
	LineData_t ld;
	init_LineData(&ld);

	assert(ld.len == 0);
	assert(ld.alloc == 100);
	assert(ld.buffer);
	assert(ld.pos == ld.buffer);
	assert(ld.linenumber == 0);

	free_LineData(&ld);

	assert(ld.len == 0);
	assert(ld.alloc == 0);
	assert(ld.buffer == NULL);
	assert(ld.pos == ld.buffer);
	assert(ld.linenumber == 0);
}

/**
 * One newline character found at the beginning of the second buffer.
 */
static void test_load_LineData1()
{
	LineData_t ld;
	const char *buffer = NULL;
	const char *end_buffer, *pos_buffer;
	bool found_nl = false;

	init_LineData(&ld);

	buffer = "here is the start of a line of input to load line data";
	end_buffer = buffer + 54;

	found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);
	assert(!found_nl);

	assert(ld.alloc == 100);
	assert(ld.len == 54);
	assert(pos_buffer == end_buffer);

	buffer = "\nblarg glarb flarg klarf";
	end_buffer = buffer + 25;
	pos_buffer = buffer;

	found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);
	assert(pos_buffer == buffer);
	assert(found_nl);
	assert(ld.len = 54);
	assert(ld.alloc == 100);
	assert(strlen(ld.buffer) == ld.len);

	const char *expected = "here is the start of a line of input to load line data";
	assert(!memcmp(ld.buffer, expected, 55));

	free_LineData(&ld);
}

/**
 * A line that is exactly 100 characters long and requires an allocation of 1
 * byte for the null terminator.
 */
static void test_load_LineData100()
{
	LineData_t ld;
	const char *buffer = NULL;
	const char *end_buffer, *pos_buffer;
	bool found_nl = false;

	init_LineData(&ld);

	buffer = "part 1 of 2 strings to form a string that is 100  ";
	end_buffer = buffer + 50;

	found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);
	assert(!found_nl);

	assert(ld.alloc == 100);
	assert(ld.len == 50);
	assert(pos_buffer == end_buffer);

	buffer = "characters long. at the end of part 2 ze have a nl\n";
	end_buffer = buffer + 51;
	pos_buffer = buffer;

	found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);
	assert(pos_buffer == buffer + 50);
	assert(found_nl);
	assert(ld.len = 100);
	assert(ld.alloc == 101);
	assert(strlen(ld.buffer) == ld.len);

	const char *expected = "part 1 of 2 strings to form a string that is 100  "
		"characters long. at the end of part 2 ze have a nl\n";
	assert(!memcmp(ld.buffer, expected, 100));

	free_LineData(&ld);
}

/**
 * A line that is longer than what is considered reasonable and is thus
 * truncated.
 */
static void test_load_LineDataMAX()
{
	LineData_t ld;
	char *buffer = NULL;
	const char *end_buffer, *pos_buffer;
	bool found_nl = false;

	// "blargblogblargblog...blargblogdeadfood\n"
	const char *blurb = "blorgblog";
	char *w = NULL;
	buffer = malloc(sizeof(char) * MAX_ACCEPTABLE_LINE_LENGTH + 10);
	memset(buffer, '1', MAX_ACCEPTABLE_LINE_LENGTH + 9);
	buffer[MAX_ACCEPTABLE_LINE_LENGTH + 9] = '\0';
	end_buffer = buffer + MAX_ACCEPTABLE_LINE_LENGTH + 10;

	assert(strlen(buffer) == (MAX_ACCEPTABLE_LINE_LENGTH + 9));

	char *end_blurb = buffer + MAX_ACCEPTABLE_LINE_LENGTH;

	for(w = buffer; (w + 9) < end_blurb; w += 9)
	{
		memcpy(w, blurb, 9);
	}
	const char *deadfood = "deadf00d";
	w = end_blurb;
	memcpy(w, deadfood, 8);
	w += 8;
	*w++ = '\n';
	*w = '\0';

	assert(strlen(buffer) == (MAX_ACCEPTABLE_LINE_LENGTH + 9));
	assert(buffer[MAX_ACCEPTABLE_LINE_LENGTH + 9] == '\0');

	init_LineData(&ld);

	found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);
	assert(found_nl);

	assert(ld.alloc == MAX_ALLOC_LINE);
	assert(ld.len == MAX_ACCEPTABLE_LINE_LENGTH);
	assert(pos_buffer == end_blurb + 8);

	assert(!memcmp(ld.buffer, buffer, MAX_ACCEPTABLE_LINE_LENGTH));

	free(buffer);
	free_LineData(&ld);
}

/**
 * Input is "\n", a single newline character.
 */
static void test_load_LineDataLF()
{
	LineData_t ld;
	char *buffer = "\n";
	const char *end_buffer = buffer + 1;
	const char *pos_buffer = NULL;

	init_LineData(&ld);

	const bool found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);

	assert(!found_nl);
	assert(ld.alloc == 100);
	assert(ld.len == 0);
	// pos_buffer does not advance b/c the first character is \n and the load
	// line call does not eat newline characters. the responsibility to eat
	// characters is the caller.
	assert(pos_buffer == buffer);

	free_LineData(&ld);
}

static void test_load_LineDataCR()
{
	LineData_t ld;
	char *buffer = "\r";
	const char *end_buffer = buffer + 1;
	const char *pos_buffer = NULL;

	init_LineData(&ld);

	const bool found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);

	assert(!found_nl);
	assert(ld.alloc == 100);
	assert(ld.len == 0);
	// pos_buffer does not advance b/c the first character is \n and the load
	// line call does not eat newline characters. the responsibility to eat
	// characters is the caller.
	assert(pos_buffer == buffer);

	free_LineData(&ld);
}

/**
 * Input is a bunch of \n and \r mixed in
 */
static void test_load_LineDataCRLF()
{
	LineData_t ld;
	const char *buffer = "\r\n\n";
	const char *end_buffer = buffer + 3;
	const char *pos_buffer = NULL;

	init_LineData(&ld);

	const bool found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, false);

	assert(!found_nl);
	assert(ld.alloc == 100);
	assert(ld.len == 0);
	// pos_buffer does not advance b/c the first character is \n and the load
	// line call does not eat newline characters. the responsibility to eat
	// characters is the caller.
	assert(pos_buffer == buffer);

	free_LineData(&ld);
}

/**
 * Test skip functionality.
 */
static void test_load_LineDataSKIP()
{
	LineData_t ld;
	char *buffer = "skip\r";
	const char *end_buffer = buffer + 5;
	const char *pos_buffer = NULL;

	init_LineData(&ld);

	const bool found_nl = load_LineData(buffer, end_buffer, &pos_buffer, &ld, true);

	assert(found_nl);
	assert(ld.alloc == 100);
	assert(ld.len == 4);
	assert(pos_buffer == buffer + 4);

	free_LineData(&ld);
}

static void test_writeline()
{
	PortLineData_t pld;

	pld.data = "something,that,has,many,columns,breaking,pfb_insert,wildly";
	pld.linenumber = 10;
	pld.len = 58;

	FILE *f = fopen("tests/unit_pfb_prune/FileInput_1.work", "wb");
	writeline(f, &pld);
	fclose(f);

	char *buffer = malloc(sizeof(char) * 100);

	FILE *f_in = fopen("tests/unit_pfb_prune/FileInput_1.work", "rb");
	size_t read = fread(buffer, sizeof(char), 100, f_in);
	fclose(f_in);
	assert(read == pld.len + 1); // for the newline
	assert(!memcmp(pld.data, buffer, pld.len));
	assert(buffer[pld.len] == '\n');

	free(buffer);
}

void test_rw_pfb_csv()
{
	test_free_LineData();
	test_init_LineData();
	test_load_LineData1();
	test_load_LineData100();
	test_load_LineDataMAX();
	test_load_LineDataLF();
	test_load_LineDataCR();
	test_load_LineDataCRLF();
	test_load_LineDataSKIP();

	test_writeline();
}
#endif
