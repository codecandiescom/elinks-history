/* Generic support for edit/search historyitem/bookmark dialog */
/* $Id: edit.c,v 1.41 2003/10/24 16:04:50 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/memory.h"
#include "util/string.h"


static unsigned char *edit_add_msg[] = {
	N_("Name"),
	N_("URL"),
};


static int
my_cancel_dialog(struct dialog_data *dlg_data, struct widget_data *wd)
{
	((void (*)(struct dialog *)) dlg_data->dlg->items[4].data)(dlg_data->dlg);
	return cancel_dialog(dlg_data, wd);
}


/* Called to setup the edit dialog */
static void
layout_add_dialog(struct dialog_data *dlg_data)
{
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct terminal *term = dlg_data->win->term;
	struct color_pair *dialog_text_color = get_bfu_color(term, "dialog.text");

	text_width(term, edit_add_msg[0], &min, &max);
	text_width(term, edit_add_msg[1], &min, &max);
	buttons_width(term, dlg_data->items + 2, 2, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;

	/* int_upper_bound(&w, max); */
	int_lower_bound(&w, min);
	int_bounds(&w, 1, term->x - 2 * DIALOG_LB);

	rw = w;

	dlg_format_text(NULL, term, edit_add_msg[0], 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, edit_add_msg[1], 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_buttons(NULL, term, dlg_data->items + 2, 2, 0,
			   &y, w, &rw, AL_CENTER);
	w = rw;
	dlg_data->xw = w + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg_data);
	draw_dlg(dlg_data);
	y = dlg_data->y + DIALOG_TB;
	dlg_format_text(term, term, edit_add_msg[0], dlg_data->x + DIALOG_LB,
			&y, w, NULL, dialog_text_color, AL_LEFT);
	dlg_format_field(NULL, term, &dlg_data->items[0], dlg_data->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, edit_add_msg[1], dlg_data->x + DIALOG_LB,
			&y, w, NULL, dialog_text_color, AL_LEFT);
	dlg_format_field(term, term, &dlg_data->items[1], dlg_data->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg_data->items[2], 3, dlg_data->x + DIALOG_LB,
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
do_edit_dialog(struct terminal *term, int intl, unsigned char *title,
	       const unsigned char *src_name,
	       const unsigned char *src_url,
	       struct session *ses, struct dialog_data *parent,
	       void when_done(struct dialog *),
	       void when_cancel(struct dialog *),
	       void *done_data,
	       enum edit_dialog_type dialog_type)
{
	/* Number of fields in edit dialog --Zas */
#define EDIT_DIALOG_FIELDS_NB 5

	unsigned char *name, *url;
	struct dialog *dlg;

	if (intl) title = _(title, term);

	/* Create the dialog */
	dlg = mem_calloc(1, sizeof(struct dialog)
			    + (EDIT_DIALOG_FIELDS_NB + 1)
			      * sizeof(struct widget)
			    + 2 * MAX_STR_LEN);
	if (!dlg) return;

	name = (unsigned char *) &dlg->items[EDIT_DIALOG_FIELDS_NB + 1];
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

	dlg->title = title;
	dlg->fn = layout_add_dialog;
	dlg->refresh = (void (*)(void *)) when_done;
	dlg->refresh_data = dlg;
	dlg->udata = parent;
	dlg->udata2 = done_data;

	dlg->items[0].type = D_FIELD;
	dlg->items[0].dlen = MAX_STR_LEN;
	dlg->items[0].data = name;
	if (dialog_type == EDIT_DLG_ADD) dlg->items[0].fn = check_nonempty;

	dlg->items[1].type = D_FIELD;
	dlg->items[1].dlen = MAX_STR_LEN;
	dlg->items[1].data = url;
	/* if (dialog_type == EDIT_DLG_ADD) d->items[1].fn = check_nonempty; */

	dlg->items[2].type = D_BUTTON;
	dlg->items[2].gid = B_ENTER;
	dlg->items[2].fn = ok_dialog;
	dlg->items[2].text = _("OK", term);

	dlg->items[3].type = D_BUTTON;
	dlg->items[3].gid = 0;
	dlg->items[3].text = _("Clear", term);
	dlg->items[3].fn = clear_dialog;

	dlg->items[4].type = D_BUTTON;
	dlg->items[4].gid = B_ESC;
	dlg->items[4].text = _("Cancel", term);
	dlg->items[4].data = (void *) when_cancel;
	dlg ->items[4].fn = when_cancel ? my_cancel_dialog : cancel_dialog;

	dlg->items[EDIT_DIALOG_FIELDS_NB].type = D_END;

	do_dialog(term, dlg, getml(dlg, NULL));

#undef EDIT_DIALOG_FIELDS_NB
}
