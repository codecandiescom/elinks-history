/* $Id: msgbox.h,v 1.9 2003/06/07 13:17:35 pasky Exp $ */

#ifndef EL__BFU_MSGBOX_H
#define EL__BFU_MSGBOX_H

#include "bfu/align.h"
#include "bfu/button.h"
#include "terminal/terminal.h"
#include "util/memlist.h"


/* Bitmask specifying some @msg_box() function parameters attributes or
 * altering function operation. See @msg_box() description for details about
 * the flags effect. */
enum msgbox_flags {
	/* {msg_box(.text)} is dynamically allocated */
	MSGBOX_EXTD_TEXT = 0x1,
	/* All the msg_box() string parameters should be run through gettext
	 * and translated. */
	MSGBOX_INTL = 0x2,
};

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
 * @flags	If the MSGBOX_EXTD_TEXT flag is passed, @text is free()d upon
 *		the dialog's death. This is equivalent to adding @text to the
 *		@mem_list.
 *
 *		If the MSGBOX_INTL flag is passed, @text, @title and button
 *		labels will be run through gettext before being further
 *		processed. Note that if you will dare to do this in conjuction
 *		with msg_text() usage, it is going to break l18n as result of
 *		format string expansion will be localized, not the format
 *		string itself (which would be the right thing).
 *
 * @title	The title of the message box.
 *
 * @align	Provides info about how @text should be aligned.
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
 * msg_box(term, mem_list, flags,
 *         title, align,
 *         text,
 *         udata, M,
 *         label1, handler1, flags1,
 *         ...,
 *         labelM, handlerM, flagsM);
 *
 * ...no matter that it could fit on one line in case of a tiny message box. */
void msg_box(struct terminal *term, struct memory_list *mem_list,
	enum msgbox_flags flags, unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...);

/* This is basically an equivalent to asprintf(), specifically to be used
 * inside of message boxes. Please always use msg_text() instead of asprintf()
 * in msg_box() parameters (ie. own format conversions can be introduced later
 * specifically crafted for message boxes, and so on).
 * The returned string is allocated and may be NULL! */
unsigned char *msg_text(unsigned char *format, ...);

#endif
