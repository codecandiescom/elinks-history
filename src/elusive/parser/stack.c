/* Parser stack toolkit */
/* $Id: stack.c,v 1.2 2003/01/19 18:00:27 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/parser.h"
#include "elusive/parser/stack.h"
#include "util/error.h"
#include "util/memory.h"


struct parser_stack_item *
state_stack_push(struct parser_state *state, size_t state_size, int state_code)
{
	struct parser_stack_item *item;

	/* debug("state_stack_push [%d]", state_code); */

	item = mem_calloc(1, state_size);
	if (!item) return NULL;

	item->up = state->data;
	state->data = item;
	item->state = state_code;
	return item;
}

struct parser_stack_item *
state_stack_pop(struct parser_state *state)
{
	struct parser_stack_item *item = state->data;

	if (!item) {
		internal("Parser state stack underflow!");
		return NULL;
	}

	/* debug("state_stack_pop [%d]", item->state); */

	state->data = item->up;
	mem_free(item);
	return state->data;
}

struct parser_stack_item *
state_stack_repush(struct parser_state *state, size_t state_size, int state_code)
{
	struct parser_stack_item *item = state->data;
	struct parser_stack_item *up;

	if (!item) {
		internal("Parser state stack underflow!");
		return NULL;
	}

	/* debug("state_stack_repush [%d]", state_code); */

	up = item->up;
	memset(item, 0, state_size);

	item->up = up;
	item->state = state_code;
	return item;
}
