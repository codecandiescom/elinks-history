/* Internal bookmarks support */
/* $Id: dialogs.c,v 1.31 2002/09/12 17:07:54 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "document/session.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "intl/language.h"
#include "util/error.h"
#include "util/memory.h"


/* Whether to save bookmarks after each modification of their list
 * (add/modify/delete). */
#define BOOKMARKS_RESAVE	1

/* The location of the box in the bookmark manager */
#define	BM_BOX_IND		6


#ifdef BOOKMARKS


/****************************************************************************
  Bookmark manager stuff.
****************************************************************************/

/* Creates the box display (holds everything EXCEPT the actual rendering
 * data) */
static struct listbox_data *
bookmark_dlg_box_build()
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_alloc(sizeof(struct listbox_data));
	if (!box) return NULL;

	memset(box, 0, sizeof(struct listbox_data));
	box->order = 1;
	box->items = &bookmark_box_items;
	add_to_list(bookmark_boxes, box);

	return box;
}


/* Cleans up after the bookmark dialog */
void
bookmark_dialog_abort_handler(struct dialog_data *dlg)
{
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[BM_BOX_IND].data;

	del_from_list(box);
	/* Delete the box structure */
	mem_free(box);
}


/* Handles events for a bookmark dialog */
int
bookmark_dialog_event_handler(struct dialog_data *dlg, struct event *ev)
{
	struct widget_data *di;

	switch (ev->ev) {
		case EV_KBD:
			di = &dlg->items[dlg->selected];
			/* Catch change focus requests */
			/* MP: dirty crap!!! This should be done in bfu.c! */

			if (ev->x == KBD_RIGHT
			    || (ev->x == KBD_TAB && !ev->y)) {
				/* Move right */
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
				if (++dlg->selected >= BM_BOX_IND)
					dlg->selected = 0;
				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_LEFT
			    || (ev->x == KBD_TAB && ev->y)) {
				/* Move left */
				display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
				if (--dlg->selected < 0)
					dlg->selected = BM_BOX_IND - 1;
				display_dlg_item(dlg, &dlg->items[dlg->selected], 1);

				return EVENT_PROCESSED;
			}

			/* Moving the box */
			if (ev->x == KBD_DOWN) {
				box_sel_move(&dlg->items[BM_BOX_IND], 1);
				display_dlg_item(dlg, &dlg->items[BM_BOX_IND], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_UP) {
				box_sel_move(&dlg->items[BM_BOX_IND], -1);
				display_dlg_item(dlg, &dlg->items[BM_BOX_IND], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_DOWN) {
				box_sel_move(&dlg->items[BM_BOX_IND],
					     dlg->items[BM_BOX_IND].item->gid / 2);
				display_dlg_item(dlg, &dlg->items[BM_BOX_IND], 1);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_UP) {
				box_sel_move(&dlg->items[BM_BOX_IND],
					     -dlg->items[BM_BOX_IND].item->gid / 2);
				display_dlg_item(dlg, &dlg->items[BM_BOX_IND], 1);

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


/* The titles to appear in the bookmark add dialog */
unsigned char *bookmark_add_msg[] = {
	TEXT(T_BOOKMARK_TITLE),
	TEXT(T_URL),
};


/* The titles to appear in the bookmark manager */
unsigned char *bookmark_dialog_msg[] = {
	TEXT(T_BOOKMARKS),
};


/* Called to setup the bookmark dialog */
void
layout_bookmark_manager(struct dialog_data *dlg)
{
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct terminal *term;

	term = dlg->win->term;

	/* Find dimensions of dialog */
	max_text_width(term, bookmark_dialog_msg[0], &max);
	min_text_width(term, bookmark_dialog_msg[0], &min);
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
	dlg_format_box(NULL, term, &dlg->items[BM_BOX_IND], dlg->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y += 1;	/* Blankline between box and menu */
	dlg_format_buttons(NULL, term, dlg->items, BM_BOX_IND, 0,
			   &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;

	y++;
	dlg_format_box(term, term, &dlg->items[BM_BOX_IND], dlg->x + DIALOG_LB,
		       &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[0], BM_BOX_IND,
			   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}


void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *,
			      struct session *);


/* Callback for the "add" button in the bookmark manager */
int
push_add_button(struct dialog_data *dlg, struct widget_data *di)
{
	launch_bm_add_doc_dialog(dlg->win->term, dlg,
				 (struct session *) dlg->dlg->udata);
	return 0;
}


void launch_bm_search_doc_dialog(struct terminal *, struct dialog_data *,
				 struct session *);


/* Callback for the "search" button in the bookmark manager */
int
push_search_button(struct dialog_data *dlg, struct widget_data *di)
{
	launch_bm_search_doc_dialog(dlg->win->term, dlg,
				    (struct session *) dlg->dlg->udata);
	return 0;
}


/* Called when the goto button is pushed */
int
push_goto_button(struct dialog_data *dlg, struct widget_data *goto_btn)
{
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[BM_BOX_IND].data;

	/* Follow the bookmark */
	if (box->sel)
		goto_url((struct session *) goto_btn->item->udata,
			 ((struct bookmark *) box->sel->udata)->url);

	/* Close the bookmark dialog */
	delete_window(dlg->win);
	return 0;
}


/* Called when an edit is complete. */
void
bookmark_edit_done(struct dialog *d) {
	struct bookmark *bm = (struct bookmark *) d->udata2;

	update_bookmark(bm, d->items[0].data, d->items[1].data);
	bm->refcount--;

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

void
bookmark_edit_cancel(struct dialog *d) {
	struct bookmark *bm = (struct bookmark *) d->udata2;

	bm->refcount--;
}

/* Called when the edit button is pushed */
int
push_edit_button(struct dialog_data *dlg, struct widget_data *edit_btn)
{
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[BM_BOX_IND].data;

	/* Follow the bookmark */
	if (box->sel) {
		struct bookmark *bm = (struct bookmark *) box->sel->udata;
		const unsigned char *name = bm->title;
		const unsigned char *url = bm->url;

		bm->refcount++;
		do_edit_dialog(dlg->win->term, TEXT(T_EDIT_BOOKMARK), name, url,
			       (struct session *) edit_btn->item->udata, dlg,
			       bookmark_edit_done, bookmark_edit_cancel,
			       (void *) bm, 1);
	}

	return 0;
}


/* Used to carry extra info between the push_delete_button() and the
 * really_del_bookmark() */
struct push_del_button_hop_struct {
	struct terminal *term;
	struct dialog *dlg;
	struct bookmark *bm;
};


/* Called to _really_ delete a bookmark (a confirm in the delete dialog) */
void
really_del_bookmark(void *vhop)
{
	struct push_del_button_hop_struct *hop;
	struct listbox_data *box;

	hop = (struct push_del_button_hop_struct *) vhop;
	hop->bm->refcount--;

	if (hop->bm->refcount > 0) {
		msg_box(hop->term, NULL,
			TEXT(T_DELETE_BOOKMARK), AL_CENTER,
			TEXT(T_BOOKMARK_USED),
			NULL, 1,
			TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
		return;
	}

	/* Take care about move of the selected item if we're deleting it. */

	foreach (box, *hop->bm->box_item->box) {

	/* Please. Don't. Reindent. This. Ask. Why. --pasky */

	/* Remember that we have to take the reverse direction here in
	 * traverse(), as we have different sort order than usual. */

	if (box->sel && box->sel->udata == hop->bm) {
		struct bookmark *bm = (struct bookmark *) box->sel->udata;

		box->sel = traverse_listbox_items_list(bm->box_item, -1,
				1, NULL, NULL);
		if (bm->box_item == box->sel)
			box->sel = traverse_listbox_items_list(bm->box_item, 1,
					1, NULL, NULL);
		if (bm->box_item == box->sel)
			box->sel = NULL;
	}

	if (box->top && box->top->udata == hop->bm) {
		struct bookmark *bm = (struct bookmark *) box->top->udata;

		box->top = traverse_listbox_items_list(bm->box_item, -1,
				1, NULL, NULL);
		if (bm->box_item == box->top)
			box->top = traverse_listbox_items_list(bm->box_item, 1,
					1, NULL, NULL);
		if (bm->box_item == box->top)
			box->top = NULL;
	}

	}

	if (!delete_bookmark(hop->bm))
		return;

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

void
cancel_del_bookmark(void *vhop)
{	struct push_del_button_hop_struct *hop;

	hop = (struct push_del_button_hop_struct *) vhop;
	hop->bm->refcount--;
}

/* Callback for the "delete" button in the bookmark manager */
int
push_delete_button(struct dialog_data *dlg,
		   struct widget_data *some_useless_delete_button)
{
	struct bookmark *bm;
	struct push_del_button_hop_struct *hop;
	struct terminal *term;
	struct listbox_data *box;

	/* FIXME There's probably a nicer way to do this */
	term = dlg->win->term;

	box = (struct listbox_data *) dlg->dlg->items[BM_BOX_IND].data;

	if (!box->sel) return 0;
	bm = (struct bookmark *) box->sel->udata;
	if (!bm) return 0;


	/* Deleted in really_del_bookmark() */
	hop = mem_alloc(sizeof(struct push_del_button_hop_struct));
	if (!hop) return 0;

	hop->bm = bm;
	hop->dlg = dlg->dlg;
	hop->term = term;

	bm->refcount++;

	msg_box(term, getml(hop, NULL),
		TEXT(T_DELETE_BOOKMARK), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_BOOKMARK), " \"", bm->title, "\" ?\n\n",
		TEXT(T_URL), ": \"", bm->url, "\"", NULL,
		hop, 2,
		TEXT(T_YES), really_del_bookmark, B_ENTER,
		TEXT(T_NO), cancel_del_bookmark, B_ESC);

	return 0;
}


/* Builds the "Bookmark manager" dialog */
void
menu_bookmark_manager(struct terminal *term, void *fcp, struct session *ses)
{
#define BM_DIALOG_MEMSIZE (sizeof(struct dialog) \
		           + (BM_BOX_IND + 2) * sizeof(struct widget) \
			   + sizeof(struct bookmark) + 2 * MAX_STR_LEN)

	struct bookmark *new_bm;
	struct dialog *d;

	/* Show all bookmarks */
	foreach (new_bm, bookmarks) {
		new_bm->box_item->visible = 1;
	}

	/* Reset momorized search criterias */
	if (bm_last_searched_name) {
		mem_free(bm_last_searched_name);
		bm_last_searched_name = NULL;
	}

	if (bm_last_searched_url) {
		mem_free(bm_last_searched_url);
		bm_last_searched_url = NULL;
	}

	/* Create the dialog */
	d = mem_alloc(BM_DIALOG_MEMSIZE);
	if (!d) return;

	memset(d, 0, BM_DIALOG_MEMSIZE);

#undef BM_DIALOG_MEMSIZE

	d->title = TEXT(T_BOOKMARK_MANAGER);
	d->fn = layout_bookmark_manager;
	d->handle_event = bookmark_dialog_event_handler;
	d->abort = bookmark_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_goto_button;
	d->items[0].udata = ses;
	d->items[0].text = TEXT(T_GOTO);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_edit_button;
	d->items[1].udata = ses;
	d->items[1].text = TEXT(T_EDIT);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_delete_button;
	d->items[2].text = TEXT(T_DELETE);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = push_add_button;
	d->items[3].text = TEXT(T_ADD);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ENTER;
	d->items[4].fn = push_search_button;
	d->items[4].text = TEXT(T_SEARCH);

	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ESC;
	d->items[5].fn = cancel_dialog;
	d->items[5].text = TEXT(T_CLOSE);

	d->items[BM_BOX_IND].type = D_BOX;
	d->items[BM_BOX_IND].gid = 12;
	d->items[BM_BOX_IND].data = (void *) bookmark_dlg_box_build();

	d->items[BM_BOX_IND + 1].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}



/****************************************************************************\
  Bookmark add dialog.
\****************************************************************************/

/* Adds the bookmark */
void
bookmark_add_add(struct dialog *d)
{
	struct bookmark *bm;
	struct listbox_data *box;

	bm = add_bookmark(NULL, d->items[0].data, d->items[1].data);
	foreach (box, *bm->box_item->box) {
		box->sel = bm->box_item;

		/* FIXME: This is *BAD*, we should find some way how to extend
		 * the chain of references to gid of the box to this struct
		 * dialog *, so that we can update the box->top properly by the
		 * traverse(). --pasky */

		box->top = bm->box_item;
	}

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}


/* Search bookmarks */
void
bookmark_search_do(struct dialog *d)
{
	struct listbox_item *item = bookmark_box_items.prev;
	struct listbox_data *box;

	if (!bookmark_simple_search(d->items[1].data, d->items[0].data)) return;
	if (list_empty(bookmark_box_items)) return;

	foreach (box, *item->box) {
		box->top = item;
		box->sel = box->top;
	}
}


/* launch_bm_add_doc_dialog() */
void
launch_bm_add_doc_dialog(struct terminal *term,
			 struct dialog_data *parent,
			 struct session *ses)
{
	do_edit_dialog(term, TEXT(T_ADD_BOOKMARK), NULL, NULL,
		       ses, parent, bookmark_add_add, NULL, NULL, 1);
}


/* launch_bm_search_doc_dialog() */
void
launch_bm_search_doc_dialog(struct terminal *term,
			    struct dialog_data *parent,
			    struct session *ses)
{
	do_edit_dialog(term, TEXT(T_SEARCH_BOOKMARK),
		       bm_last_searched_name, bm_last_searched_url,
		       ses, parent, bookmark_search_do, NULL, NULL, 0);
}


/* Called to launch an add dialog on the current link */
void
launch_bm_add_link_dialog(struct terminal *term,
			  struct dialog_data *parent,
			  struct session *ses)
{
	unsigned char url[MAX_STR_LEN];

	do_edit_dialog(term, TEXT(T_ADD_BOOKMARK), NULL,
		       get_current_link_url(ses, url, MAX_STR_LEN), ses,
		       parent, bookmark_add_add, NULL, NULL, 1);
}

#else /* BOOKMARKS */

void menu_bookmark_manager(struct terminal *t, void *d, struct session *s) {}
void launch_bm_add_doc_dialog(struct terminal *t, struct dialog_data *d, struct session *s) {}
void launch_bm_add_link_dialog(struct terminal *t, struct dialog_data *d, struct session *s) {}

#endif
