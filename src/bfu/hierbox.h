/* $Id: hierbox.h,v 1.12 2003/11/09 14:01:37 jonas Exp $ */

#ifndef EL__BFU_HIERBOX_H
#define EL__BFU_HIERBOX_H

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

/* We use hierarchic listbox browsers for the various managers. They consist
 * of a listbox widget and some buttons.
 *
 * @term	The terminal where the browser should be appear.
 *
 * @title	The title of the browser. It is automatically localized.
 *
 * @add_size	The size of extra data to be allocated with the dialog.
 *
 * @listbox_data
 *		The data used for the listbox widget.
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
		struct listbox_data *listbox_data, void *udata,
		size_t buttons, ...);

#endif
