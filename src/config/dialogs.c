/* Options dialogs */
/* $Id: dialogs.c,v 1.2 2002/12/07 17:48:06 pasky Exp $ */

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
#include "config/dialogs.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "dialogs/hierbox.h"
#include "document/session.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"

/* The location of the box in the options manager */
#define	OP_BOX_IND		2


/****************************************************************************
  Bookmark manager stuff.
****************************************************************************/

/* Creates the box display (holds everything EXCEPT the actual rendering
 * data) */
static struct listbox_data *
option_dlg_box_build()
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->items = &root_option_box_items;
	add_to_list(option_boxes, box);

	return box;
}


/* Cleans up after the option dialog */
static void
option_dialog_abort_handler(struct dialog_data *dlg)
{
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[OP_BOX_IND].data;

	del_from_list(box);
	/* Delete the box structure */
	mem_free(box);
}


#if 0
/* Called when an edit is complete. */
static void
option_edit_done(struct dialog *d) {
	struct option *bm = (struct option *) d->udata2;

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
#endif

static void
done_info_button(void *vhop)
{
#if 0
	struct option *option = vhop;

#endif
}

static int
push_info_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct option *option;
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[OP_BOX_IND].data;

	/* Show history item info */
	if (!box->sel) return 0;
	option = box->sel->udata;
	if (!option) return 0;

	if (option_types[option->type].write) {
		unsigned char *value = init_str();
		int val_len = 0;

		option_types[option->type].write(option, &value, &val_len);

		msg_box(term, getml(value, NULL),
			TEXT(T_INFO), AL_LEFT | AL_EXTD_TEXT,
			TEXT(T_NNAME), ": ", option->name, "\n",
			TEXT(T_TYPE), ": ", option_types[option->type].name, "\n",
			TEXT(T_VALUE), ": ", value, "\n",
			TEXT(T_DESCRIPTION), ": ", option->desc, NULL,
			option, 1,
			TEXT(T_OK), done_info_button, B_ESC | B_ENTER);
	} else {
		msg_box(term, NULL,
			TEXT(T_INFO), AL_LEFT | AL_EXTD_TEXT,
			TEXT(T_NNAME), ": ", option->name, "\n",
			TEXT(T_TYPE), ": ", option_types[option->type].name, "\n",
			TEXT(T_DESCRIPTION), ": ", option->desc, NULL,
			option, 1,
			TEXT(T_OK), done_info_button, B_ESC | B_ENTER);
	}

	return 0;
}


/* Builds the "Options manager" dialog */
void
menu_options_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog *d;

	/* Create the dialog */
	d = mem_calloc(1, sizeof(struct dialog)
			  + (OP_BOX_IND + 2) * sizeof(struct widget)
			  + sizeof(struct option) + 2 * MAX_STR_LEN);
	if (!d) return;

	d->title = TEXT(T_OPTIONS_MANAGER);
	d->fn = layout_hierbox_browser;
	d->handle_event = hierbox_dialog_event_handler;
	d->abort = option_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_info_button;
	d->items[0].udata = ses;
	d->items[0].text = TEXT(T_INFO);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ESC;
	d->items[1].fn = cancel_dialog;
	d->items[1].text = TEXT(T_CLOSE);

	d->items[OP_BOX_IND].type = D_BOX;
	d->items[OP_BOX_IND].gid = 12;
	d->items[OP_BOX_IND].data = (void *) option_dlg_box_build();

	d->items[OP_BOX_IND + 1].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}
