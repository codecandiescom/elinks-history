/* Parser frontend */
/* $Id: parser.c,v 1.7 2002/12/30 02:04:45 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/parser.h"
#include "elusive/parser/syntree.h"
#include "util/memory.h"


#include "elusive/parser/html/parser.h"

struct parser_backend *parser_backends[] = {
	NULL,
	NULL,
	&html_backend,
};


struct parser_state *
elusive_init(enum parser_backend_type parser)
{
	struct parser_state *state;

	state = mem_calloc(1, sizeof(struct parser_state));
	if (!state) return NULL;

	state->real_root = init_syntree_node();
	state->current = state->root = state->real_root;
	state->parser = parser;

	if (parser_backends[state->parser] &&
	    parser_backends[state->parser]->init)
		parser_backends[state->parser]->init(state);

	return state;
}

void
elusive_parse(struct parser_state *state, unsigned char **str, int *len)
{
	if (!parser_backends[state->parser] ||
	    !parser_backends[state->parser]->parse)
		return;

	parser_backends[state->parser]->parse(state, str, len);
}

void
elusive_done(struct parser_state *state)
{
	if (parser_backends[state->parser] &&
	    parser_backends[state->parser]->done)
		parser_backends[state->parser]->done(state);

	done_syntree_node(state->real_root);
	mem_free(state);
}
