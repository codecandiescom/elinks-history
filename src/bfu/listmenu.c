/* List menus functions */
/* $Id: listmenu.c,v 1.17 2004/04/17 15:11:47 jonas Exp $ */

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
#include "util/math.h"
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

	foreach_menu_item (m, m->data)
		if (menu_contains(m, f))
			return 1;

	return 0;
}

void
do_select_submenu(struct terminal *term, struct menu_item *menu,
		  struct session *ses)
{
	struct menu_item *m;
	int def = int_max(0, get_current_state(ses));
	int sel = 0;

	foreach_menu_item (m, menu) {
		if (menu_contains(m, def)) {
			sel = m - menu;
			break;
		}
	}

	do_menu_selected(term, menu, ses, sel, 0);
}

void
new_menu_item(struct list_menu *menu, unsigned char *name, int data, int fullname)
	/* name == NULL - up;	data == -1 - down */
{
	struct menu_item *new_menu_item = NULL; /* no uninitialized warnings */

	if (!name) {
		menu->stack_size--;
		return;
	}

	clr_spaces(name);
	if (!name[0]) {
		mem_free(name);
		name = stracpy(" ");
		if (!name) return;
	}

	if (data == -1) {
		new_menu_item = mem_calloc(1, sizeof(struct menu_item));
		if (!new_menu_item) {
			mem_free(name);
			return;
		}
	}

	if (menu->stack_size) {
		struct menu_item *top, *item;
		size_t size = 0;

		top = menu->stack[menu->stack_size - 1];

		foreach_menu_item (item, top) size++;

		top = mem_realloc(top, (size + 2) * sizeof(struct menu_item));
		if (!top) {
			if (data == -1) mem_free(new_menu_item);
			mem_free(name);
			return;
		}

		item = top + size;
		menu->stack[menu->stack_size - 1] = top;
		if (menu->stack_size >= 2) {
			struct menu_item *below = menu->stack[menu->stack_size - 2];

			while (below->text) below++;
			below[-1].data = top;
		}

		if (data == -1) {
			SET_MENU_ITEM(item, name, NULL, ACT_MAIN_NONE, do_select_submenu,
				      new_menu_item, SUBMENU | NO_INTL,
				      0, 0);
		} else {
			SET_MENU_ITEM(item, name, NULL, ACT_MAIN_NONE, selected_item,
				      data, (fullname ? MENU_FULLNAME : 0) | NO_INTL,
				      0, 0);
		}

		item++;
		/* TODO: recheck that --Zas */
		memset(item, 0, sizeof(struct menu_item));

	} else mem_free(name);

	if (data == -1) {
		int size = (menu->stack_size + 1) * sizeof(struct menu_item *);
		struct menu_item **ms = mem_realloc(menu->stack, size);

		if (!ms) return;
		menu->stack = ms;
		menu->stack[menu->stack_size++] = new_menu_item;
	}
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

	foreach_menu_item (mm, m) {
		mem_free_if(mm->text);
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
menu_labels(struct menu_item *items, unsigned char *base, unsigned char **lbls)
{
	struct menu_item *item;
	unsigned char *bs;

	foreach_menu_item (item, items) {
		bs = (item->flags & MENU_FULLNAME) ? (unsigned char *)"" : base;
		bs = straconcat(bs, item->text, NULL);
		if (!bs) continue;

		if (item->func == (menu_func) do_select_submenu) {
			add_to_strn(&bs, " ");
			menu_labels(item->data, bs, lbls);
			mem_free(bs);
		} else {
			assert(item->func == (menu_func) selected_item);
			lbls[(int)item->data] = bs;
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
