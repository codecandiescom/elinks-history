/* Global history dialogs */
/* $Id: globhist.c,v 1.28 2002/08/29 11:48:31 pasky Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "links.h"

#include <string.h>

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "dialogs/edit.h"
#include "dialogs/globhist.h"
#include "document/globhist.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "util/memory.h"
#include "util/string.h"


#ifdef GLOBHIST

#define HISTORY_BOX_IND 6

struct history_dialog_list_item {
	struct history_dialog_list_item *next;
	struct history_dialog_list_item *prev;
	struct dialog_data *dlg;
};

static struct list_head history_dialog_list = {
	&history_dialog_list,
	&history_dialog_list
};


void
update_all_history_dialogs(void)
{
	struct history_dialog_list_item *item;

	foreach (item, history_dialog_list) {
		display_dlg_item(item->dlg,
				 &(item->dlg->items[HISTORY_BOX_IND]), 1);
	}
}

/* Creates the box display (holds everything EXCEPT the actual rendering data) */
static struct listbox_data *
history_dialog_box_build()
{
	struct listbox_data *box;
	struct listbox_item *item;

	/* Deleted in abort */
	box = mem_alloc(sizeof(struct listbox_data));
	if (!box) return NULL;

	memset(box, 0, sizeof(struct listbox_data));
	box->items = &gh_box_items;
	foreach (item, *box->items) {
		item->data = box;
	}

	return box;
}


/* Cleans up after the history dialog */
static void
history_dialog_abort_handler(struct dialog_data *dlg)
{
	struct listbox_data *box;
	struct history_dialog_list_item *item;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	foreach (item, history_dialog_list) {
		if (item->dlg == dlg) {
			del_from_list(item);
			mem_free(item);
			break;
		}
	}

	mem_free(box);
}

