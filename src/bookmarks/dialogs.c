/* Internal bookmarks support */
/* $Id: dialogs.c,v 1.8 2002/05/08 13:55:01 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/bfu.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "document/session.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "intl/language.h"
#include "util/error.h"


/* Whether to save bookmarks after each modification of their list
 * (add/modify/delete). */
#define BOOKMARKS_RESAVE	1

/* The location of the box in the bookmark manager */
#define	BM_BOX_IND		6


#ifdef BOOKMARKS


/****************************************************************************
  Bookmark manager stuff.
****************************************************************************/

/* Clears the bookmark list from the bookmark_dialog */
static inline void
bookmark_dlg_list_clear(struct list_head *bm_list)
{
	free_list(*bm_list);
}


/* Updates the bookmark list for a dialog. Returns the number of bookmarks.
 * FIXME: Must be changed for hierarchical bookmarks. */
int
bookmark_dlg_list_update(struct list_head *bm_list)
{
	struct bookmark *bm;	/* Iterator over bm list */
	struct box_item	*item;	/* New box item (one per displayed bookmark) */
	unsigned char *text;
	int count = 0;
	bookmark_id id;

	/* Empty the list */
	bookmark_dlg_list_clear(bm_list);

	/* Copy each bookmark into the display list */
	foreach(bm, bookmarks) if (bm->selected) {
		/* Deleted in bookmark_dlg_clear_list() */
		item = mem_alloc( sizeof(struct box_item) + strlen(bm->title)
			+ 1);
		if (!item) return count;

		item->text = text = ((unsigned char *)item + sizeof(struct box_item));
		item->data = (void *)(id = bm->id);

		/* Note that free_i is left at zero */
		/* XXX: ??? --Zas */

		strcpy(text, bm->title);

		add_to_list( *bm_list, item);
		count++;
	}

	return count;
}


/* Creates the box display (holds everything EXCEPT the actual rendering
 * data) */
struct dlg_data_item_data_box *
bookmark_dlg_box_build(struct dlg_data_item_data_box **box)
{
	/* Deleted in abort */
	*box = mem_alloc(sizeof(struct dlg_data_item_data_box));
	if (!*box) return NULL;

	memset(*box, 0, sizeof(struct dlg_data_item_data_box));

	init_list((*box)->items);

	(*box)->list_len = bookmark_dlg_list_update(&((*box)->items));

	return *box;
}

/* Get the id of the currently selected bookmark */
bookmark_id
bookmark_dlg_box_id_get(struct dlg_data_item_data_box *box)
{
	struct box_item *citem;
	int sel = box->sel;

	if (sel == -1) return BAD_BOOKMARK_ID;

	/* Sel is an index into the list of bookmarks. Therefore, we spin thru
	 * until sel equals zero, and return the id at that point. */
	foreach(citem, box->items) {
		if (!sel) return (bookmark_id) citem->data;
		sel--;
	}

	return BAD_BOOKMARK_ID;
}


/* Cleans up after the bookmark dialog */
void
bookmark_dialog_abort_handler(struct dialog_data *dlg)
{
	struct dlg_data_item_data_box *box;

	box = (struct dlg_data_item_data_box *)(dlg->dlg->items[BM_BOX_IND].data);

	/* Zap the display list */
	bookmark_dlg_list_clear(&(box->items));

	/* Delete the box structure */
	mem_free(box);
}


