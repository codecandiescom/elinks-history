/* $Id: libintl.h,v 1.8 2003/04/14 15:44:26 zas Exp $ */

#ifndef EL__INTL_GETTEXT_LIBINTL_H
#define EL__INTL_GETTEXT_LIBINTL_H

/* This header file provides an interface between ELinks and GNU libintl. I was
 * astonished that there is no libintl.h but libgnuintl.h (and that name seemed
 * ugly ;-), and I also decided that it will be better to introduce a clean
 * interface instead of digging to libgnuintl.h too much. */

/* Contrary to a standard gettext interface, we pass destination charset and
 * language (in form of struct terminal) directly with each call, allowing to
 * easily multiplex between several terminals. */

#include "intl/gettext/libgettext.h"

#include "config/options.h"
#include "intl/charsets.h"
#include "lowlevel/terminal.h"


/* TODO: Ideally, we should internally work only in Unicode - then the need for
 * charsets multiplexing would cease. That'll take some work yet, though.
 * --pasky */

/* Wraps around gettext(), employing charset multiplexing. If you don't care
 * about charset (usually during initialization or when you don't use terminals
 * at all), use gettext() directly. */
static inline unsigned char *
_(unsigned char *msg, struct terminal *term) {
	static int current_charset = -1;
	int new_charset;

	/* Prevent useless (and possibly dangerous) calls. */
	if (!msg || !*msg) return msg;

	/* Prevent useless switching. */
	new_charset = get_opt_int_tree(term->spec, "charset");
	if (current_charset != new_charset) {
		current_charset = new_charset;
		bind_textdomain_codeset(/* PACKAGE */ "elinks",
			get_cp_mime_name(current_charset));
	}

	return (unsigned char *) gettext(msg);
}

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

extern int get_system_language_index();

/* The current state. The state should be initialized a by set_language(0)
 * call. */

extern int current_language, system_language;
extern void set_language(int language);

#endif
