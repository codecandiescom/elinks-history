/* Internal bookmarks support */
/* $Id: dialogs.c,v 1.48 2002/10/08 21:20:28 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "dialogs/hierbox.h"
#include "document/session.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "intl/language.h"
#include "util/error.h"
#include "util/memory.h"


/* Whether to save bookmarks after each modification of their list
 * (add/modify/delete). */
#define BOOKMARKS_RESAVE	1

/* The location of the box in the bookmark manager */
/* Duplicate with dialogs/hierbox.c. */
#define	BM_BOX_IND		8


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
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->items = &bookmark_box_items;
	add_to_list(bookmark_boxes, box);

	return box;
}


/* Cleans up after the bookmark dialog */
static void
bookmark_dialog_abort_handler(struct dialog_data *dlg)
{
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[BM_BOX_IND].data;

	del_from_list(box);
	/* Delete the box structure */
	mem_free(box);
}


/* The titles to appear in the bookmark manager */
unsigned char *bookmark_dialog_msg[] = {
	TEXT(T_BOOKMARKS),
};


void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *,
			      struct session *);


/* Callback for the "add" button in the bookmark manager */
static int
push_add_button(struct dialog_data *dlg, struct widget_data *di)
{
	launch_bm_add_doc_dialog(dlg->win->term, dlg,
				 (struct session *) dlg->dlg->udata);
	return 0;
}


void launch_bm_search_doc_dialog(struct terminal *, struct dialog_data *,
				 struct session *);


/* Callback for the "search" button in the bookmark manager */
static int
push_search_button(struct dialog_data *dlg, struct widget_data *di)
{
	launch_bm_search_doc_dialog(dlg->win->term, dlg,
				    (struct session *) dlg->dlg->udata);
	return 0;
}


static void
do_add_folder(struct dialog_data *dlg, unsigned char *name)
{
	struct widget_data *box_widget_data;
	struct listbox_data *box;
	struct bookmark *bm = NULL;

	box_widget_data = &dlg->items[BM_BOX_IND];
	box = (struct listbox_data *) box_widget_data->item->data;

	if (box->sel) {
		if (box->sel->type == BI_FOLDER) {
			bm = box->sel->udata;
		} else if (box->sel->root) {
			bm = box->sel->root->udata;
		}
	}
	bm = add_bookmark(bm, 0, name, "");
	bm->box_item->type = BI_FOLDER;

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

	/* We touch only the actual bookmark dialog, not all of them;
	 * that's right, right? ;-) --pasky */

	/* FIXME: No, I don't like this. But can we do better? --pasky */
	/* FIXME: _ought_ to be better. */
#if 0
	box->sel = bm->box_item;
	box->top = bm->box_item;
	box->sel_offset = 0;
#endif
}

static int
push_add_folder_button(struct dialog_data *dlg, struct widget_data *di)
{
	input_field(dlg->win->term, NULL, TEXT(T_ADD_FOLDER), TEXT(T_FOLDER_NAME),
		    TEXT(T_OK), TEXT(T_CANCEL), dlg, NULL,
		    MAX_STR_LEN, NULL, 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) do_add_folder,
		    NULL);
	return 0;
}