/* Handles events for a bookmark dialog */
int
bookmark_dialog_event_handler(struct dialog_data *dlg, struct event *ev)
{
	struct dialog_item_data *di;

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
				show_dlg_item_box(dlg, &dlg->items[BM_BOX_IND]);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_UP) {
				box_sel_move(&dlg->items[BM_BOX_IND], -1);
				show_dlg_item_box(dlg, &dlg->items[BM_BOX_IND]);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_DOWN) {
				box_sel_move(&dlg->items[BM_BOX_IND],
					     dlg->items[BM_BOX_IND].item->gid / 2);
				show_dlg_item_box(dlg, &dlg->items[BM_BOX_IND]);

				return EVENT_PROCESSED;
			}

			if (ev->x == KBD_PAGE_UP) {
				box_sel_move(&dlg->items[BM_BOX_IND],
					     -dlg->items[BM_BOX_IND].item->gid / 2);
				show_dlg_item_box(dlg, &dlg->items[BM_BOX_IND]);

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
push_add_button(struct dialog_data *dlg, struct dialog_item_data *di)
{
	launch_bm_add_doc_dialog(dlg->win->term, dlg,
				 (struct session *) dlg->dlg->udata);
	return 0;
}


void launch_bm_search_doc_dialog(struct terminal *, struct dialog_data *,
				 struct session *);


/* Callback for the "search" button in the bookmark manager */
int
push_search_button(struct dialog_data *dlg, struct dialog_item_data *di)
{
	launch_bm_search_doc_dialog(dlg->win->term, dlg,
				    (struct session *) dlg->dlg->udata);
	return 0;
}


/* Called when the goto button is pushed */
int
push_goto_button(struct dialog_data *dlg, struct dialog_item_data *goto_btn)
{
	bookmark_id id;
	struct dlg_data_item_data_box *box;

	box = (struct dlg_data_item_data_box*)(dlg->dlg->items[BM_BOX_IND].data);

	/* Follow the bookmark */
	id = bookmark_dlg_box_id_get(box);
	if (id != BAD_BOOKMARK_ID)
		goto_url((struct session *) goto_btn->item->udata,
			 get_bookmark_by_id(id)->url);

	/* FIXME: There really should be some feedback to the user here. */

	/* Close the bookmark dialog */
	delete_window(dlg->win);
	return 0;
}


/* Called when an edit is complete. */
void
bookmark_edit_done(struct dialog *d) {
	bookmark_id id = (bookmark_id)d->udata2;
	struct dialog_data *parent;

	update_bookmark(id, d->items[0].data, d->items[1].data);

	parent = d->udata;

	/* Tell the bookmark dialog to redraw */
	if (parent)
		bookmark_dlg_list_update(
			&((struct dlg_data_item_data_box *)
			  parent->dlg->items[BM_BOX_IND].data)->items);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}


/* Called when the edit button is pushed */
int
push_edit_button(struct dialog_data *dlg,
		 struct dialog_item_data *edit_btn)
{
	bookmark_id id;
	struct dlg_data_item_data_box *box;

	box = (struct dlg_data_item_data_box*)(dlg->dlg->items[BM_BOX_IND].data);

	/* Follow the bookmark */
	id = bookmark_dlg_box_id_get(box);
	if (id != BAD_BOOKMARK_ID) {
		const unsigned char *name = get_bookmark_by_id(id)->title;
		const unsigned char *url = get_bookmark_by_id(id)->url;

		do_edit_dialog(dlg->win->term, TEXT(T_EDIT_BOOKMARK), name, url,
			       (struct session *) edit_btn->item->udata, dlg,
			       bookmark_edit_done, (void *) id, 1);
	}
	/* FIXME There really should be some feedback to the user here */
	return 0;
}


/* Used to carry extra info between the push_delete_button() and the
 * really_del_bookmark() */
struct push_del_button_hop_struct {
	struct dialog *dlg;
	struct dlg_data_item_data_box *box;
	bookmark_id id;
};


/* Called to _really_ delete a bookmark (a confirm in the delete dialog) */
void
really_del_bookmark(void *vhop)
{
	struct push_del_button_hop_struct *hop;
	int last;

	hop = (struct push_del_button_hop_struct *)vhop;

	if (!delete_bookmark_by_id(hop->id))
		return;

	last = bookmark_dlg_list_update(&(hop->box->items)) - 1;

	/* In case we deleted the last bookmark */
	if (hop->box->sel >= last)
		hop->box->sel = last;

	/* Made in push_delete_button() */
	/*mem_free(vhop);*/

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}


/* Callback for the "delete" button in the bookmark manager */
int
push_delete_button(struct dialog_data *dlg,
		   struct dialog_item_data *some_useless_delete_button)
{
	struct bookmark *bm;
	struct push_del_button_hop_struct *hop;
	struct terminal *term;
	struct dlg_data_item_data_box *box;

	/* FIXME There's probably a nicer way to do this */
	term = dlg->win->term;

	box = (struct dlg_data_item_data_box*)(dlg->dlg->items[BM_BOX_IND].data);

	bm = get_bookmark_by_id(bookmark_dlg_box_id_get(box));
	if (!bm) return 0;


	/* Deleted in really_del_bookmark() */
	hop = mem_alloc(sizeof(struct push_del_button_hop_struct));
	if (!hop) return 0;

	hop->id = bm->id;
	hop->dlg = dlg->dlg;
	hop->box = box;

	msg_box(term, getml(hop, NULL),
		TEXT(T_DELETE_BOOKMARK), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_BOOKMARK), " \"", bm->title, "\" ?\n\n",
		TEXT(T_URL), ": \"", bm->url, "\"", NULL,
		hop, 2,
		TEXT(T_YES), really_del_bookmark, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);

	return 0;
}


