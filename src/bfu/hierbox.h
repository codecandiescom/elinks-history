/* $Id: hierbox.h,v 1.29 2003/11/22 14:02:41 jonas Exp $ */

#ifndef EL__BFU_HIERBOX_H
#define EL__BFU_HIERBOX_H

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "terminal/terminal.h"
#include "util/lists.h"

struct hierbox_browser {
	struct list_head boxes;
	struct list_head *items;
	struct list_head dialogs;
	struct listbox_ops *ops;
};

struct listbox_item *
init_browser_box(struct hierbox_browser *browser, unsigned char *text, void *data);
void done_browser_box(struct hierbox_browser *browser, struct listbox_item *box);
void update_hierbox_browser(struct hierbox_browser *browser);

/* We use hierarchic listbox browsers for the various managers. They consist
 * of a listbox widget and some buttons.
 *
 * @term	The terminal where the browser should appear.
 *
 * @title	The title of the browser. It is automatically localized.
 *
 * @add_size	The size of extra data to be allocated with the dialog.
 *
 * @browser	The browser structure that contains info to setup listbox data
 *		and manage the dialog list to keep instances of the browser in
 *		sync on various terminals.
 *
 * @udata	Is a reference to any data that the dialog could use.
 *
 * @buttons	Denotes the number of buttons given as varadic arguments.
 *		For each button 4 arguments are extracted:
 *			o First the label text. It is automatically localized.
 *			  If NULL, this button is skipped.
 *			o Second a pointer to a widget handler.
 *			o Third any key flags.
 *			o Last any the button data.
 *		XXX: A close button will be installed by default.
 *
 * XXX: Note that the @listbox_data is detached and freed by the dialog handler.
 *	Any other requirements should be handled by installing a specific
 *	dlg->abort handler. */

struct dialog_data *
hierbox_browser(struct terminal *term, unsigned char *title, size_t add_size,
		struct hierbox_browser *browser, void *udata,
		size_t buttons, ...);

int push_hierbox_delete_button(struct dialog_data *dlg_data, struct widget_data *button);

#endif
