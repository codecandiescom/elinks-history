/* Parser frontend */
/* $Id: parser.c,v 1.1 2002/12/26 02:46:10 pasky Exp $ */

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
elusive_parser(enum parser_backend_type parser, struct syntree_node **root,
		unsigned char **str, int *len)
{
	if (parser_backends[parser] && parser_backends[parser]->parse)
		parser_backends[parser]->parse(root, str, len);
}