/* Builds the "Bookmark manager" dialog */
void
menu_bookmark_manager(struct terminal *term, void *fcp, struct session *ses)
{
#define BM_DIALOG_MEMSIZE (sizeof(struct dialog) \
		           + (BM_BOX_IND + 2) * sizeof(struct dialog_item) \
			   + sizeof(struct bookmark) + 2 * MAX_STR_LEN)

	struct bookmark *new_bm;
	struct dialog *d;

	/* Show all bookmarks */
	foreach (new_bm, bookmarks) {
		new_bm->selected = 1;
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

	/* MP: D_BOX is nonsense. I tried to remove it, but didn't succeed. */
	d->items[BM_BOX_IND].type = D_BOX;
	d->items[BM_BOX_IND].gid = 12;

	bookmark_dlg_box_build((struct dlg_data_item_data_box **) &(d->items[BM_BOX_IND].data));

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
	struct dialog_data *parent;

	add_bookmark(d->items[0].data, d->items[1].data);

	parent = d->udata;

	/* Tell the bookmark dialog to redraw */
	if (parent) {
		int new, box_top = 0;
		int gid = parent->dlg->items[BM_BOX_IND].gid;
		struct dlg_data_item_data_box *box;

		box = (struct dlg_data_item_data_box *)
		      parent->dlg->items[BM_BOX_IND].data;
		new = bookmark_dlg_list_update(&box->items);

		box->sel = new - 1;

		if (new >= gid) box_top = new - gid;
		box->box_top = box_top;

	}
#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}


/* Search bookmarks */
void
bookmark_search_do(struct dialog *d)
{
	struct dialog_data *parent;
	int res;

	res = bookmark_simple_search(d->items[1].data, d->items[0].data);

	parent = d->udata;

	/* Tell the bookmark dialog to redraw */
	if (parent && res) {
		struct dlg_data_item_data_box *box;

		box = (struct dlg_data_item_data_box *)
		      parent->dlg->items[BM_BOX_IND].data;

		box->box_top = 0;
		box->sel = 0;

		bookmark_dlg_list_update(&box->items);
	}
}


/* launch_bm_add_doc_dialog() */
void
launch_bm_add_doc_dialog(struct terminal *term,
			 struct dialog_data *parent,
			 struct session *ses)
{
	do_edit_dialog(term, TEXT(T_ADD_BOOKMARK), NULL, NULL,
		       ses, parent, bookmark_add_add, NULL, 1);
}


/* launch_bm_search_doc_dialog() */
void
launch_bm_search_doc_dialog(struct terminal *term,
			    struct dialog_data *parent,
			    struct session *ses)
{
	do_edit_dialog(term, TEXT(T_SEARCH_BOOKMARK),
		       bm_last_searched_name, bm_last_searched_url,
		       ses, parent, bookmark_search_do, NULL, 0);
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
		       parent, bookmark_add_add, NULL, 1);
}

#else /* BOOKMARKS */

void menu_bookmark_manager(struct terminal *t, void *d, struct session *s) {}
void launch_bm_add_doc_dialog(struct terminal *t, struct dialog_data *d, struct session *s) {}
void launch_bm_add_link_dialog(struct terminal *t, struct dialog_data *d, struct session *s) {}

#endif
