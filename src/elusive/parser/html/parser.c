/* Parser HTML backend */
/* $Id: parser.c,v 1.35 2003/07/23 02:28:50 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/html/parser.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/property.h"
#include "elusive/parser/stack.h"
#include "elusive/parser/syntree.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* We maintain the states (see below for a nice diagram) in a stack, maintained
 * by utility functions and structures below. */

enum html_state_code {
	HPT_PLAIN,
	HPT_ENTITY,
	HPT_TAG,
	HPT_TAG_WHITE,
	HPT_TAG_NAME,
	HPT_TAG_ATTR,
	HPT_TAG_ATTR_VAL,
	HPT_TAG_COMMENT,
	HPT_NO,
};

struct html_parser_state {
	struct parser_stack_item stack;

	union {
		/* HPT_PLAIN */
		struct {
			unsigned char *start;
		} text;
		/* HPT_TAG, HPT_TAG_NAME, HPT_TAG_COMMENT */
		struct {
			unsigned char *tagname;
			int taglen;
			/* / -> ending, !,? -> comment */
			unsigned char type;
		} tag;
		/* HPT_TAG_ATTR, HPT_TAG_ATTR_VAL */
		struct {
			unsigned char *attrname;
			int attrlen;
			int ate_eq;
		} attr;
	} data;
};

static inline struct html_parser_state *
html_state_push(struct parser_state *state, enum html_state_code state_code)
{
	return (struct html_parser_state *)
		state_stack_push(state, sizeof(struct html_parser_state),
				 state_code);
}

static inline struct html_parser_state *
html_state_pop(struct parser_state *state)
{
	return (struct html_parser_state *)
		state_stack_pop(state);
}


/* The scheme of state-specific parser subroutines gives us considerably better
 * large-block (state-wise blocks) performance, but it'll get worse with
 * heavily fragmented string with a lot of state changes, counting in the
 * function call overhead. But that should be really rare. */

/* We are built to be able to parse even small fragments of source at once,
 * thus we don't maintain per-function state in stack but generic state. That
 * brings some overhead, but I believe that it should be almost seamless. */

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
 * TAG +> TAG_NAME ( +> TAG_WHITE -> TAG_NAME )
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
static enum pstate_code
plain_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	while (html_len) {
		if (*html == '&') {
			pstate = html_state_push(state, HPT_ENTITY);
			*str = html, *len = html_len;
			return PSTATE_CHANGE;
		}

		if (*html == '<') {
			pstate = html_state_push(state, HPT_TAG);
			*str = html, *len = html_len;
			return PSTATE_CHANGE;
		}

		/* If we can't append ourselves to the current node, make up a
		 * new one for ourselves. We attempt to append only in the
		 * first pass - we can't do it sooner because we would spawn a
		 * lot of zero-length fragments. */
		if (html_len == *len &&
		    (state->current->special != NODE_SPEC_TEXT ||
		     state->current->str + state->current->strlen != html)) {
			spawn_syntree_node(state);

			state->current->special = NODE_SPEC_TEXT;
			state->current->str = html;
		}

		state->current->strlen++;
		html++, html_len--;
	}

	*str = html, *len = html_len;
	return PSTATE_COMPLETE;
}

/* This tries to eat a HTML entity and add it as a node. */
/* XXX: Now this is in fact only variant of plain_parse() - the real code
 * is missing yet. */
static enum pstate_code
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
		return PSTATE_CHANGE;
	}

	/* Huh. Neverending entity? Try the next time. */
	return PSTATE_SUSPEND;
}

/* This handles a sign of the allmighty tag, determines what the tag is about.
 * It also handles the tag's death. */
