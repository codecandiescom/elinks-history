/* Raw syntax tree layouter */
/* $Id: layouter.c,v 1.7 2003/01/18 00:36:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "elusive/layouter/syntree/layouter.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/property.h"
#include "elusive/parser/syntree.h"
#include "util/memory.h"


/* TODO: Support for incremental layouting! --pasky */


static struct layout_box *
spawn_box(struct layouter_state *state)
{
	struct layout_box *box = init_layout_box();

	if (state->current->next && state->current != state->root)
		add_at_pos(state->current, box);
	else
		add_to_list(state->root->leafs, box);

	box->root = state->root;

	return box;
}

static void
layout_node(struct layouter_state *state, struct syntree_node *node)
{
	struct layout_box_text *text;
	struct layout_box *box;
	struct property *property;
	struct syntree_node *leaf;

	/* TODO: Dream out some properties. --pasky */
	/* TODO: Then make it possible to tie the syntree output with some user
	 * CSS. --pasky */

	box = spawn_box(state);
	box->data_type = RECT_TEXT;
	box->data = text = mem_alloc(sizeof(struct layout_box_text));
	text->str = "[NODE] ";
	text->len = 7;

	box = spawn_box(state);
	box->data_type = RECT_TEXT;
	box->data = text = mem_alloc(sizeof(struct layout_box_text));
	text->str = node->str;
	text->len = node->strlen;

	box = spawn_box(state);
	box->data_type = RECT_TEXT;
	box->data = text = mem_alloc(sizeof(struct layout_box_text));
	text->str = " ";
	text->len = 1;

	{
		unsigned char numbuf[64];
		int numbuflen;

		snprintf(numbuf, 64, "%d", node->special);
		numbuflen = strlen(numbuf) + 1;

		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text) + numbuflen);
		text->str = box->data + sizeof(struct layout_box_text);
		text->len = numbuflen - 1;
		memcpy(text->str, numbuf, numbuflen);
	}

	foreach (property, node->properties) {
		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text));
		text->str = " ";
		text->len = 1;

		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text));
		text->str = property->name;
		text->len = property->namelen;

		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text));
		text->str = "->\"";
		text->len = 3;

		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text));
		text->str = property->value;
		text->len = property->valuelen;

		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text));
		text->str = "\"";
		text->len = 1;
	}

	foreach (leaf, node->leafs) {
		struct layout_box *root_box = state->root;

		box = spawn_box(state);
		state->root = box;
		add_property(&state->current->properties, "display", 7, "block", 5);
		layout_node(state, leaf);
		state->root = root_box;
		state->current = box;
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
	add_property(&state->current->properties, "display", 7, "block", 5);
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
