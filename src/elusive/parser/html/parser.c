/* Parser frontend */
/* $Id: parser.c,v 1.4 2002/12/27 22:31:51 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/html/parser.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/syntree.h"
#include "util/error.h"


/* TODO: Unicode...? --pasky */
#define whitespace(x) (x <= 32)


/* We maintain the states (see below for a nice diagram) in a stack, maintained
 * by utility functions and structures below. */

enum state_code {
	HPT_PLAIN,
	HTP_ENTITY,
	HPT_TAG,
	HPT_NO,
};

struct html_parser_state {
	struct html_parser_state *up;

	enum state_code state;

	union {
		/* HPT_TAG */
		struct {
			unsigned char *tagname;
			int taglen;
			/* / -> ending, !,? -> comment */
			unsigned char type;
		} tag;
		/* HPT_TAG_ATTR */
		struct {
			unsigned char *attrname;
			int attrlen;
		} attr;
	} data;
};

static struct html_parser_state *
html_state_push(struct parser_state *state, enum state_code state_code)
{
	struct html_parser_state *pstate;

	pstate = mem_calloc(1, sizeof(struct html_parser_state));
	if (!pstate) return NULL;

	pstate->up = state->data;
	state->data = pstate;
	pstate->state = state_code;
	return pstate;
}

static struct html_parser_state *
html_state_pop(struct parser_state *state)
{
	struct html_parser_state *pstate = state->data;

	if (!pstate) {
		internal("HTML state stack underflow!");
		return;
	}
	state->data = pstate->up;
	mem_free(pstate);
	return state->data;
}


static struct syntree_node *
spawn_syntree_node(struct parser_state *state)
{
	struct syntree_node *node = init_syntree_node();

	node->root = state->root;
	if (state->root != state->current) {
		add_at_pos(state->current, node);
	} else {
		/* We've spawned non-leaf node right before. So we will fit
		 * under it (not along it) nicely. */
		add_to_list(state->root, node);
	}
	state->current = node;

	return node;
}


/* The scheme of state-specific parser subroutines gives us considerably better
 * large-block (state-wise blocks) performance, but it'll get worse with
 * heavily fragmented string with a lot of state changes, counting in the
 * function call overhead. But that should be really rare. */

/* TODO: We should investigate if it's ever possible to get into parser routine
 * with zero len - that'd give us some trouble. --pasky */


/* The state machine scheme (don't confuse grammar and syntax - the state stack
 * doesn't depend on the elements inheritance, just on the markups).
 *
 * +> is adding of STATE to the stack
 * -> is removing of ourselves from the stack and STATE is on the top
 * => is exchanging of ourselves and STATE on the top of the stack
 * passes in parenthesis happen without intermediate state parser calls or they
 * are otherwise "hidden"
 *
 * PLAIN +> TAG
 * PLAIN +> ENTITY
 * ENTITY -> PLAIN
 *
 * TAG -> PLAIN
 * TAG +> TAG_COMMENT
 * TAG +> TAG_NAME
 *
 * TAG_COMMENT -> TAG
 *
 * TAG_NAME => TAG_ATTR ( +> TAG_WHITE -> TAG_ATTR )
 * TAG_NAME -> TAG [2]
 *
 * TAG_ATTR +> TAG_ATTR_VAL
 * TAG_ATTR => TAG_ATTR ( +> TAG_WHITE -> TAG_ATTR )
 * TAG_ATTR -> TAG [2]
 *
 * TAG_ATTR_VAL +> ENTITY
 * TAG_ATTR_VAL -> TAG_ATTR ( +> TAG_WHITE -> TAG_ATTR )
 * TAG_ATTR_VAL -> ( TAG_ATTR -> ) TAG [2]
 *
 * ENTITY -> PLAIN
 * ENTITY -> TAG_ATTR_VAL
 *
 * TAG [2] is TAG with non-NULL tagname.
 */


/* This just eats plain HTML text until it hits something neat. */
/* TODO: Do the text transformations (newlines eating, whitespaces compression,
 * ...). */
static int
plain_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	/* TODO: Don't create zero-length text nodes, if the pass will be at
	 * the first char. */

	/* If we can't append ourselves to the current node, make up a new
	 * one for ourselves. */
	if (state->current->special != NODE_SPEC_TEXT ||
	    state->current->str + state->current->len != html) {
		spawn_syntree_node(state);

		state->current->special = NODE_SPEC_TEXT;
		state->current->str = html;
	}

	while (html_len) {
		if (*html == '&') {
			state->current->strlen += *len - html_len;

			pstate = html_state_push(state, HTP_ENTITY);
			*str = html, *len = html_len;
			return 0;
		}

#if 0
		if (*html == '<') {
			state->current->strlen += *len - html_len;

			pstate = html_state_push(state, HTP_TAG);
			*str = html, *len = html_len;
			return 0;
		}
#endif

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
		/* We aren't so strict and don't require the entity to end by
		 * a semicolon if we can be sure that it's over already. */

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
			spawn_syntree_node(state);

			state->current->special = NODE_SPEC_TEXT;
			state->current->str = *str;
			state->current->strlen = *len - html_len;
		}

		pstate = html_state_pop(state);
		*str = html, *len = html_len;
		return 0;
	}

	/* Huh. Neverending entity? Try the next time. */
	return -1;
}

/* This handles a sign of the allmighty tag, determines what the tag is about. */
static int
tag_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;
	int name_len = 0;

	if (pstate->data.tag.tagname) {
		/* We've parsed the whole tag and now we're at '>'. */
		/* We don't have anything to do for now. So just retire. */
		pstate = html_state_pop(state);
#ifdef DEBUG
		if (pstate->state != HTP_PLAIN)
			internal("At HTP_TAG [2], pstate->state is %d! That means corrupted HTML stack. Fear.", pstate->state);
#endif

		html++, html_len--; /* > */
		*str = html, *len = html_len;
		return 0;
	}

	html++, html_len--; /* < */

	if (!html_len) return -1;

	/* Closing tag fun! */
	if (*html == '/') {
		pstate->data.tag.type = *html;
		pstate = html_state_push(state, HPT_TAG_NAME);

		html++, html_len--;
		*str = html, *len = html_len;
		return 0;
	}

	/* Comment fun...? */
	if (*html == '!' && html_len < 3) {
		/* We must be sure. */
		return -1;
	}
	if (*html == '?' || (*html == '!' && !strncmp(html + 1, "--", 2))) {
		pstate->data.tag.type = *html;
		pstate = html_state_push(state, HPT_TAG_COMMENT);
		pstate->data.tag.type = *html;

		html++, html_len--;
		*str = html, *len = html_len;
		return 0;
	}

	/* Ordinary tag. *yawn* */
	pstate = html_state_push(state, HPT_TAG_NAME);
	/* Let's save one parser turn: */
	if (whitespace(*html)) {
		pstate = html_state_push(state, HPT_TAG_WHITE);
	}

	return 0;
}

#if 0
/* This handles a sign of the allmighty tag, determines what the tag is about. */
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
	tag_parse,
};


static void
html_parser(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;

	if (!pstate) pstate = html_state_push(state, HTP_PLAIN);
	if (!pstate) return;

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