/* Called when the goto button is pushed */
static int
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
static void
bookmark_edit_done(struct dialog *d) {
	struct bookmark *bm = (struct bookmark *) d->udata2;

	update_bookmark(bm, d->items[0].data, d->items[1].data);
	bm->refcount--;

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

static void
bookmark_edit_cancel(struct dialog *d) {
	struct bookmark *bm = (struct bookmark *) d->udata2;

	bm->refcount--;
}

/* Called when the edit button is pushed */
static int
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
static void
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

static void
cancel_del_bookmark(void *vhop)
{	struct push_del_button_hop_struct *hop;

	hop = (struct push_del_button_hop_struct *) vhop;
	hop->bm->refcount--;
}

/* Callback for the "delete" button in the bookmark manager */
static int
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


static struct bookmark *move_cache_root_avoid;

static void
update_depths(struct listbox_item *parent)
{
	struct listbox_item *item;

	foreach (item, parent->child) {
		item->depth = parent->depth + 1;
		if (item->type == BI_FOLDER)
			update_depths(item);
	}
}

static void
do_move_bookmark(struct bookmark *dest, struct list_head *destb,
		 struct list_head *desti, struct list_head *src,
		 struct listbox_data *box) {
	struct bookmark *bm = (struct bookmark *) src; /* piggy */
	struct bookmark *bm_next = bm->next;
	int blind_insert = (destb && desti);

	/* Like foreach(), but foreach() hates when we delete actual items
	 * from the list. */
	while ((struct list_head *) bm_next != src) {
		bm = bm_next;
		bm_next = bm->next;

		if (bm == dest) {
			/* We WON'T ever try to proceed ourselves - saves us
			 * from insanities like moving a folder into itself
			 * and so on. */
			continue;
		}

		if (bm->box_item->marked && bm != move_cache_root_avoid) {
			bm->box_item->marked = 0;

			if (box->top == bm->box_item) {
				/* It's theoretically impossible that bm->next
				 * would be invalid (point to list_head), as it
				 * would be case only when there would be only
				 * one item in the list, and then bm != dest
				 * will save us already. */
				box->top = bm->box_item->next;
			}

			del_from_list(bm->box_item);
			del_from_list(bm);
			add_at_pos((!blind_insert ? dest
						 : (struct bookmark *) destb),
				   bm);
			add_at_pos((!blind_insert ? dest->box_item
						 : (struct listbox_item *) desti),
				   bm->box_item);

			if (blind_insert) {
				bm->root = dest;
				bm->box_item->root = dest->box_item;
			} else {
				bm->root = dest->root;
				bm->box_item->root = dest->box_item->root;
			}
			bm->box_item->depth = bm->root ? bm->root->box_item->depth + 1 : 0;
			if (bm->box_item->type == BI_FOLDER)
				update_depths(bm->box_item);

			dest = bm;
			blind_insert = 0;

			/* We don't want to care about anything marked inside
			 * of the marked folder, let's move it as a whole
			 * directly. I believe that this is more intuitive.
			 * --pasky */
			continue;
		}

		if (bm->box_item->type == BI_FOLDER) {
			do_move_bookmark(dest, blind_insert ? destb : NULL,
					 blind_insert ? desti : NULL,
					 &bm->child, box);
		}
	}
}

static int
push_move_button(struct dialog_data *dlg,
		 struct widget_data *blah)
{
	struct widget_data *box_widget_data;
	struct listbox_data *box;
	struct bookmark *dest = NULL;
	struct list_head *destb = NULL, *desti = NULL;

	box_widget_data = &dlg->items[BM_BOX_IND];
	box = (struct listbox_data *) box_widget_data->item->data;

	if (!box->sel) return 0; /* nowhere to move to */

	if (box->sel->type == BI_FOLDER && box->sel->expanded) {
		dest = box->sel->udata;
		destb = &((struct bookmark *) box->sel->udata)->child;
		desti = &box->sel->child;
	} else {
		dest = box->sel->udata;
	}

	/* Avoid recursion headaches (prevents moving a folder into itself). */
	move_cache_root_avoid = NULL;
	{
		struct bookmark *bm = dest->root;

		while (bm) {
			if (bm->box_item->marked)
				move_cache_root_avoid = bm;
			bm = bm->root;
		}
	}

	/* Traverse all expanded folders and move all marked items right
	 * after bm_dest. */
	do_move_bookmark(dest, destb, desti, &bookmarks, box);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
	display_dlg_item(dlg, box_widget_data, 1);
	return 0;
}


/* Builds the "Bookmark manager" dialog */
void
menu_bookmark_manager(struct terminal *term, void *fcp, struct session *ses)
{
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
	d = mem_calloc(1, sizeof(struct dialog)
			  + (BM_BOX_IND + 2) * sizeof(struct widget)
			  + sizeof(struct bookmark) + 2 * MAX_STR_LEN);
	if (!d) return;

	d->title = TEXT(T_BOOKMARK_MANAGER);
	d->fn = layout_hierbox_browser;
	d->handle_event = hierbox_dialog_event_handler;
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
	d->items[3].fn = push_move_button;
	d->items[3].text = TEXT(T_MOVE);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ENTER;
	d->items[4].fn = push_add_folder_button;
	d->items[4].text = TEXT(T_ADD_FOLDER);

	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ENTER;
	d->items[5].fn = push_add_button;
	d->items[5].text = TEXT(T_ADD);

	d->items[6].type = D_BUTTON;
	d->items[6].gid = B_ENTER;
	d->items[6].fn = push_search_button;
	d->items[6].text = TEXT(T_SEARCH);

	d->items[7].type = D_BUTTON;
	d->items[7].gid = B_ESC;
	d->items[7].fn = cancel_dialog;
	d->items[7].text = TEXT(T_CLOSE);

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
static void
bookmark_add_add(struct dialog *d)
{
	struct widget_data *box_widget_data;
	struct listbox_data *box;
	struct bookmark *bm = NULL;

	if (d->udata) {
		box_widget_data =
			&((struct dialog_data *) d->udata)->items[BM_BOX_IND];
		box = (struct listbox_data *) box_widget_data->item->data;

		if (box->sel) {
			if (box->sel->type == BI_FOLDER) {
				bm = box->sel->udata;
			} else if (box->sel->root) {
				bm = box->sel->root->udata;
			}
		}
	}

	bm = add_bookmark(bm, 1, d->items[0].data, d->items[1].data);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

	/* We touch only the actual bookmark dialog, not all of them;
	 * that's right, right? ;-) --pasky */

	/* And as the bookmark is supposed to be placed at the bottom of the
	 * list, we just move as down as possible. This is done so that
	 * box->top is adjusted correctly. */

	/* FIXME FIXME FIXME FIXME FIXME */
#if 0
	box_sel_move(box_widget_data, 32000); /* That is stuuupid. */
	/* ..and doesn't work at all for non-root adding. */
#endif
}


/* Search bookmarks */
static void
bookmark_search_do(struct dialog *d)
{
	struct listbox_item *item = bookmark_box_items.next;
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
