/* $Id: libintl.h,v 1.5 2003/01/03 02:29:35 pasky Exp $ */

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
#define _(msg,term) (term=term,gettext(msg))

/* no-op - just for marking */
#define N_(msg) (gettext_noop(msg))


/* Languages table lookups. */

struct language {
	unsigned char *name;
	unsigned char *iso639;
};

extern struct language languages[];

/* These two calls return 1 (english) if the code/name wasn't found. */
extern int name_to_language(unsigned char *name);
extern int iso639_to_language(unsigned char *iso639);

extern unsigned char *language_to_name(int language);
extern unsigned char *language_to_iso639(int language);


/* The current state. The state should be initialized a by set_language(0)
 * call. */

extern int current_language, system_language;
extern void set_language(int language);

#endif