/* Handles events for history dialog */
static int
history_dialog_event_handler(struct dialog_data *dlg, struct event *ev)
{
	struct widget_data *di;

	switch (ev->ev) {
		case EV_KBD:
			di = &dlg->items[dlg->selected];

			/* Catch change focus requests */
			if (ev->x == KBD_RIGHT || (ev->x == KBD_TAB && !ev->y)) {
				/* MP: dirty crap!!! this should be done in bfu.c */
				/* Move right */
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
				if (++dlg->selected >= HISTORY_BOX_IND)
					dlg->selected = 0;
				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_LEFT || (ev->x == KBD_TAB && ev->y)) {
				/* Move left */
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
				if (--dlg->selected < 0)
					dlg->selected = HISTORY_BOX_IND - 1;
				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);

				return EVENT_PROCESSED;
			}

			/* Moving the box */
			if (ev->x == KBD_DOWN) {
				box_sel_move(&dlg->items[HISTORY_BOX_IND], 1);
				display_dlg_item(dlg, &dlg->items[HISTORY_BOX_IND], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_UP) {
				box_sel_move(&dlg->items[HISTORY_BOX_IND], -1);
				display_dlg_item(dlg, &dlg->items[HISTORY_BOX_IND], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_DOWN) {
				box_sel_move(&dlg->items[HISTORY_BOX_IND],
					     dlg->items[HISTORY_BOX_IND].item->gid / 2);
				display_dlg_item(dlg, &dlg->items[HISTORY_BOX_IND], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_UP) {
				box_sel_move(&dlg->items[HISTORY_BOX_IND],
					     -dlg->items[HISTORY_BOX_IND].item->gid / 2);
				display_dlg_item(dlg, &dlg->items[HISTORY_BOX_IND], 1);

				return EVENT_PROCESSED;
			}

			/* Selecting a button */
			break;

		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
		case EV_MOUSE:
		case EV_ABORT:
			break;

		default:
			internal("Unknown event received: %d", ev->ev);
	}

	return EVENT_NOT_PROCESSED;
}

/* The titles to appear in the history dialog */
static unsigned char *history_dialog_msg[] = {
	TEXT(T_GLOBAL_HISTORY),
};

/* Called to setup the history dialog */
static void
layout_history_manager(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;

	/* Find dimensions of dialog */
	max_text_width(term, history_dialog_msg[0], &max);
	min_text_width(term, history_dialog_msg[0], &min);
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;

	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;

	if (w < 1)
		w = 1;

	w = rw = 50;

	y += 1;	/* Blankline between top and top of box */
	dlg_format_box(NULL, term, &dlg->items[HISTORY_BOX_IND], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y += 1;	/* Blankline between box and menu */
	dlg_format_buttons(NULL, term, dlg->items, HISTORY_BOX_IND, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;

	y++;
	dlg_format_box(term, term, &dlg->items[HISTORY_BOX_IND], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[0], HISTORY_BOX_IND, dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}


static void
history_search_do(struct dialog *d)
{
	struct listbox_data *box;
	struct dialog_data *parent;

	if (!d->items[0].data && !d->items[1].data)
		return;

	if (gh_last_searched_title) mem_free(gh_last_searched_title);
	gh_last_searched_title = stracpy(d->items[0].data);

	if (gh_last_searched_url) mem_free(gh_last_searched_url);
	gh_last_searched_url = stracpy(d->items[1].data);

	parent = d->udata;
	if (!parent) return;

	box = (struct listbox_data *)
	      parent->dlg->items[HISTORY_BOX_IND].data;
//	box->box_top = 0;
//	box->sel = 0;
//	history_dialog_list_update(&box->items);
}

static void
launch_search_dialog(struct terminal *term, struct dialog_data *parent,
		     struct session *ses)
{
	do_edit_dialog(term, TEXT(T_SEARCH_HISTORY), gh_last_searched_title,
		       gh_last_searched_url, ses, parent, history_search_do,
		       NULL, 0);
}

static int
push_search_button(struct dialog_data *dlg, struct widget_data *di)
{
	launch_search_dialog(dlg->win->term, dlg,
			     (struct session *) dlg->dlg->udata);
	return 0;
}


static int
push_goto_button(struct dialog_data *dlg, struct widget_data *goto_btn)
{
	struct global_history_item *historyitem;
	struct listbox_data *box;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	/* Follow the history item */
	historyitem = box->sel->udata;
	if (historyitem)
		goto_url((struct session *) goto_btn->item->udata,
			 historyitem->url);

	/* Close the history dialog */
	delete_window(dlg->win);
	return 0;
}


static int
push_delete_button(struct dialog_data *dlg,
		   struct widget_data *some_useless_delete_button)
{
	struct global_history_item *historyitem;
	struct terminal *term = dlg->win->term;
	struct listbox_data *box;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	historyitem = box->sel->udata;
	if (!historyitem)
		return 0;

	msg_box(term, NULL,
		TEXT(T_DELETE_HISTORY_ITEM), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_HISTORY_ITEM), " \"", historyitem->title, "\" ?\n\n",
		TEXT(T_URL), ": \"", historyitem->url, "\"", NULL,
		historyitem, 2,
		TEXT(T_YES), delete_global_history_item, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);

	return 0;
}


static void
really_clear_history(struct listbox_data *box)
{
	while (global_history.n) {
		delete_global_history_item(global_history.items.prev);
	}
	box->sel = NULL;
	box->top = NULL;
}

static int
push_clear_button(struct dialog_data *dlg,
		  struct widget_data *some_useless_clear_button)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	msg_box(term, NULL,
		TEXT(T_CLEAR_GLOBAL_HISTORY), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_CLEAR_GLOBAL_HISTORY), "?", NULL,
		box, 2,
		TEXT(T_YES), really_clear_history, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);

	return 0;
}

static int
push_info_button(struct dialog_data *dlg,
		  struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct global_history_item *historyitem;
	struct listbox_data *box;

	box = (struct listbox_data *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	/* Show history item info */
	historyitem = box->sel->udata;
	if (historyitem) {
		msg_box(term, NULL,
			TEXT(T_INFO), AL_LEFT | AL_EXTD_TEXT,
			TEXT(T_TITLE), ": ", historyitem->title, "\n",
			TEXT(T_URL), ": ", historyitem->url, "\n",
			TEXT(T_LAST_VISIT_TIME), ": ", ctime(&historyitem->last_visit), NULL,
			historyitem, 1,
			TEXT(T_OK), NULL, B_ESC | B_ENTER);
	}

	return 0;
}



void
menu_history_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog *d;
	struct dialog_data *dd;
	struct history_dialog_list_item *item;

	if (gh_last_searched_title) {
		mem_free(gh_last_searched_title);
		gh_last_searched_title = NULL;
	}

	if (gh_last_searched_url) {
		mem_free(gh_last_searched_url);
		gh_last_searched_url = NULL;
	}

#define DIALOG_MEMSIZE (sizeof(struct dialog) \
		       + (HISTORY_BOX_IND + 2) * sizeof(struct widget) \
		       + sizeof(struct global_history_item) + 2 * MAX_STR_LEN)
	/* XXX: sizeof(struct global_history_item): why? */

	d = mem_alloc(DIALOG_MEMSIZE);
	if (!d) return;

	memset(d, 0, DIALOG_MEMSIZE);

#undef DIALOG_MEMSIZE

	d->title = TEXT(T_HISTORY_MANAGER);
	d->fn = layout_history_manager;
	d->handle_event = history_dialog_event_handler;
	d->abort = history_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_goto_button;
	d->items[0].udata = ses;
	d->items[0].text = TEXT(T_GOTO);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_info_button;
	d->items[1].text = TEXT(T_INFO);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_delete_button;
	d->items[2].text = TEXT(T_DELETE);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = push_search_button;
	d->items[3].text = TEXT(T_SEARCH);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ENTER;
	d->items[4].fn = push_clear_button;
	d->items[4].text = TEXT(T_CLEAR);

	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ESC;
	d->items[5].fn = cancel_dialog;
	d->items[5].text = TEXT(T_CLOSE);

	d->items[HISTORY_BOX_IND].type = D_BOX;
	d->items[HISTORY_BOX_IND].gid = 12;
	d->items[HISTORY_BOX_IND].data = (void *) history_dialog_box_build();

	d->items[HISTORY_BOX_IND + 1].type = D_END;
	dd = do_dialog(term, d, getml(d, NULL));

	item = mem_alloc(sizeof(struct history_dialog_list_item));
	if (item) {
		item->dlg = dd;
		add_to_list(history_dialog_list, item);
	}
}

#else /* GLOBHIST */

void menu_history_manager(struct terminal *t, void *d, struct session *s) {}

#endif
