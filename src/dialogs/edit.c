/* Generic support for edit/search historyitem/bookmark dialog */
/* $Id: edit.c,v 1.22 2003/01/03 00:38:33 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/text.h"
#include "document/session.h"
#include "dialogs/edit.h"
#include "lowlevel/terminal.h"
#include "intl/language.h"
#include "util/memory.h"
#include "util/string.h"


static unsigned char *edit_add_msg[] = {
	N_("Name"),
	N_("URL"),
};


static int
my_cancel_dialog(struct dialog_data *dlg, struct widget_data *wd)
{
	((void (*)(struct dialog *)) dlg->dlg->items[4].data)(dlg->dlg);
	return cancel_dialog(dlg, wd);
}


/* Called to setup the edit dialog */
static void
layout_add_dialog(struct dialog_data *dlg)
{
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct terminal *term = dlg->win->term;
	int dialog_text_color = get_bfu_color(term, "dialog.text");

	max_text_width(term, edit_add_msg[0], &max);
	min_text_width(term, edit_add_msg[0], &min);
	max_text_width(term, edit_add_msg[1], &max);
	min_text_width(term, edit_add_msg[1], &min);
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;

	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	w = rw = 50;

	dlg_format_text(NULL, term, edit_add_msg[0], 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, edit_add_msg[1], 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_buttons(NULL, term, dlg->items + 2, 2, 0,
			   &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, edit_add_msg[0], dlg->x + DIALOG_LB,
			&y, w, NULL, dialog_text_color, AL_LEFT);
	dlg_format_field(NULL, term, &dlg->items[0], dlg->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, edit_add_msg[1], dlg->x + DIALOG_LB,
			&y, w, NULL, dialog_text_color, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[1], dlg->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[2], 3, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}


/* Edits an item's fields.
 * If parent is defined, then that points to a dialog that should be sent
 * an update when the add is done.
 *
 * If either of src_name or src_url are NULL, try to obtain the name and url
 * of the current document. If you want to create two null fields, pass in a
 * pointer to a zero length string (""). */
void
do_edit_dialog(struct terminal *term, unsigned char *title,
	       const unsigned char *src_name,
	       const unsigned char *src_url,
	       struct session *ses, struct dialog_data *parent,
	       void when_done(struct dialog *),
	       void when_cancel(struct dialog *),
	       void *done_data,
	       int dialog_type /* 1 edit/add or 0 search dialog */)
{
	/* Number of fields in edit dialog --Zas */
#define BM_EDIT_DIALOG_FIELDS_NB 5

	unsigned char *name, *url;
	struct dialog *d;

	/* Create the dialog */
	d = mem_calloc(1, sizeof(struct dialog)
			  + (BM_EDIT_DIALOG_FIELDS_NB + 1)
			    * sizeof(struct widget)
			  + 2 * MAX_STR_LEN);
	if (!d) return;

	name = (unsigned char *) &d->items[BM_EDIT_DIALOG_FIELDS_NB + 1];
	url = name + MAX_STR_LEN;

	/* Get the name */
	if (!src_name) {
		/* Unknown name. */
		get_current_title(ses, name, MAX_STR_LEN);
	} else {
		/* Known name. */
		safe_strncpy(name, src_name, MAX_STR_LEN);
	}

	/* Get the url */
	if (!src_url) {
		/* Unknown . */
		get_current_url(ses, url, MAX_STR_LEN);
	} else {
		/* Known url. */
		safe_strncpy(url, src_url, MAX_STR_LEN);
	}

	d->title = title;
	d->fn = layout_add_dialog;
	d->refresh = (void (*)(void *)) when_done;
	d->refresh_data = d;
	d->udata = parent;
	d->udata2 = done_data;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = name;
	if (dialog_type == 1) d->items[0].fn = check_nonempty;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = url;
	/* if (dialog_type == 1) d->items[1].fn = check_nonempty; */

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = ok_dialog;
	d->items[2].text = N_("OK");

	d->items[3].type = D_BUTTON;
	d->items[3].gid = 0;
	d->items[3].text = N_("Clear");
	d->items[3].fn = clear_dialog;

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ESC;
	d->items[4].text = N_("Cancel");
	d->items[4].data = (void *) when_cancel;
	d->items[4].fn = when_cancel ? my_cancel_dialog : cancel_dialog;

	d->items[BM_EDIT_DIALOG_FIELDS_NB].type = D_END;

	do_dialog(term, d, getml(d, NULL));

#undef BM_EDIT_DIALOG_FIELDS_NB
}
