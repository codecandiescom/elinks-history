/* $Id: libintl.h,v 1.3 2003/01/03 01:25:48 pasky Exp $ */

#ifndef EL__INTL_GETTEXT_LIBINTL_H
#define EL__INTL_GETTEXT_LIBINTL_H

/* This header file provides an interface between ELinks and GNU libintl. I was
 * astonished that there is no libintl.h but libgnuintl.h (and that name seemed
 * ugly ;-), and I also decided that it will be better to introduce a clean
 * interface instead of digging to libgnuintl.h too much. */

/* Contrary to a standard gettext interface, we pass destination charset and
 * language (in form of struct terminal) directly with each call, allowing to
 * easily multiplex between several terminals. */
/* XXX: And currently we ignore the terminal. Will be fixed soon. --pasky */

#include "intl/gettext/libgettext.h"

/* TODO: Make use of the term. */
#define _(msg,term) gettext(msg)

/* no-op - just for marking */
#define N_(msg) gettext_noop(msg)

/* The current state - the language is stored as a ISO 639 code or string
 * "system" - then value of system_language is used. */
/* The state should be initialized a by set_language("system") call. */
extern unsigned char *current_language, *system_language;
extern void set_language(unsigned char *language);

#endif
