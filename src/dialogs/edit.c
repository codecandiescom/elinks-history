/* Generic support for edit/search historyitem/bookmark dialog */
/* $Id: edit.c,v 1.74 2003/11/23 17:07:13 jonas Exp $ */

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


static int
my_cancel_dialog(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	((void (*)(struct dialog *)) dlg_data->dlg->widgets[4].data)(dlg_data->dlg);
	return cancel_dialog(dlg_data, widget_data);
}


/* Edits an item's fields.
 * If parent is defined, then that points to a dialog that should be sent
 * an update when the add is done.
 *
 * If either of src_name or src_url are NULL, try to obtain the name and url
 * of the current document. If you want to create two null fields, pass in a
 * pointer to a zero length string (""). */
/* TODO: In bookmark/dialogs.c most users seem to want also the dialog_data
 * so we should make when_*() functions take dialog_data instead. --jonas */
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
	unsigned char *name, *url;
	struct dialog *dlg;

	if (intl) title = _(title, term);

	/* Number of fields in edit dialog --Zas */
#define EDIT_WIDGETS_COUNT 5

	/* Create the dialog */
	dlg = calloc_dialog(EDIT_WIDGETS_COUNT, 2 * MAX_STR_LEN);
	if (!dlg) return;

	name = (unsigned char *) &dlg->widgets[EDIT_WIDGETS_COUNT];
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
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.maximize_width = 1;
	dlg->refresh = (void (*)(void *)) when_done;
	dlg->refresh_data = dlg;
	dlg->udata = parent;
	dlg->udata2 = done_data;

	add_dlg_field(dlg, _("Name", term), 0, 0, NULL, MAX_STR_LEN, name, NULL);
	if (dialog_type == EDIT_DLG_ADD)
		dlg->widgets[dlg->widgets_size - 1].fn = check_nonempty;

	add_dlg_field(dlg, _("URL", term), 0, 0, NULL, MAX_STR_LEN, url, NULL);
	/* if (dialog_type == EDIT_DLG_ADD) d->widgets[n - 1].fn = check_nonempty; */

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, 0, clear_dialog, _("Clear", term), NULL);

	add_dlg_button(dlg, B_ESC, when_cancel ? my_cancel_dialog : cancel_dialog,
			_("Cancel", term), NULL);
	dlg->widgets[dlg->widgets_size - 1].data = (void *) when_cancel;

	add_dlg_end(dlg, EDIT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));

#undef EDIT_WIDGETS_COUNT
}
