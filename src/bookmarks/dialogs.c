/* Bookmarks dialogs */
/* $Id: dialogs.c,v 1.93 2003/10/24 16:14:36 zas Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "dialogs/hierbox.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"


/* Whether to save bookmarks after each modification of their list
 * (add/modify/delete). */
#define BOOKMARKS_RESAVE	1

/* The location of the box in the bookmark manager */
#define	BM_BOX_IND		8


#ifdef BOOKMARKS

/* Last searched values */
unsigned char *bm_last_searched_name = NULL;
unsigned char *bm_last_searched_url = NULL;

static void listbox_delete_bookmark(struct terminal *, struct listbox_data *);

static struct listbox_ops bookmarks_listbox_ops = {
	listbox_delete_bookmark,
};

/****************************************************************************
  Bookmark manager stuff.
****************************************************************************/

/* Creates the box display (holds everything EXCEPT the actual rendering
 * data) */
static struct listbox_data *
bookmark_dlg_box_build(void)
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->ops = &bookmarks_listbox_ops;
	box->items = &bookmark_box_items;
	add_to_list(bookmark_boxes, box);

	return box;
}


/* Cleans up after the bookmark dialog */
static void
bookmark_dialog_abort_handler(struct dialog_data *dlg_data)
{
	struct widget *widget = &(dlg_data->dlg->items[BM_BOX_IND]);
	struct listbox_data *box = (struct listbox_data *) widget->data;

	del_from_list(box);
	/* Delete the box structure */
	mem_free(box);
}


void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *,
			      struct session *);


/* Callback for the "add" button in the bookmark manager */
static int
push_add_button(struct dialog_data *dlg_data, struct widget_data *di)
{
	launch_bm_add_doc_dialog(dlg_data->win->term, dlg_data,
				 (struct session *) dlg_data->dlg->udata);
	return 0;
}


static
void launch_bm_search_doc_dialog(struct terminal *, struct dialog_data *,
				 struct session *);


/* Callback for the "search" button in the bookmark manager */
static int
push_search_button(struct dialog_data *dlg_data, struct widget_data *di)
{
	launch_bm_search_doc_dialog(dlg_data->win->term, dlg_data,
				    (struct session *) dlg_data->dlg->udata);
	return 0;
}


/**** ADD FOLDER *****************************************************/

static void
focus_bookmark(struct widget_data *box_widget_data, struct listbox_data *box,
		struct bookmark *bm)
{
	/* Infinite loop protector. Maximal safety. It will protect your system
	 * from 100% CPU time. Buy it now. Only from Sirius Labs. */
	struct listbox_item *sel2 = NULL;

	do {
		sel2 = box->sel;
		box_sel_move(box_widget_data, 1);
	} while (box->sel->udata != bm && box->sel != sel2);
}

static void
do_add_folder(struct dialog_data *dlg_data, unsigned char *name)
{
	struct bookmark *bm = NULL;
	struct widget_data *widget_data = &dlg_data->items[BM_BOX_IND];
	struct listbox_data *box;

	box = (struct listbox_data *) widget_data->item->data;

	if (box->sel) {
		if (box->sel->type == BI_FOLDER && box->sel->expanded) {
			bm = box->sel->udata;
		} else if (box->sel->root) {
			bm = box->sel->root->udata;
		}
	}
	bm = add_bookmark(bm, 1, name, "");
	bm->box_item->type = BI_FOLDER;

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

	/* We touch only the actual bookmark dialog, not all of them;
	 * that's right, right? ;-) --pasky */
	focus_bookmark(widget_data, box, bm);
}

static int
push_add_folder_button(struct dialog_data *dlg_data, struct widget_data *di)
{
	input_field(dlg_data->win->term, NULL, 1,
		    N_("Add folder"), N_("Folder name"),
		    N_("OK"), N_("Cancel"), dlg_data, NULL,
		    MAX_STR_LEN, NULL, 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) do_add_folder,
		    NULL);
	return 0;
}


/**** GOTO ***********************************************************/

