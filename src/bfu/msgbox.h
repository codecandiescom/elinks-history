/* $Id: msgbox.h,v 1.7 2003/06/07 12:40:40 pasky Exp $ */

#ifndef EL__BFU_MSGBOX_H
#define EL__BFU_MSGBOX_H

#include "bfu/align.h"
#include "bfu/button.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

/* This is _the_ dialog function used in almost all parts of the code. It is
 * used to easily format dialogs containing only text and few buttons below.
 *
 * @term	The terminal where the message box should be appear.
 *
 * @mem_list	A list of pointers to allocated memory that should be
 *		free()'d when then dialog is closed. The list can be
 *		initialized using getml(args, NULL) using NULL as the end.
 *		This is useful especially when you pass stuff to @udata
 *		which you want to be free()d when not needed anymore.
 *
 * @title	The title of the message box.
 *
 * @align	Provides info about how @text should be aligned.
 *
 *		If a special flag AL_EXTD_TEXT is bitwise or'd with the
 *		alignment information, @text is free()d upon the dialog's
 *		death. This is equivalent to adding @text to the @mem_list.
 *
 * @text	The info text of the message box. If the text requires
 *		formatting use msg_text(format, args...). This will allocate
 *		a string so remember to @align |= AL_EXTD_TEXT.
 *
 *		If no formatting is needed just pass the string and don't
 *		@align |= AL_EXTD_TEXT or you will get in trouble. ;)
 *
 * @udata	Is a reference to any data that should be passed to
 *		the handlers associated with each button. NULL if none.
 *
 * @buttons	Denotes the number of buttons given as varadic arguments.
 *		For each button 3 arguments are extracted:
 *			o First the label text. If NULL, this button is
 *			  skipped.
 *			o Second pointer to the handler function (taking
 *			  one (void *), which is incidentally the udata).
 *			o Third any flags.
 *
 * Note that you should ALWAYS format the msg_box() call like:
 *
 * msg_box(term, mem_list,
 *         title, align,
 *         text,
 *         udata, M,
 *         label1, handler1, flags1,
 *         ...,
 *         labelM, handlerM, flagsM);
 *
 * ...no matter that it could fit on one line in case of a tiny message box. */
void msg_box(struct terminal *term, struct memory_list *mem_list,
	unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...);

/* This is basically an equivalent to asprintf(), specifically to be used
 * inside of message boxes. Please always use msg_text() instead of asprintf()
 * in msg_box() parameters (ie. own format conversions can be introduced later
 * specifically crafted for message boxes, and so on).
 * The returned string is allocated and may be NULL! */
unsigned char *msg_text(unsigned char *format, ...);

#endif
