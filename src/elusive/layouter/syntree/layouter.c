/* Raw syntax tree layouter */
/* $Id: layouter.c,v 1.12 2003/01/24 11:54:35 pasky Exp $ */

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


struct syntree_layouter_state {
	struct syntree_node *last_node;
};


static struct layout_box *
spawn_box(struct layouter_state *state)
{
	struct layout_box *box = init_layout_box();

	if (state->current->next && state->current != state->root)
		add_at_pos(state->current, box);
	else
		add_to_list(state->root->leafs, box);

	box->root = state->root;

	state->current = box;

	return box;
}


static void
format_node(struct layouter_state *state, struct syntree_node *node)
{
	struct layout_box_text *text;
	struct layout_box *box;
	struct property *property;

	/* TODO: Dream out some properties. --pasky */
	/* TODO: Then make it possible to tie the syntree output with some user
	 * CSS. --pasky */

	box = spawn_box(state);
	box->data_type = RECT_TEXT;
	box->data = text = mem_alloc(sizeof(struct layout_box_text));
	text->str = "[NODE] str :: ";
	text->len = 14;

	box = spawn_box(state);
	box->data_type = RECT_TEXT;
	box->data = text = mem_alloc(sizeof(struct layout_box_text));
	text->str = node->str;
	text->len = node->strlen;

	box = spawn_box(state);
	box->data_type = RECT_TEXT;
	box->data = text = mem_alloc(sizeof(struct layout_box_text));
	text->str = " :: special :: ";
	text->len = 15;

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
		text->str = " :: property :: ";
		text->len = 16;

		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text));
		text->str = property->name;
		text->len = property->namelen;

		box = spawn_box(state);
		box->data_type = RECT_TEXT;
		box->data = text = mem_alloc(sizeof(struct layout_box_text));
		text->str = " :: -> :: \"";
		text->len = 11;

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
}

static void
layout_node(struct layouter_state *state, struct syntree_node *orig_node)
{
	struct syntree_layouter_state *pdata = state->data;
	struct syntree_node *node = orig_node;
	struct list_head *leafs = node->root ? &node->root->leafs : NULL;

	/* XXX: We unfortunately cannot do this by an elegant recursion since
	 * it would be then *very* hard to implement resuming of layouting in
	 * the middle of tree :/. --pasky */

	do {
		struct layout_box *box;

		/* Mark the last node we processed, so that we can resume from
		 * here when we'll layout the next fragment. */
		pdata->last_node = node;

		box = spawn_box(state);
		add_property(&box->properties, "display", 7, "block", 5);
		add_property(&box->properties, "padding-left", 12, "2", 1);
		state->root = box;

		format_node(state, node);

		if (!list_empty(node->leafs)) {
			/* Descend to a lower level; keep root */
			leafs = &node->leafs;
			node = node->leafs.next;
			continue;
		}

		state->root = box->root;

go_on:

		if ((struct list_head *) node->next == leafs) {
			/* Last item in the list, travel upwards and further */

			/* XXX: assumes 1:1 box<->node hiearchy */
			state->current = state->root;
			state->root = state->root ? state->root->root : NULL;

			node = node->root;
			if (node) {
				if (node->root)
					leafs = &node->root->leafs;
				else
					leafs = NULL;
				pdata->last_node = node;
				goto go_on;
			}
			continue;
		}

		/* Next item in the list (or the root item!) */
		node = node->next;
	} while (node);
}


static void
syntree_init(struct layouter_state *state)
{
	state->parser_state = elusive_parser_init(state->parser);
	state->data = mem_calloc(1, sizeof(struct syntree_layouter_state));
}

static void
syntree_layout(struct layouter_state *state, unsigned char **str, int *len)
{
	struct syntree_layouter_state *pdata = state->data;

	elusive_parser_parse(state->parser_state, str, len);

	if (pdata->last_node) {
		/* Resume layouting */
		layout_node(state, pdata->last_node);
	} else {
		/* The first layout fragment */
		layout_node(state, state->parser_state->real_root);
	}
}

static void
syntree_done(struct layouter_state *state)
{
	mem_free(state->data);
	elusive_parser_done(state->parser_state);
}


struct layouter_backend syntree_layouter_backend = {
	syntree_init,
	syntree_layout,
	syntree_done,
};
