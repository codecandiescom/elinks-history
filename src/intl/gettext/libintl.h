/* $Id: libintl.h,v 1.15 2003/06/07 14:43:07 pasky Exp $ */

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
#include "terminal/terminal.h"
#include "util/error.h"

/* Define it to find redundant useless calls */
/* #define DEBUG_IT */

/* TODO: Ideally, we should internally work only in Unicode - then the need for
 * charsets multiplexing would cease. That'll take some work yet, though.
 * --pasky */

#ifndef DEBUG_IT
/* Wraps around gettext(), employing charset multiplexing. If you don't care
 * about charset (usually during initialization or when you don't use terminals
 * at all), use gettext() directly. */
static inline unsigned char *
_(unsigned char *msg, struct terminal *term)
{
	static int current_charset = -1;
	int new_charset;

	/* Prevent useless (and possibly dangerous) calls. */
	if (!term || !msg || !*msg)
		return msg;

	/* Prevent useless switching. */
	new_charset = get_opt_int_tree(term->spec, "charset");
	if (current_charset != new_charset) {
		current_charset = new_charset;
		bind_textdomain_codeset( /* PACKAGE */ "elinks",
					get_cp_mime_name(current_charset));
	}

	return (unsigned char *) gettext(msg);
}
#else

/* This one will emit errors on null/empty msgs and when multiple calls are
 * done for the same result in the same function. Some noise is possible,
 * when a function is called twice or more, but then we should cache msg,
 * in function caller. --Zas */

/* __FUNCTION__ isn't supported by all, but it's debugging code. */
#define _(m, t) __(__FILE__, __LINE__, __FUNCTION__, m, t)

/* overflows are theorically possible here. Debug purpose only. */
static inline unsigned char *
__(unsigned char *file, unsigned int line, unsigned char *func,
   unsigned char *msg, struct terminal *term)
{
	static unsigned char last_file[512] = "";
	static unsigned int last_line = 0;
	static unsigned char last_func[1024] = "";
	static unsigned char last_result[16384] = "";
	static int current_charset = -1;
	int new_charset;
	unsigned char *result;

	/* Prevent useless (and possibly dangerous) calls. */
	if (!term) return msg;
	if (!msg || !*msg) {
		error("%s:%d %s msg parameter", file, line, msg ? "empty": "NULL");
		return msg;
	}

	/* Prevent useless switching. */
	new_charset = get_opt_int_tree(term->spec, "charset");
	if (current_charset != new_charset) {
		current_charset = new_charset;
		bind_textdomain_codeset( /* PACKAGE */ "elinks",
					get_cp_mime_name(current_charset));
	}

	result = (unsigned char *) gettext(msg);

	if (!strcmp(result, last_result)
	    && !strcmp(file, last_file)
	    && !strcmp(func, last_func)) {
		error("%s:%d Duplicate call to _() in %s() (previous at line %d)",
		      file, line, func, last_line);
	}

	/* Risky ;) */
	strcpy(last_file, file);
	strcpy(last_func, func);
	strcpy(last_result, result);
	last_line = line;

	return result;
}

#endif

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

extern int get_system_language_index(void);

/* The current state. The state should be initialized a by set_language(0)
 * call. */

extern int current_language, system_language;
extern void set_language(int language);

#endif
