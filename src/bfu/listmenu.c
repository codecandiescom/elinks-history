/* List menus functions */
/* $Id: listmenu.c,v 1.2 2004/01/04 00:56:29 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "sched/session.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"
#include "viewer/text/link.h" /* get_current_state() */

/* TODO: massive cleanup, merging, code redundancy tracking between this file
 * and bfu/menu.c (and perhaps others.)
 * We should unify and clarify menu-related code. */

static int
menu_contains(struct menu_item *m, int f)
{
	if (m->func != (menu_func) do_select_submenu)
		return (int)m->data == f;
	for (m = m->data; m->text; m++)
		if (menu_contains(m, f))
			return 1;
	return 0;
}

void
do_select_submenu(struct terminal *term, struct menu_item *menu,
		  struct session *ses)
{
	struct menu_item *m;
	int def = get_current_state(ses);
	int sel = 0;

	if (def < 0) def = 0;
	for (m = menu; m->text; m++, sel++)
		if (menu_contains(m, def))
			goto found;
	sel = 0;

found:
	do_menu_selected(term, menu, ses, sel, 0);
}

void
new_menu_item(struct list_menu *menu, unsigned char *name, int data, int fullname)
	/* name == NULL - up;	data == -1 - down */
{
	struct menu_item *new_menu_item = NULL; /* no uninitialized warnings */

	if (name) {
		clr_spaces(name);
		if (!name[0]) mem_free(name), name = stracpy(" ");
	}

	if (name && data == -1) {
		new_menu_item = mem_calloc(1, sizeof(struct menu_item));
		if (!new_menu_item) {
			mem_free(name);
			return;
		}
	}

	if (name && menu->stack_size) {
		struct menu_item *top, *item;

		top = item = menu->stack[menu->stack_size - 1];
		while (item->text) item++;

		top = mem_realloc(top, (char *)(item + 2) - (char *)top);
		if (!top) {
			if (data == -1) mem_free(new_menu_item);
			mem_free(name);
			return;
		}
		item = item - menu->stack[menu->stack_size - 1] + top;
		menu->stack[menu->stack_size - 1] = top;
		if (menu->stack_size >= 2) {
			struct menu_item *below = menu->stack[menu->stack_size - 2];

			while (below->text) below++;
			below[-1].data = top;
		}

		if (data == -1) {
			SET_MENU_ITEM(item, name, NULL, ACT_NONE, do_select_submenu,
				      new_menu_item, SUBMENU | (fullname ? MENU_FULLNAME : 0) | NO_INTL,
				      0, 0);
		} else {
			SET_MENU_ITEM(item, name, NULL, ACT_NONE, selected_item,
				      data, (fullname ? MENU_FULLNAME : 0) | NO_INTL,
				      0, 0);
		}

		item++;
		memset(item, 0, sizeof(struct menu_item));

	} else if (name) mem_free(name);

	if (name && data == -1) {
		int size = (menu->stack_size + 1) * sizeof(struct menu_item *);
		struct menu_item **ms = mem_realloc(menu->stack, size);

		if (!ms) return;
		menu->stack = ms;
		menu->stack[menu->stack_size++] = new_menu_item;
	}

	if (!name) menu->stack_size--;
}

void
init_menu(struct list_menu *menu)
{
	menu->stack_size = 0;
	menu->stack = NULL;
	new_menu_item(menu, stracpy(""), -1, 0);
}

/* TODO: merge with free_menu_items() in bfu/menu.h --Zas */
void
free_menu(struct menu_item *m) /* Grrr. Recursion */
{
	struct menu_item *mm;

	if (!m) return; /* XXX: Who knows... need to be verified */

	for (mm = m; mm->text; mm++) {
		if (mm->text) mem_free(mm->text);
		if (mm->func == (menu_func) do_select_submenu) free_menu(mm->data);
	}
	mem_free(m);
}

struct menu_item *
detach_menu(struct list_menu *menu)
{
	struct menu_item *i = NULL;

	if (menu->stack) {
		if (menu->stack_size) i = menu->stack[0];
		mem_free(menu->stack);
	}

	return i;
}

void
destroy_menu(struct list_menu *menu)
{
	if (menu->stack) free_menu(menu->stack[0]);
	detach_menu(menu);
}

void
menu_labels(struct menu_item *m, unsigned char *base, unsigned char **lbls)
{
	unsigned char *bs;

	for (; m->text; m++) {
		if (m->func == (menu_func) do_select_submenu) {
			bs = stracpy(base);
			if (bs) {
				add_to_strn(&bs, m->text);
				add_to_strn(&bs, " ");
				menu_labels(m->data, bs, lbls);
				mem_free(bs);
			}
		} else {
			assert(m->func == (menu_func) selected_item);
			bs = stracpy((m->flags & MENU_FULLNAME)
				     ? (unsigned char *)"" : base);
			if (bs) add_to_strn(&bs, m->text);
			lbls[(int)m->data] = bs;
		}
	}
}

void
add_select_item(struct list_menu *menu, struct string *string,
		unsigned char **value, int order, int dont_add)
{
	assert(menu && string);

	if (!string->source) return;

	if (!value[order - 1])
		value[order - 1] = memacpy(string->source, string->length);

	if (dont_add) {
		done_string(string);
	} else {
		new_menu_item(menu, string->source, order - 1, 1);
		string->source = NULL;
		string->length = 0;
	}
}

