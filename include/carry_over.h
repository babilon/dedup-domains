#ifndef CARRY_OVER_H
#define CARRY_OVER_H
#include "dedupdomains.h"

typedef struct carry_over
{
	linenumber_t *linenumbers;
} carry_over_t;

extern void insert_carry_over(carry_over_t *co, linenumber_t ln);
extern linenumber_t len_carry_over(carry_over_t *co);
extern void init_carry_over(carry_over_t *co);
extern void free_carry_over(carry_over_t *co);
extern void transfer_linenumbers(linenumber_t *dest_linenumbers, carry_over_t *co);

#endif
