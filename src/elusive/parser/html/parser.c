/* Parser frontend */
/* $Id: parser.c,v 1.1 2002/12/27 14:59:56 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/html/parser.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/syntree.h"
#include "util/error.h"

enum state_code {
	HPT_PLAIN,
	HTP_ENTITY,
	HPT_NO,
};

struct html_parser_state {
	enum state_code state;
	/* We don't need a stack of states since the only possible ambiguity
	 * is (plain|attrval)->entity->?. */
	/* XXX: We update this only in (HTP_PLAIN,HTTP_ATTRVAL) -> HTP_ENTITY. */
	enum state_code prevstate;

#if 0
	union {
		unsigned char *tagname; /* HPT_TAG_NAME -> HPT_TAG_* */
		unsigned char *attrname; /* HPT_TAG_ATTR -> HTP_TAG_ATTRVAL */
		int comment_type; /* HPT_COMMENT */
	} data;
#endif
};


/* The scheme of state-specific parser subroutines gives us considerably better
 * large-block (state-wise blocks) performance, but it'll get worse with
 * heavily fragmented string with a lot of state changes, counting in the
 * function call overhead. But that should be really rare. */

/* TODO: We should investigate if it's ever possible to get into parser routine
 * with zero len - that'd give us some trouble. --pasky */


/* This just eats plain HTML text until it hits something neat. */
/* TODO: Do the text transformations (newlines eating, whitespaces compression,
 * ...). */
static int
plain_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	/* If we can't append ourselves to the current node, make up a new
	 * one for ourselves. */
	if (state->current->special != NODE_SPEC_TEXT ||
	    state->current->str + state->current->len != html) {
		struct syntree_node *node = init_syntree_node();

		node->special = NODE_SPEC_TEXT;
		node->str = html;

		node->root = state->root;
		add_at_pos(state->current, node);
		state->current = node;
	}

	while (html_len) {
		if (*html == '&') {
			state->current->strlen += *len - html_len;

			pstate->prevstate = HTP_PLAIN;
			pstate->state = HTP_ENTITY;
			*str = html, *len = html_len;
			return 0;
		}

		if (*html == '<') {
			state->current->strlen += *len - html_len;

			pstate->state = HTP_TAG;
			*str = html, *len = html_len;
			return 0;
		}

		html++, html_len--;
	}

	state->current->strlen += *len;
	*str = html, *len = html_len;
	return 1;
}

/* This tries to eat a HTML entity and add it as a node. */
/* XXX: Now this is in fact only variant of plain_parse() - the real code
 * is missing yet. */
static int
entity_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	html++, html_len--; /* & */

	while (html_len) {
		if (isA(*html)) {
			html++, html_len--;
			continue;
		}

		if (*html == ';')
			html++, html_len--;

		/* TODO: We don't burden ourselves with the codepages yet. We
		 * will have to UTF8 encode some sane form of a
		 * get_entity_string() call. */

		{
			struct syntree_node *node = init_syntree_node();

			node->special = NODE_SPEC_TEXT;
			node->str = *str;
			node->strlen = *len - html_len;

			node->root = state->root;
			add_at_pos(state->current, node);
			state->current = node;
		}

		pstate->state = pstate->prevstate;
		*str = html, *len = html_len;
		return 0;
	}

	/* Huh. Neverending entity? Try the next time. */
	return -1;
}

#if 0
/* This handles a sign of the allmighty tag. We have to make sure all the time
 * that we have the whole tag available. I'm pedantic about CPU time here, so
 * we will do it all one-pass. */
static int
tag_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;
	int name_len = 0;

	html++, html_len--; /* < */

	if (!html_len) return -1;

	/* Closing tag fun. */
	if (*html == '/') {
		html++, html_len--; /* / */
		while (name_len < html_len) {
			int end_bracket;

			if (isA(html[name_len])) {
				name_len++;
				continue;
			}

			end_bracket = name_len;
			while (end_bracket < html_len) {
				struct syntree_node *node = state->root;

				if (html[end_bracket] != '>') {
					end_bracket++;
					continue;
				}

				while (node) {
					/* XXX: We rely on the fact that all
					 * non-leaf nodes are tags here. */
					if (node->str &&
					    !strncmp(node->str, html,
						name_len > node->strlen
							? node->strlen
							: name_len))
						break;
					node = node->root;
				}

				if (node) {
					if (node->root) {
						state->current = node->root;
					} else {
						/* The root node is special. */
						state->current = node;
					}
					state->root = state->current->root;
				}

				*str = html + end_bracket, *len = html_len - end_bracket;
				return 0;
			}
			break;
		}
		return -1;
	}

	/* Comment fun...? */
	if (*html == '?' || *html == '!') {
		pstate->state = HPT_COMMENT;
		return 0;
	}

	while (name_len < html_len) {
		unsigned char *name = html;

		if (isA(html[name_len])) {
			name_len++;
			continue;
		}

		html += name_len;
		html_len -= name_len--;

		while (html_len) {
			if (!isA(*html)) {
				html++, html_len--;
				continue;
			}

			
		}
		break;
	}

	return -1;
}
#endif

/* State parser returns:
 * 1  on completion of the string (you can't rely on this not being 0, though),
 * 0  on change of the state,
 * -1 when we can't parse further but the string wasn't completed yet. */
static int (*state_parsers)(struct parser_state *, unsigned char *, int *)
								[HPT_NO] = {
	plain_parse,
	entity_parse,
};


static void
html_parser(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate;

	if (!state->data) {
		state->data = mem_calloc(1, sizeof(struct html_parser_state));
		if (!state->data)
			return;
	}
	pstate = state->data;

	while (html_len) {
		if (state_parsers[pstate->state](state, str, len) < 0) {
			*str = html;
			*len = html_len;
			return;
		}
	}
}


struct parser_backend html_backend = {
	html_parser,
};
