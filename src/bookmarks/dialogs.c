/* Bookmarks dialogs */
/* $Id: dialogs.c,v 1.174 2004/07/14 13:51:18 jonas Exp $ */

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
#include "bfu/hierbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/ttime.h"


/* Whether to save bookmarks after each modification of their list
 * (add/modify/delete). */
#define BOOKMARKS_RESAVE	1


/* Last searched values */
unsigned char *bm_last_searched_name = NULL;
unsigned char *bm_last_searched_url = NULL;

static void
lock_bookmark(struct listbox_item *item)
{
	object_lock((struct bookmark *) item->udata);
}

static void
unlock_bookmark(struct listbox_item *item)
{
	object_unlock((struct bookmark *) item->udata);
}

static int
is_bookmark_used(struct listbox_item *item)
{
	return is_object_used((struct bookmark *) item->udata);
}

static unsigned char *
get_bookmark_text(struct listbox_item *item, struct terminal *term)
{
	struct bookmark *bookmark = item->udata;

	return stracpy(bookmark->title);
}

static unsigned char *
get_bookmark_info(struct listbox_item *item, struct terminal *term)
{
	struct bookmark *bookmark = item->udata;
	struct string info;

	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: %s", _("Title", term), bookmark->title);
	add_format_to_string(&info, "\n%s: %s", _("URL", term), bookmark->url);

	return info.source;
}

static struct uri *
get_bookmark_uri(struct listbox_item *item)
{
	struct bookmark *bookmark = item->udata;

	return bookmark->url && *bookmark->url
		? get_uri(bookmark->url, 0) : NULL;
}

static struct listbox_item *
get_bookmark_root(struct listbox_item *item)
{
	struct bookmark *bookmark = item->udata;

	return bookmark->root ? bookmark->root->box_item : NULL;
}

static int
can_delete_bookmark(struct listbox_item *item)
{
	return 1;
}

static void
delete_bookmark_item(struct listbox_item *item, int last)
{
	struct bookmark *bookmark = item->udata;

	assert(!is_object_used(bookmark));

	delete_bookmark(bookmark);

#ifdef BOOKMARKS_RESAVE
	if (last) write_bookmarks();
#endif
}

static struct listbox_ops_messages bookmarks_messages = {
	/* cant_delete_item */
	N_("Sorry, but bookmark \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but bookmark \"%s\" is being used by something else."),
	/* cant_delete_folder */
	N_("Sorry, but bookmark's folder \"%s\" cannot be deleted."),
	/* cant_delete_used_folder */
	N_("Sorry, but bookmark's folder \"%s\" is being used by something else."),
	/* delete_marked_items_title */
	N_("Delete marked bookmarks"),
	/* delete_marked_items */
	N_("Delete marked bookmarks?"),
	/* delete_folder_title */
	N_("Delete bookmark's folder"),
	/* delete_folder */
	N_("Delete folder \"%s\" and all bookmarks in it?"),
	/* delete_item_title */
	N_("Delete bookmark"),
	/* delete_item */
	N_("Delete this bookmark?"),
	/* clear_all_items_title */
	N_("Clear all bookmarks"),
	/* clear_all_items_title */
	N_("Do you really want to remove all bookmarks?"),
};

static struct listbox_ops bookmarks_listbox_ops = {
	lock_bookmark,
	unlock_bookmark,
	is_bookmark_used,
	get_bookmark_text,
	get_bookmark_info,
	get_bookmark_uri,
	get_bookmark_root,
	NULL,
	can_delete_bookmark,
	delete_bookmark_item,
	NULL,
	&bookmarks_messages,
};

/****************************************************************************
  Bookmark manager stuff.
****************************************************************************/

void launch_bm_add_doc_dialog(struct terminal *, struct dialog_data *,
			      struct session *);


/* Callback for the "add" button in the bookmark manager */
static int
push_add_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
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
push_search_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
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
	struct widget_data *widget_data = dlg_data->widgets_data;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct bookmark *bm = NULL;

	if (box->sel) {
		struct bookmark *selected = box->sel->udata;

		if (box->sel->type == BI_FOLDER && box->sel->expanded) {
			bm = selected;
		} else {
			bm = selected->root;
		}
	}
	bm = add_bookmark(bm, 1, name, NULL);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

	/* We touch only the actual bookmark dialog, not all of them;
	 * that's right, right? ;-) --pasky */
	focus_bookmark(widget_data, box, bm);
}

static int
push_add_folder_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	input_field(dlg_data->win->term, NULL, 1,
		    N_("Add folder"), N_("Folder name"),
		    N_("OK"), N_("Cancel"), dlg_data, NULL,
		    MAX_STR_LEN, NULL, 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) do_add_folder,
		    NULL);
	return 0;
}


/**** EDIT ***********************************************************/

