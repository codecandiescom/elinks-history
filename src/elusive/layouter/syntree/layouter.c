/* Raw syntax tree layouter */
/* $Id: layouter.c,v 1.4 2002/12/31 17:13:04 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "elusive/layouter/syntree/layouter.h"
#include "elusive/parser/attrib.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/syntree.h"
#include "util/memory.h"


/* TODO: Support for incremental layouting! --pasky */


static struct layout_rectangle *
spawn_rect(struct layouter_state *state)
{
	struct layout_rectangle *rect = init_layout_rectangle();

	if (state->current->next && state->current != state->root)
		add_at_pos(state->current, rect);
	else
		add_to_list(state->root->leafs, rect);

	rect->root = state->root;

	return rect;
}

static void
layout_node(struct layouter_state *state, struct syntree_node *node)
{
	struct layout_rectangle_text *text;
	struct layout_rectangle *rect;
	struct attribute *attrib;
	struct syntree_node *leaf;

	/* TODO: Dream out some attributes. --pasky */
	/* TODO: Then make it possible to tie the syntree output with some user
	 * CSS. --pasky */

	rect = spawn_rect(state);
	rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
	text->str = "[NODE] ";
	text->len = 7;

	rect = spawn_rect(state);
	rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
	text->str = node->str;
	text->len = node->strlen;

	rect = spawn_rect(state);
	rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
	text->str = " ";
	text->len = 1;

	{
		unsigned char numbuf[64];
		int numbuflen;

		snprintf(numbuf, 64, "%d", node->special);
		numbuflen = strlen(numbuf) + 1;

		rect = spawn_rect(state);
		rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text) + numbuflen);
		text->str = rect->data + sizeof(struct layout_rectangle_text);
		text->len = numbuflen - 1;
		memcpy(text->str, numbuf, numbuflen);
	}

	foreach (attrib, node->attrs) {
		rect = spawn_rect(state);
		rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
		text->str = " ";
		text->len = 1;

		rect = spawn_rect(state);
		rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
		text->str = attrib->name;
		text->len = attrib->namelen;

		rect = spawn_rect(state);
		rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
		text->str = "->\"";
		text->len = 3;

		rect = spawn_rect(state);
		rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
		text->str = attrib->value;
		text->len = attrib->valuelen;

		rect = spawn_rect(state);
		rect->data = text = mem_alloc(sizeof(struct layout_rectangle_text));
		text->str = "\"";
		text->len = 1;
	}

	foreach (leaf, node->leafs) {
		rect = spawn_rect(state);
		state->root = rect;
		add_attrib(state->current->attrs, "display", 7, "block", 5);
		layout_node(state, leaf);
	}
}


static void
syntree_init(struct layouter_state *state)
{
	state->parser_state = elusive_parser_init(state->parser);
}

static void
syntree_layout(struct layouter_state *state, unsigned char **str, int *len)
{
	struct syntree_node *node;

	elusive_parser_parse(state->parser_state, str, len);

	node = state->parser_state->real_root;
	add_attrib(state->current->attrs, "display", 7, "block", 5);
	layout_node(state, node);
}

static void
syntree_done(struct layouter_state *state)
{
	elusive_parser_done(state->parser_state);
}


struct layouter_backend syntree_layouter_backend = {
	syntree_init,
	syntree_layout,
	syntree_done,
};