/* Called when the goto button is pushed */
static int
push_goto_button(struct dialog_data *dlg_data, struct widget_data *goto_btn)
{
	struct widget *widget = &(dlg_data->dlg->items[BM_BOX_IND]);
	struct listbox_data *box = (struct listbox_data *) widget->data;

	/* Do nothing with a folder */
	if (box->sel && box->sel->type == BI_FOLDER)
		return 0;

	/* Follow the bookmark */
	if (box->sel)
		goto_url((struct session *) goto_btn->item->udata,
			 ((struct bookmark *) box->sel->udata)->url);

	/* Close the bookmark dialog */
	delete_window(dlg_data->win);
	return 0;
}


/**** EDIT ***********************************************************/

/* Called when an edit is complete. */
static void
bookmark_edit_done(struct dialog *dlg) {
	struct bookmark *bm = (struct bookmark *) dlg->udata2;

	update_bookmark(bm, dlg->items[0].data, dlg->items[1].data);
	bm->refcount--;

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

static void
bookmark_edit_cancel(struct dialog *dlg) {
	struct bookmark *bm = (struct bookmark *) dlg->udata2;

	bm->refcount--;
}

/* Called when the edit button is pushed */
static int
push_edit_button(struct dialog_data *dlg_data, struct widget_data *edit_btn)
{
	struct widget *widget = &(dlg_data->dlg->items[BM_BOX_IND]);
	struct listbox_data *box = (struct listbox_data *) widget->data;

	/* Follow the bookmark */
	if (box->sel) {
		struct bookmark *bm = (struct bookmark *) box->sel->udata;
		const unsigned char *name = bm->title;
		const unsigned char *url = bm->url;

		bm->refcount++;
		do_edit_dialog(dlg_data->win->term, 1, N_("Edit bookmark"),
			       name, url,
			       (struct session *) edit_btn->item->udata, dlg_data,
			       bookmark_edit_done, bookmark_edit_cancel,
			       (void *) bm, EDIT_DLG_ADD);
	}

	return 0;
}


/**** DELETE *********************************************************/

/* Used to carry extra info between the push_delete_button() and the
 * really_del_bookmark() */
struct push_del_button_hop_struct {
	struct terminal *term;
	struct bookmark *bm; /* NULL -> destroy marked bookmarks */
};

/* Do the job needed for really deleting a bookmark. */
static void
do_del_bookmark(struct terminal *term, struct bookmark *bookmark)
{
	struct listbox_data *box;

	if (bookmark->refcount > 0) {
		if (bookmark->box_item->type == BI_FOLDER)
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Delete bookmark"), AL_CENTER,
			msg_text(term, N_("Sorry, but this bookmark is already "
				"being used by something right now.\n\n"
				"Title: \"%s\""),
				bookmark->title),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		else
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Delete bookmark"), AL_CENTER,
			msg_text(term, N_("Sorry, but this bookmark is already "
				"being used by something right now.\n\n"
				"Title: \"%s\"\n"
				"URL: \"%s\""),
				bookmark->title, bookmark->url),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return;
	}

	if (bookmark->box_item->type == BI_FOLDER) {
		while (!list_empty(bookmark->child)) {
			do_del_bookmark(term, bookmark->child.next);
		}
	}

	/* Take care about move of the selected item if we're deleting it. */

	foreach (box, *bookmark->box_item->box) {

	/* Please. Don't. Reindent. This. Ask. Why. --pasky */

	/* Remember that we have to take the reverse direction here in
	 * traverse(), as we have different sort order than usual. */

	if (box->sel && box->sel->udata == bookmark) {
		struct bookmark *bm = (struct bookmark *) box->sel->udata;

		box->sel = traverse_listbox_items_list(bm->box_item, -1,
				1, NULL, NULL);
		if (bm->box_item == box->sel)
			box->sel = traverse_listbox_items_list(bm->box_item, 1,
					1, NULL, NULL);
		if (bm->box_item == box->sel)
			box->sel = NULL;
	}

	if (box->top && box->top->udata == bookmark) {
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

	if (list_empty(bookmark->child))
		delete_bookmark(bookmark);
}

static int
delete_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct push_del_button_hop_struct *hop = data_;

	if (item->marked) {
		do_del_bookmark(hop->term, item->udata);
		return 1;
	}
	return 0;
}

/* Called to _really_ delete a bookmark (a confirm in the delete dialog) */
static void
really_del_bookmark(void *vhop)
{
	struct push_del_button_hop_struct *hop;

	hop = (struct push_del_button_hop_struct *) vhop;

	if (hop->bm) {
		if (hop->bm->refcount) hop->bm->refcount--;
		do_del_bookmark(hop->term, hop->bm);
	} else {
		traverse_listbox_items_list(bookmark_box_items.next, 0, 0,
						delete_marked, hop);
	}

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

static void
cancel_del_bookmark(void *vhop)
{	struct push_del_button_hop_struct *hop;

	hop = (struct push_del_button_hop_struct *) vhop;
	if (hop->bm) hop->bm->refcount--;
}

static int
scan_for_marks(struct listbox_item *item, void *data_, int *offset)
{
	if (item->marked) {
		struct push_del_button_hop_struct *hop = data_;

		hop->bm = NULL;
		*offset = 0;
	}
	return 0;
}

static void
listbox_delete_bookmark(struct terminal *term, struct listbox_data *box)
{
	struct push_del_button_hop_struct *hop;
	struct bookmark *bm;

	if (!box->sel) return;
	bm = (struct bookmark *) box->sel->udata;
	if (!bm) return;

	/* Deleted in really_del_bookmark() */
	hop = mem_alloc(sizeof(struct push_del_button_hop_struct));
	if (!hop) return;

	hop->bm = bm;
	hop->term = term;

	/* Look if it wouldn't be more interesting to blast off the marked
	 * bookmarks. */
	traverse_listbox_items_list(box->items->next, 0, 0, scan_for_marks, hop);
	bm = hop->bm;

	if (bm) bm->refcount++;

	if (!bm)
	msg_box(term, getml(hop, NULL), 0,
		N_("Delete bookmark"), AL_CENTER,
		N_("Delete marked bookmarks?"),
		hop, 2,
		N_("Yes"), really_del_bookmark, B_ENTER,
		N_("No"), cancel_del_bookmark, B_ESC);
	else if (bm->box_item->type == BI_FOLDER)
	msg_box(term, getml(hop, NULL), MSGBOX_FREE_TEXT,
		N_("Delete bookmark"), AL_CENTER,
		msg_text(term, N_("Delete content of folder \"%s\"?"),
			bm->title),
		hop, 2,
		N_("Yes"), really_del_bookmark, B_ENTER,
		N_("No"), cancel_del_bookmark, B_ESC);
	else
	msg_box(term, getml(hop, NULL), MSGBOX_FREE_TEXT,
		N_("Delete bookmark"), AL_CENTER,
		msg_text(term, N_("Delete bookmark \"%s\"?\n\n"
			"URL: \"%s\""),
			bm->title, bm->url),
		hop, 2,
		N_("Yes"), really_del_bookmark, B_ENTER,
		N_("No"), cancel_del_bookmark, B_ESC);

	return;
}

/* Callback for the "delete" button in the bookmark manager */
static int
push_delete_button(struct dialog_data *dlg_data,
		   struct widget_data *some_useless_delete_button)
{
	struct terminal *term = dlg_data->win->term;
	struct widget *widget = &(dlg_data->dlg->items[BM_BOX_IND]);
	struct listbox_data *box = (struct listbox_data *) widget->data;

	listbox_delete_bookmark(term, box);
	return 0;
}


/**** MOVE ***********************************************************/

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
		 struct listbox_data *box)
{
	struct bookmark *bm = (struct bookmark *) src; /* piggy */
	struct bookmark *bm_next = bm->next;
	int blind_insert = (destb && desti);

	/* Like foreach (), but foreach () hates when we delete actual items
	 * from the list. */
	while ((struct list_head *) bm_next != src) {
		bm = bm_next;
		bm_next = bm->next;

		if (bm != dest /* prevent moving a folder into itself */
		    && bm->box_item->marked && bm != move_cache_root_avoid) {
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
push_move_button(struct dialog_data *dlg_data,
		 struct widget_data *blah)
{
	struct bookmark *dest = NULL;
	struct list_head *destb = NULL, *desti = NULL;
	struct widget_data *widget_data = &dlg_data->items[BM_BOX_IND];
	struct listbox_data *box = (struct listbox_data *) widget_data->item->data;

	if (!box->sel) return 0; /* nowhere to move to */

	dest = box->sel->udata;
	if (box->sel->type == BI_FOLDER && box->sel->expanded) {
		destb = &((struct bookmark *) box->sel->udata)->child;
		desti = &box->sel->child;
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

	bookmarks_dirty = 1;

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
	display_dlg_item(dlg_data, widget_data, 1);
	return 0;
}


/**** MANAGEMENT *****************************************************/

/* Builds the "Bookmark manager" dialog */
void
menu_bookmark_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct bookmark *new_bm;
	struct dialog *dlg;
	int n = 0;

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
	dlg = mem_calloc(1, sizeof(struct dialog)
			 + (BM_BOX_IND + 2) * sizeof(struct widget)
			 + sizeof(struct bookmark) + 2 * MAX_STR_LEN);
	if (!dlg) return;

	dlg->title = _("Bookmark manager", term);
	dlg->fn = layout_hierbox_browser;
	dlg->handle_event = hierbox_dialog_event_handler;
	dlg->abort = bookmark_dialog_abort_handler;
	dlg->udata = ses;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ENTER;
	dlg->items[n].fn = push_goto_button;
	dlg->items[n].udata = ses;
	dlg->items[n].text = _("Goto", term);
	n++;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ENTER;
	dlg->items[n].fn = push_edit_button;
	dlg->items[n].udata = ses;
	dlg->items[n].text = _("Edit", term);
	n++;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ENTER;
	dlg->items[n].fn = push_delete_button;
	dlg->items[n].text = _("Delete", term);
	n++;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ENTER;
	dlg->items[n].fn = push_move_button;
	dlg->items[n].text = _("Move", term);
	n++;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ENTER;
	dlg->items[n].fn = push_add_folder_button;
	dlg->items[n].text = _("Add folder", term);
	n++;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ENTER;
	dlg->items[n].fn = push_add_button;
	dlg->items[n].text = _("Add", term);
	n++;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ENTER;
	dlg->items[n].fn = push_search_button;
	dlg->items[n].text = _("Search", term);
	n++;

	dlg->items[n].type = D_BUTTON;
	dlg->items[n].gid = B_ESC;
	dlg->items[n].fn = cancel_dialog;
	dlg->items[n].text = _("Close", term);
	n++;

	assert(n == BM_BOX_IND);

	dlg->items[n].type = D_BOX;
	dlg->items[n].gid = 12;
	dlg->items[n].data = (void *) bookmark_dlg_box_build();
	n++;

	dlg->items[n].type = D_END;

	do_dialog(term, dlg, getml(dlg, NULL));
}



/****************************************************************************\
  Bookmark add dialog.
\****************************************************************************/

/* Adds the bookmark */
static void
bookmark_add_add(struct dialog *dlg)
{
	struct widget_data *widget_data = NULL; /* silence stupid gcc */
	struct listbox_data *box = NULL;
	struct bookmark *bm = NULL;

	if (dlg->udata) {
		struct dialog_data *dlg_data = (struct dialog_data *) dlg->udata;

		widget_data = &(dlg_data->items[BM_BOX_IND]);
		box = (struct listbox_data *) widget_data->item->data;

		if (box->sel) {
			if (box->sel->type == BI_FOLDER) {
				bm = box->sel->udata;
			} else if (box->sel->root) {
				bm = box->sel->root->udata;
			}
		}
	}

	bm = add_bookmark(bm, 1, dlg->items[0].data, dlg->items[1].data);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

	if (dlg->udata) {
		/* We touch only the actual bookmark dialog, not all of them;
		 * that's right, right? ;-) --pasky */
		focus_bookmark(widget_data, box, bm);
	}
}


/* Searchs a substring either in title or url fields (ignoring
 * case).  If search_title and search_url are not empty, it selects bookmarks
 * matching the first OR the second.
 *
 * Perhaps another behavior could be to search bookmarks matching both
 * (replacing OR by AND), but it would break a cool feature: when on a page,
 * opening search dialog will have fields corresponding to that page, so
 * pressing ok will find any bookmark with that title or url, permitting a
 * rapid search of an already existing bookmark. --Zas */

struct bookmark_search_ctx {
	unsigned char *search_url;
	unsigned char *search_title;
	int found;
	int ofs;
};

#define NULL_BOOKMARK_SEARCH_CTX {NULL, NULL, 0, 0}

static int
test_search(struct listbox_item *item, void *data_, int *offset) {
	struct bookmark_search_ctx *ctx = data_;
	struct bookmark *bm = item->udata;
	int m = ((ctx->search_title && *ctx->search_title
		  && strcasestr(bm->title, ctx->search_title)) ||
		 (ctx->search_url && *ctx->search_url
		  && strcasestr(bm->url, ctx->search_url)));

	if (!ctx->ofs) m = 0; /* ignore possible match on first item */
	ctx->found = m;
	ctx->ofs++;

	if (m) *offset = 0;
	return 0;
}

/* Search bookmarks */
static void
bookmark_search_do(struct dialog *dlg)
{
	unsigned char *search_title = dlg->items[0].data;
	unsigned char *search_url = dlg->items[1].data;
	struct bookmark_search_ctx ctx = NULL_BOOKMARK_SEARCH_CTX;
	struct widget_data *widget_data = NULL;
	struct listbox_data *box = NULL;
	struct dialog_data *dlg_data;

	if (!search_title || !search_url)
		return;

	if (!dlg->udata)
		internal("Bookmarks search without udata in dialog! Let's panic.");

	dlg_data = (struct dialog_data *) dlg->udata;
	widget_data = &(dlg_data->items[BM_BOX_IND]);
	box = (struct listbox_data *) widget_data->item->data;

	/* Memorize last searched title */
	if (bm_last_searched_name) mem_free(bm_last_searched_name);
	bm_last_searched_name = stracpy(search_title);
	if (!bm_last_searched_name) return;

	/* Memorize last searched url */
	if (bm_last_searched_url) mem_free(bm_last_searched_url);
	bm_last_searched_url = stracpy(search_url);
	if (!bm_last_searched_url) {
		mem_free(bm_last_searched_name);
		return;
	}

	ctx.search_url = search_url;
	ctx.search_title = search_title;

	traverse_listbox_items_list(box->sel, 0, 0, test_search, &ctx);
	if (!ctx.found) return;

	box_sel_move(widget_data, ctx.ofs - 1);
}


/* launch_bm_add_doc_dialog() */
void
launch_bm_add_doc_dialog(struct terminal *term,
			 struct dialog_data *parent,
			 struct session *ses)
{
	do_edit_dialog(term, 1, N_("Add bookmark"), NULL, NULL,
		       ses, parent, bookmark_add_add, NULL, NULL,
		       EDIT_DLG_ADD);
}


/* launch_bm_search_doc_dialog() */
static void
launch_bm_search_doc_dialog(struct terminal *term,
			    struct dialog_data *parent,
			    struct session *ses)
{
	do_edit_dialog(term, 1, N_("Search bookmarks"),
		       bm_last_searched_name, bm_last_searched_url,
		       ses, parent, bookmark_search_do, NULL, NULL,
		       EDIT_DLG_SEARCH);
}


/* Called to launch an add dialog on the current link */
void
launch_bm_add_link_dialog(struct terminal *term,
			  struct dialog_data *parent,
			  struct session *ses)
{
	unsigned char title[MAX_STR_LEN], url[MAX_STR_LEN];

	do_edit_dialog(term, 1, N_("Add bookmark"),
		       get_current_link_name(ses, title, MAX_STR_LEN),
		       get_current_link_url(ses, url, MAX_STR_LEN), ses,
		       parent, bookmark_add_add, NULL, NULL, EDIT_DLG_ADD);
}

#endif
