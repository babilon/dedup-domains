#include "carry_over.h"

/**
 * Increase the array of linenumber_t by one.
 */
static linenumber_t alloc_carry_over(carry_over_t *co)
{
	ASSERT(co);

	// new length of the array
	const linenumber_t count = len_carry_over(co) + 1;

	// plus 1 to account for index 0 which holds the size/length of the array.
	// this extra 1 is not included in the length of the array.
	linenumber_t *tmp = realloc(co->linenumbers, sizeof(linenumber_t) * (count + 1));

	if(tmp)
	{
		co->linenumbers = tmp;
		co->linenumbers[0] = count;
		ADD_CC;
	}
	else
	{
		ELOG_STDERR("ERROR: failed to realloc carry_over[]\n");
		exit(EXIT_FAILURE);
	}

	ADD_CC;
	return co->linenumbers[0];
}

/**
 * Initialize the given carry_over_t to a default state.
 * Calls to free_carry_over() are safe after calling this.
 */
void init_carry_over(carry_over_t *co)
{
	ASSERT(co);
	co->linenumbers = NULL;
	ADD_CC;
}

/**
 * Release memory allocated by calls to alloc_carry_over().
 */
void free_carry_over(carry_over_t *co)
{
	ASSERT(co);
	free(co->linenumbers);
	co->linenumbers = NULL;
	ADD_CC;
}

/**
 * Move the contents of the internal array of linenumber_t to the given array of
 * linenumber_t. This will free the internal array of the given carry_over_t.
 */
void transfer_linenumbers(linenumber_t *dest_linenumbers, carry_over_t *co)
{
	ASSERT(dest_linenumbers);
	ASSERT(co);
	linenumber_t count = len_carry_over(co);

	if(count == 0)
	{
		// gracefully handle a bogus request
		ADD_CC;
		return;
	}

	memmove(dest_linenumbers, co->linenumbers + 1,
			sizeof(linenumber_t) * len_carry_over(co));

	free_carry_over(co);
	ADD_CC;
}

/**
 * Returns the number of elements in the internal array. This is safe to call
 * after init_carry_over() and after free_carry_over().
 */
linenumber_t len_carry_over(carry_over_t *co)
{
	ASSERT(co);
	if(co->linenumbers)
	{
		ADD_CC;
		return co->linenumbers[0];
	}

	ADD_CC;
	return 0;
}

/**
 * Appends to the end of the internal array the given linenumber_t.
 *
 * This is safe to call after init_carry_over().
 */
void insert_carry_over(carry_over_t *co, linenumber_t ln)
{
	ASSERT(co);
	ASSERT(ln > 0);

	const linenumber_t index = alloc_carry_over(co);

	// first element value is at [1]. the length/size of array is at [0].
	ASSERT(index >= 1);
	ASSERT(co->linenumbers);
	ASSERT(index <= co->linenumbers[0]);
	co->linenumbers[index] = ln;
	ADD_CC;
}

#ifdef BUILD_TESTS
static void test_init_carry_over()
{
	carry_over_t co, co_zero;
	memset(&co, 0xf, sizeof(carry_over_t));
	memset(&co_zero, 0, sizeof(carry_over_t));

	// legal to call free on a zero'd carry_over_t
	free_carry_over(&co_zero);

	init_carry_over(&co);

	assert(0 == memcmp(&co, &co_zero, sizeof(carry_over_t)));

	free_carry_over(&co);

	assert(0 == memcmp(&co, &co_zero, sizeof(carry_over_t)));

	// legal to call free on a free'd carry_over_t
	free_carry_over(&co);

	ADD_TCC;
}

static void test_len_carry_over()
{
	carry_over_t co;
	init_carry_over(&co);
	assert(0 == len_carry_over(&co));

	insert_carry_over(&co, 33);

	assert(1 == len_carry_over(&co));

	free_carry_over(&co);

	assert(0 == len_carry_over(&co));

	ADD_TCC;
}

static void test_insert_carry_over()
{
	carry_over_t co;
	init_carry_over(&co);

	insert_carry_over(&co, 3);
	insert_carry_over(&co, 33);
	insert_carry_over(&co, 2);
	insert_carry_over(&co, 22);

	assert(4 == len_carry_over(&co));

	assert(co.linenumbers[0] == 4);
	assert(co.linenumbers[1] == 3);
	assert(co.linenumbers[2] == 33);
	assert(co.linenumbers[3] == 2);
	assert(co.linenumbers[4] == 22);

	free_carry_over(&co);

	assert(0 == len_carry_over(&co));
	ADD_TCC;
}

void test_transfer()
{
	carry_over_t co;
	init_carry_over(&co);

	insert_carry_over(&co, 101);
	insert_carry_over(&co, 202);
	insert_carry_over(&co, 303);
	insert_carry_over(&co, 404);
	insert_carry_over(&co, 505);

	const linenumber_t count = len_carry_over(&co);
	assert(count == 5);
	linenumber_t *xfered = calloc(count, sizeof(linenumber_t));
	transfer_linenumbers(xfered, &co);

	free_carry_over(&co);

	assert(len_carry_over(&co) == 0);

	for(size_t i = 0; i < count; i++)
	{
		assert(xfered[i] == ((i + 1) * 101));
	}

	free(xfered);
	ADD_TCC;
}

void test_carry_over()
{
	test_init_carry_over();
	test_len_carry_over();
	test_insert_carry_over();
	test_transfer();

	ADD_TCC;
}
#endif