/* Called when an edit is complete. */
static void
bookmark_edit_done(struct dialog *dlg) {
	struct bookmark *bm = (struct bookmark *) dlg->udata2;

	update_bookmark(bm, dlg->widgets[0].data, dlg->widgets[1].data);
	object_unlock(bm);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

static void
bookmark_edit_cancel(struct dialog *dlg) {
	struct bookmark *bm = (struct bookmark *) dlg->udata2;

	object_unlock(bm);
}

/* Called when the edit button is pushed */
static int
push_edit_button(struct dialog_data *dlg_data, struct widget_data *edit_btn)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);

	/* Follow the bookmark */
	if (box->sel) {
		struct bookmark *bm = (struct bookmark *) box->sel->udata;
		const unsigned char *name = bm->title;
		const unsigned char *url = bm->url;

		object_lock(bm);
		do_edit_dialog(dlg_data->win->term, 1, N_("Edit bookmark"),
			       name, url,
			       (struct session *) dlg_data->dlg->udata, dlg_data,
			       bookmark_edit_done, bookmark_edit_cancel,
			       (void *) bm, EDIT_DLG_ADD);
	}

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
			} else {
				bm->root = dest->root;
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
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct bookmark *dest = NULL;
	struct list_head *destb = NULL, *desti = NULL;
	struct widget_data *widget_data = dlg_data->widgets_data;

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

	bookmarks_set_dirty();

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
	display_dlg_item(dlg_data, widget_data, 1);
	return 0;
}


/**** MANAGEMENT *****************************************************/

#define	BOOKMARK_MANAGER_BUTTONS	8

static struct hierbox_browser_button bookmark_buttons[] = {
	{ N_("Goto"),		push_hierbox_goto_button,	1 },
	{ N_("Edit"),		push_edit_button,		0 },
	{ N_("Delete"),		push_hierbox_delete_button,	0 },
	{ N_("Move"),		push_move_button,		0 },
	{ N_("Add folder"),	push_add_folder_button,		0 },
	{ N_("Add"),		push_add_button,		0 },
	{ N_("Search"),		push_search_button,		1 },
	{ N_("Clear"),		push_hierbox_clear_button,	0 },
#if 0
	/* TODO: Would this be useful? --jonas */
	{ N_("Save"),		push_save_button		},
#endif
};

struct_hierbox_browser(
	bookmark_browser,
	N_("Bookmark manager"),
	bookmark_buttons,
	&bookmarks_listbox_ops
);

/* Builds the "Bookmark manager" dialog */
void
bookmark_manager(struct session *ses)
{
	/* Reset momorized search criterias */
	mem_free_set(&bm_last_searched_name, NULL);
	mem_free_set(&bm_last_searched_url, NULL);
	bookmark_browser.expansion_callback = bookmarks_set_dirty;
	hierbox_browser(&bookmark_browser, ses);
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

		widget_data = dlg_data->widgets_data;
		box = get_dlg_listbox_data(dlg_data);

		if (box->sel) {
			struct bookmark *selected = box->sel->udata;

			if (box->sel->type == BI_FOLDER) {
				bm = selected;
			} else {
				bm = selected->root;
			}
		}
	}

	bm = add_bookmark(bm, 1, dlg->widgets[0].data, dlg->widgets[1].data);

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
	unsigned char *search_title = dlg->widgets[0].data;
	unsigned char *search_url = dlg->widgets[1].data;
	struct bookmark_search_ctx ctx = NULL_BOOKMARK_SEARCH_CTX;
	struct widget_data *widget_data = NULL;
	struct listbox_data *box = NULL;
	struct dialog_data *dlg_data;

	if (!search_title || !search_url)
		return;

	assertm(dlg->udata, "Bookmark search with NULL udata in dialog");
	if_assert_failed return;

	dlg_data = (struct dialog_data *) dlg->udata;
	widget_data = dlg_data->widgets_data;
	box = get_dlg_listbox_data(dlg_data);

	/* Memorize last searched title */
	mem_free_set(&bm_last_searched_name, stracpy(search_title));
	if (!bm_last_searched_name) return;

	/* Memorize last searched url */
	mem_free_set(&bm_last_searched_url, stracpy(search_url));
	if (!bm_last_searched_url) {
		mem_free(bm_last_searched_name);
		return;
	}

	ctx.search_url = search_url;
	ctx.search_title = search_title;

	traverse_listbox_items_list(box->sel, box, 0, 0, test_search, &ctx);
	if (!ctx.found) return;

	box_sel_move(widget_data, ctx.ofs - 1);
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


void
launch_bm_add_dialog(struct terminal *term,
		     struct dialog_data *parent,
		     struct session *ses,
		     unsigned char *title,
		     unsigned char *url)
{
	do_edit_dialog(term, 1, N_("Add bookmark"), title, url, ses,
		       parent, bookmark_add_add, NULL, NULL, EDIT_DLG_ADD);
}

void
launch_bm_add_doc_dialog(struct terminal *term,
			 struct dialog_data *parent,
			 struct session *ses)
{
	launch_bm_add_dialog(term, parent, ses, NULL, NULL);
}

void
launch_bm_add_link_dialog(struct terminal *term,
			  struct dialog_data *parent,
			  struct session *ses)
{
	unsigned char title[MAX_STR_LEN], url[MAX_STR_LEN];

	launch_bm_add_dialog(term, parent, ses,
			     get_current_link_name(ses, title, MAX_STR_LEN),
			     get_current_link_url(ses, url, MAX_STR_LEN));
}

void
bookmark_terminal_tabs_dialog(struct terminal *term)
{
	struct string string;

	if (!init_string(&string)) return;

	add_to_string(&string, _("Saved session", term));

#ifdef HAVE_STRFTIME
	add_to_string(&string, " - ");
	add_date_to_string(&string, get_opt_str("ui.date_format"), NULL);
#endif

	input_field(term, NULL, 1,
		    N_("Bookmark tabs"), N_("Enter folder name"),
		    N_("OK"), N_("Cancel"), term, NULL,
		    MAX_STR_LEN, string.source, 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) bookmark_terminal_tabs,
		    NULL);

	done_string(&string);
}
