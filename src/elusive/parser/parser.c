/* Parser frontend */
/* $Id: parser.c,v 1.2 2002/12/27 00:01:20 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/parser.h"
#include "elusive/parser/syntree.h"


struct parser_backend *parser_backends[] = {
	NULL,
	NULL,
	NULL,
};

void
elusive_parser(enum parser_backend_type parser, struct parser_state **state,
		unsigned char **str, int *len)
{
	if (!parser_backends[parser] || !parser_backends[parser]->parse)
		return;

	if (!*state) {
		*state = mem_calloc(1, sizeof(struct parser_state));
		if (!*state)
			return; /* Try next time, buddy. */

		(*state)->root = init_syntree_node(NULL);
		(*state)->current = (*state)->root;
	}

	parser_backends[parser]->parse(*state, str, len);
}