static enum pstate_code
tag_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	if (pstate->data.tag.tagname || pstate->data.tag.type) {
		/* We've parsed the whole tag and now we're at '>'. */

#ifdef DEBUG
		/* Hopefully. */
		if (*html != '>')
			internal("At HPT_TAG [2], *html is '%c'(%x)! That means trouble. Serious trouble.", *html, *html);
#endif

		/* We don't have anything to do for now. So just retire. */
		pstate = html_state_pop(state);
#ifdef DEBUG
		if (pstate->stack.state != HPT_PLAIN)
			internal("At HPT_TAG [2], pstate->stack.state is %d! That means corrupted HTML stack. Fear.", pstate->stack.state);
#endif

		html++, html_len--; /* > */
		*str = html, *len = html_len;
		return PSTATE_CHANGE;
	}

	html++, html_len--; /* < */

	if (!html_len) return PSTATE_SUSPEND;

	/* Closing tag fun! */
	if (*html == '/') {
		pstate->data.tag.type = *html;
		pstate = html_state_push(state, HPT_TAG_NAME);
		pstate->data.tag.type = *html;

		html++, html_len--;
		*str = html, *len = html_len;
		return PSTATE_CHANGE;
	}

	/* Comment fun...? */
	if (*html == '!' && html_len < 3) {
		/* We must be sure. */
		return PSTATE_SUSPEND;
	}
	if (*html == '?' || (*html == '!' && !strncmp(html + 1, "--", 2))) {
		/* The next time we'll visit you, we will be at the end of our
		 * way. */
		pstate->data.tag.type = *html;
		pstate = html_state_push(state, HPT_TAG_COMMENT);
		pstate->data.tag.type = *html;

		if (*html == '!') {
			/* Skip -- as well. */
			html += 2, html_len -= 2;
		}
		html++, html_len--;
		*str = html, *len = html_len;
		return PSTATE_CHANGE;
	}

	/* Ordinary tag. *yawn* */
	pstate = html_state_push(state, HPT_TAG_NAME);
	/* Let's try to save one parser turn: */
	if (WHITECHAR(*html)) {
		pstate = html_state_push(state, HPT_TAG_WHITE);
	}

	*str = html, *len = html_len;
	return PSTATE_CHANGE;
}

/* This skips sequence of whitespaces inside of a tag. */
static enum pstate_code
tag_white_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	while (html_len) {
		if (WHITECHAR(*html)) {
			html++, html_len--;
			continue;
		}

		pstate = html_state_pop(state);
		*str = html, *len = html_len;
		return PSTATE_CHANGE;
	}

	*str = html, *len = html_len;
	return PSTATE_SUSPEND;
}

/* This eats name of the tag. Also, it maintains the corresponding struct
 * syntree_node. */
static enum pstate_code
tag_name_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	if (WHITECHAR(*html)) {
		pstate = html_state_push(state, HPT_TAG_WHITE);
		return PSTATE_CHANGE;
	}

	while (html_len) {
		int name_len;

		if (isA(*html)) {
			html++, html_len--;
			continue;
		}

		pstate = html_state_pop(state);

		name_len = *len - html_len;
		if (name_len) {
			/* Non-empty tag name. */
			if (pstate->data.tag.type == '/') {
				/* Closing tag */
				struct syntree_node *node = state->root;

				while (node) {
					/* XXX: We rely on the fact that all
					 * non-leaf nodes are tags here. */
					if (node->str
					    && node->strlen == name_len
					    && !strncasecmp(node->str, *str,
								name_len))
						break;
					node = node->root;
				}

				if (node) {
					state->current = node;
					state->root = node->root;
				}

			} else {
				/* Opening tag */

				spawn_syntree_node(state);

				state->current->str = *str;
				state->current->strlen = name_len;
				if (**str != '!') {
					/* TODO: Look up the DTD whether the
					 * tag is pair or not. */
					state->root = state->current;
				}
				pstate->data.tag.tagname = *str;
				pstate->data.tag.taglen = name_len;
			}
		}

		pstate = html_state_push(state, HPT_TAG_ATTR);
		if (WHITECHAR(*html)) {
			pstate = html_state_push(state, HPT_TAG_WHITE);
		}
		*str = html, *len = html_len;
		return PSTATE_CHANGE;
	}

	return PSTATE_SUSPEND;
}

/* This eats tag attributes names. */
static enum pstate_code
tag_attr_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;

	if (WHITECHAR(*html)) {
		pstate = html_state_push(state, HPT_TAG_WHITE);
		return PSTATE_CHANGE;
	}

	if (*html == '>') {
		pstate = html_state_pop(state);
		return PSTATE_CHANGE;
	}

	while (html_len) {
		int name_len;

		if (isA(*html)) {
			html++, html_len--;
			continue;
		}

		name_len = *len - html_len;
		if (name_len) {
			pstate->data.attr.attrname = *str;
			pstate->data.attr.attrlen = name_len;
			pstate = html_state_push(state, HPT_TAG_ATTR_VAL);
			pstate->data.attr.attrname = *str;
			pstate->data.attr.attrlen = name_len;
			if (WHITECHAR(*html))
				pstate = html_state_push(state, HPT_TAG_WHITE);

		} else {
			if (*html == '/') {
				/* The tag isn't paired. */
				/* FIXME: We should match this only at the end
				 * of the tag, I think. --pasky */
				if (state->root == state->current)
					state->root = state->current->root;
			}
			/* TODO: State switch or while () here. --pasky */
			html++, html_len--;
		}

		*str = html, *len = html_len;
		return PSTATE_CHANGE;
	}

	return PSTATE_SUSPEND;
}

