/* Global history */
/* $Id: globhist.c,v 1.3 2002/04/01 20:48:36 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include <stdio.h>
#include <string.h>

#include <bfu/bfu.h>
#include <dialogs/globhist.h>
#include <document/globhist.h>
#include <intl/language.h>
#include <lowlevel/kbd.h>


#define HISTORY_BOX_IND 3

static inline void
history_dialog_list_clear(struct list_head *list)
{
	struct box_item *item;

	foreach(item, *list) {
		free_global_history_item((struct global_history_item *)
					 item->data);
		mem_free(item->data);
	}

	free_list(*list);
}

static int
history_dialog_list_update(struct list_head *list)
{
	struct global_history_item *historyitem, *newhistoryitem;
	struct box_item	*item;
	int count = 0;

	/* Empty the list */
	history_dialog_list_clear(list);

	foreach(historyitem, global_history.items) {
		item = mem_alloc(sizeof(struct box_item)
				 + strlen(historyitem->url) + 1);
		if (!item) {
			return count;
		}

		item->text = ((unsigned char *) item + sizeof(struct box_item));

		newhistoryitem = mem_alloc(sizeof(struct global_history_item));
		if (!newhistoryitem) {
			mem_free(item);
			return count;
		}

		/* Wow, this is stupid. */
		newhistoryitem->last_visit = historyitem->last_visit;
		newhistoryitem->title = stracpy(historyitem->title);
		newhistoryitem->url = stracpy(historyitem->url);

		item->data = (void *) newhistoryitem;

		strcpy(item->text, historyitem->url);

		add_to_list(*list, item);
		count++;
	}
	return count;
}

/* Creates the box display (holds everything EXCEPT the actual rendering data) */
static struct dlg_data_item_data_box *
history_dialog_box_build(struct dlg_data_item_data_box **box)
{
	/* Deleted in abort */
	*box = mem_alloc(sizeof(struct dlg_data_item_data_box));
	if (!*box) return NULL;

	memset(*box, 0, sizeof(struct dlg_data_item_data_box));

	init_list((*box)->items);

	(*box)->list_len = history_dialog_list_update(&((*box)->items));
	return *box;
}

/* Get the id of the currently selected history */
static struct global_history_item *
history_dialogue_get_selected_history_item(struct dlg_data_item_data_box *box)
{
	struct box_item *citem;
	int sel = box->sel;

	if (sel == -1)
		return NULL;

	/* sel is an index into the history list. Therefore, we spin thru until
	 * sel equals zero, and return the id at that point. */
	foreach (citem, box->items) {
		if (sel == 0)
			return (struct global_history_item *) citem->data;
		sel--;
	}

	return NULL;
}


/* Cleans up after the history dialog */
static void
history_dialog_abort_handler(struct dialog_data *dlg)
{
	struct dlg_data_item_data_box *box;

	box = (struct dlg_data_item_data_box *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	history_dialog_list_clear(&(box->items));

	mem_free(box);
}

/* Handles events for history dialog */
static int
history_dialog_event_handler(struct dialog_data *dlg, struct event *ev)
{
	struct dialog_item_data *di;

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
				show_dlg_item_box(dlg, &dlg->items[HISTORY_BOX_IND]);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_UP) {
				box_sel_move(&dlg->items[HISTORY_BOX_IND], -1);
				show_dlg_item_box(dlg, &dlg->items[HISTORY_BOX_IND]);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_DOWN) {
				box_sel_move(&dlg->items[HISTORY_BOX_IND],
					     dlg->items[HISTORY_BOX_IND].item->gid / 2);
				show_dlg_item_box(dlg, &dlg->items[HISTORY_BOX_IND]);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_UP) {
				box_sel_move(&dlg->items[HISTORY_BOX_IND],
					     -dlg->items[HISTORY_BOX_IND].item->gid / 2);
				show_dlg_item_box(dlg, &dlg->items[HISTORY_BOX_IND]);

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
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct terminal *term;

	term = dlg->win->term;

	/* Find dimensions of dialog */
	max_text_width(term, history_dialog_msg[0], &max);
	min_text_width(term, history_dialog_msg[0], &min);
#if 0
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);
#endif

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


static int
push_goto_button(struct dialog_data *dlg, struct dialog_item_data *goto_btn)
{
	struct global_history_item *historyitem;
	struct dlg_data_item_data_box *box;

	box = (struct dlg_data_item_data_box *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	/* Follow the history item */
	historyitem = history_dialogue_get_selected_history_item(box);
	if (historyitem)
		goto_url((struct session *) goto_btn->item->udata,
			 historyitem->url);

	/* Close the history dialogue */
	delete_window(dlg->win);
	return 0;
}

/* Used to carry extra info between push_delete_button() and 
 * really_del_history() */
struct push_del_button_hop_struct {
	struct dialog *dlg;
	struct dlg_data_item_data_box *box;
	struct global_history_item *historyitem;
};

static void
really_del_history(void *vhop)
{
	struct push_del_button_hop_struct *hop;
	struct global_history_item *historyitem;
	int last;

	hop = (struct push_del_button_hop_struct *) vhop;

	historyitem = get_global_history_item(hop->historyitem->url,
					      hop->historyitem->title, 0);
	if (!historyitem)
		return;

	delete_global_history_item(historyitem);

	last = history_dialog_list_update(&hop->box->items);
	/* In case we deleted the last history item */
	if (hop->box->sel >= (last - 1))
		hop->box->sel = last - 1;

	/* Made in push_delete_button() */
	/* mem_free(vhop); */
}

static int
push_delete_button(struct dialog_data *dlg,
		   struct dialog_item_data *some_useless_delete_button)
{
	struct global_history_item *historyitem;
	struct push_del_button_hop_struct *hop;
	struct terminal *term;
	struct dlg_data_item_data_box *box;

	term = dlg->win->term;

	box = (struct dlg_data_item_data_box *)
	      dlg->dlg->items[HISTORY_BOX_IND].data;

	historyitem = history_dialogue_get_selected_history_item(box);
	if (!historyitem)
		return 0;

	/* Deleted in really_del_history() */
	hop = mem_alloc(sizeof(struct push_del_button_hop_struct));
	if (!hop)
		return 0;

	hop->historyitem = historyitem;
	hop->dlg = dlg->dlg;
	hop->box = box;

	msg_box(term, getml(hop, NULL),
		TEXT(T_DELETE_HISTORY_ITEM), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_HISTORY_ITEM), " \"", historyitem->title, "\" (",
		TEXT(T_URL), ": \"", historyitem->url, "\")?", NULL,
		hop, 2,
		TEXT(T_YES), really_del_history, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);

	return 0;
}


void
menu_history_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog *d;

#define DIALOG_MEMSIZE (sizeof(struct dialog) \
		       + (HISTORY_BOX_IND + 2) * sizeof(struct dialog_item) \
		       + sizeof(struct global_history_item) + 2 * MAX_STR_LEN)

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
	d->items[1].fn = push_delete_button;
	d->items[1].text = TEXT(T_DELETE);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ESC;
	d->items[2].fn = cancel_dialog;
	d->items[2].text = TEXT(T_CLOSE);

	d->items[HISTORY_BOX_IND].type = D_BOX;
	d->items[HISTORY_BOX_IND].gid = 12;
	history_dialog_box_build((struct dlg_data_item_data_box **)
				 &(d->items[HISTORY_BOX_IND].data));

	d->items[HISTORY_BOX_IND + 1].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}
