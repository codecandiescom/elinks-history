/* $Id: msgbox.h,v 1.4 2003/06/07 01:45:54 jonas Exp $ */

#ifndef EL__BFU_MSGBOX_H
#define EL__BFU_MSGBOX_H

#include "bfu/align.h"
#include "bfu/button.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

/* This is _the_ dialog function used in almost all parts of the code.
 *
 * @term	The terminal where the message box should be appear.
 *
 * @mem_list	A list of pointers to allocated memory that should be
 *		free()'d when then dialog is closed. The list can be
 *		initialized using getml(args, NULL) using NULL as the end.
 *
 * @title	The title of the message box.
 *
 * @align	Provides info about how text should be aligned and
 *		(in a hacked way) if @text string should be free()'d.
 *
 * @text	The info text of the message box. If the text requires
 *		formatting use msg_text(format, args...). This will allocate
 *		a string so remember to OR @align with AL_EXTD_TEXT.
 *
 *		If no formatting is needed just pass the string and don't
 *		OR @align with AL_EXTD_TEXT or you will get in trouble. ;)
 *
 * @udata	Is a reference to any data that should be passed to
 *		the handlers associated with each buttons. NULL if none.
 *
 * @buttons	Denotes the number of buttons given as varadic arguments.
 *		For each button 3 arguments are extracted:
 *			o First the label text.
 *			o Second the functions pointer to the handler.
 *			o Third any flags.
 */
void
msg_box(struct terminal *term, struct memory_list *mem_list,
	unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...);

/* A wrapper around vasprintf() to format gettextized string.
 * The returned string is allocated and may be NULL! */
unsigned char *msg_text(unsigned char *format, ...);

#endif
