/* $Id: stack.h,v 1.1 2003/01/01 18:58:13 pasky Exp $ */

#ifndef EL__USIVE_PARSER_STACK_H
#define EL__USIVE_PARSER_STACK_H

/* This is generic parser stack toolkit. It will allow you to manage a stack of
 * states... ie. for 'asdf&ent;<foo bar="baz&ent;">':
 * 
 * asdf   PLAIN
 * &ent;  PLAIN ENTITY
 * <      PLAIN TAG
 * foo    PLAIN TAG TAG_NAME
 *        PLAIN TAG TAG_ATTR TAG_WHITESPACE
 * bar    PLAIN TAG TAG_ATTR
 * ="baz  PLAIN TAG TAG_ATTR TAG_VALUE
 * &ent;  PLAIN TAG TAG_ATTR TAG_VALUE ENTITY
 * "      PLAIN TAG ATG_ATTR TAG_VALUE
 * >      PLAIN TAG
 *
 * So that you will be able to reuse some states at various places of the state
 * tree. In order to use the stack toolkit, insert *AT THE TOP* of your parser
 * state structure (that one stored in parser_state.data) the stack structure
 * (struct parser_stack_item) - inline, not as a pointer. See the XML parser
 * for an example. */

#include "elusive/parser/parser.h"

struct parser_stack_item {
	/* Ehm, this points downwards in the stack :^). */
	struct parser_stack_item *up;
	/* You should have some enum for this. */
	int state;
};

/* This will insert a new state at the top of the stack. It returns pointer to
 * the new state or NULL if the insert failed (the stack is not modified then).
 * You can safely cast the return value to pointer to your parser state
 * structure. */
/* state_size indicates the size of your parser state structure - the newly
 * created parser_stack_item will be of that size. */
struct parser_stack_item *
state_stack_push(struct parser_state *state, size_t state_size, int state_code);

/* This will remove a state from the top of the stack. It returns pointer to
 * the new state at the top of the stack or NULL if the remove failed (the
 * stack is not modified then). You can safely cast the return value to pointer
 * to your parser state structure. */
struct parser_stack_item *
state_stack_pop(struct parser_state *state);

#endif