/* This eats tag value, if there is any. It also adds the attribute to the
 * syntax tree node (as a property). */
/* TODO: Parse entities inside of the attribute values! --pasky */
static enum pstate_code
tag_attr_val_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;
	unsigned char quoted = 0;
	unsigned char *attr; int attr_len;

	if (WHITECHAR(*html)) {
		pstate = html_state_push(state, HPT_TAG_WHITE);
		return PSTATE_CHANGE;
	}

	if (*html == '>') {
		/* Shortcut. */
		pstate = html_state_pop(state);
	}
	if (!pstate->data.attr.ate_eq && *html != '=') {
		add_property(&state->current->properties,
			pstate->data.attr.attrname, pstate->data.attr.attrlen,
			html, 0);
		pstate = html_state_pop(state);
		return PSTATE_CHANGE;
	}

	if (!pstate->data.attr.ate_eq) {
		html++, html_len--; /* = */
		pstate->data.attr.ate_eq = 1;
		if (!html_len) {
			*str = html, *len = html_len;
			return PSTATE_CHANGE;
		}
		if (WHITECHAR(*html)) {
			pstate = html_state_push(state, HPT_TAG_WHITE);
			*str = html, *len = html_len;
			return PSTATE_CHANGE;
		}
	}

	/* FIXME: @quoted persistence over suspends? --pasky */

	if (*html == '"' || *html == '\'') {
		quoted = *html;
		html++, html_len--;
	}

	attr = html, attr_len = html_len;

	while (html_len) {
		if ((!WHITECHAR(*html) && *html != '>')
		    && (!quoted || *html != quoted)) {
			html++, html_len--;
			continue;
		}

		pstate = html_state_pop(state);

		add_property(&state->current->properties,
			pstate->data.attr.attrname, pstate->data.attr.attrlen,
			attr, attr_len - html_len);

		if (quoted)
			html++, html_len--;

		*str = html, *len = html_len;
		return PSTATE_CHANGE;
	}

	return PSTATE_SUSPEND;
}

/* Walk through a comment towards the light. */
static enum pstate_code
comment_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;
	unsigned char *html = *str;
	int html_len = *len;
	unsigned char *end = "-->";
	int endp = 0;

	while (html_len) {
		if (pstate->data.tag.type == '?' && *html == '?') {
			if (html_len == 1) {
				*str = html, *len = html_len;
				return PSTATE_SUSPEND;
			}
			if (html[1] == '>') {
				html++, html_len--;

comment_end:
				/* TODO: Maybe skip directly the '>' and jump
				 * straightly over HPT_TAG ? But we can't be
				 * sure what we'll want to do in HPT_TAG [2]
				 * in the future. --pasky */
				pstate = html_state_pop(state);

				*str = html, *len = html_len;
				return PSTATE_CHANGE;
			}

		} else if (pstate->data.tag.type == '!' && *html == end[endp]) {
			if (!endp) {
				/* Checkpoint. */
				*str = html, *len = html_len;
			}
			endp++;
			if (endp == 3) {
				goto comment_end;
			}

		} else if (endp) {
			endp = 0;
		}

		html++, html_len--;
	}

	if (endp) {
		return PSTATE_SUSPEND;
	} else {
		*str = html, *len = html_len;
		return PSTATE_COMPLETE;
	}
}

/* State parser returns:
 * 1  on completion of the string (you can't rely on this not being 0, though),
 * 0  on change of the state,
 * -1 when we can't parse further but the string wasn't completed yet. */
typedef int (*parse_func)(struct parser_state *, unsigned char **, int *);

static parse_func state_parsers[HPT_NO] = {
	plain_parse,
	entity_parse,
	tag_parse,
	tag_white_parse,
	tag_name_parse,
	tag_attr_parse,
	tag_attr_val_parse,
	comment_parse,
};


static void
html_init(struct parser_state *state)
{
	html_state_push(state, HPT_PLAIN);
}

static void
html_parse(struct parser_state *state, unsigned char **str, int *len)
{
	struct html_parser_state *pstate = state->data;

	while (*len) {
		/* debug("parsing [%d] ::%s::", pstate->stack.state, *str); */
		if (state_parsers[pstate->stack.state](state, str, len) < 0) {
			return;
		}
		pstate = state->data; /* update to the top of the stack */
	}
}

static void
html_done(struct parser_state *state)
{
	while (state->data)
		html_state_pop(state);
}


struct parser_backend html_parser_backend = {
	html_init,
	html_parse,
	html_done,
};
