/* $Id: hierbox.h,v 1.11 2003/11/09 13:53:37 jonas Exp $ */

#ifndef EL__BFU_HIERBOX_H
#define EL__BFU_HIERBOX_H

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "sched/session.h"
#include "terminal/terminal.h"

/* We use hierarchic listbox browsers in the various managers. They consist
 * of a listbox widget some buttons.
 *
 * @term	The terminal where the message box should be appear.
 *
 * @title	The title of the browser. It is automatically localized.
 *
 * @add_size	The size of data to be allocated with the dialog.
 *
 * @listbox_data
 *		The data used for the listbox widget.
 *
 * @udata	Is a reference to any data that should be passed to
 *		the handlers associated with each button. NULL if none.
 *
 * @buttons	Denotes the number of buttons given as varadic arguments.
 *		For each button 4 arguments are extracted:
 *			o First the label text. It is automatically localized.
 *			  If NULL, this button is skipped.
 *			o Second a pointer to a widget handler.
 *			o Third any key flags.
 *			o Last any the button data.
 *
 * XXX: Note that the @listbox_data is detached and freed by the dialog handler.
 *	Any other requirements should be handled by installing a specific
 *	dlg->abort handler. */

struct dialog_data *
hierbox_browser(struct terminal *term, unsigned char *title, size_t add_size,
		struct listbox_data *listbox_data, void *udata,
		size_t buttons, ...);

#endif
