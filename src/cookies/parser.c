/* Cookies name-value pairs parser  */
/* $Id: parser.c,v 1.10 2004/06/26 11:53:03 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_COOKIES

#include <stdlib.h>

#include "elinks.h"

#include "cookies/parser.h"
#include "util/string.h"


/* In order to be able to compile parsetst, you should try to use minimum
 * of foreign stuff here. */

#if 0
static inline void
debug_cookie_parser(struct cookie_str *cstr, unsigned char *pos, int ws, int eq)
{
	int namelen = int_max(cstr->nam_end - cstr->str, 0);
	int valuelen = int_max(cstr->val_end - cstr->val_start, 0);

	printf("[%.*s] :: (%.*s) :: %d,%d [%s] %d\n",
	       namelen, cstr->str,
	       valuelen, cstr->val_start,
	       ws, eq, pos, cstr->nam_end - cstr->str);
}
#else
#define debug_cookie_parser(cstr, pos, ws, eq)
#endif


/* Return cstr on success, NULL on failure. */
struct cookie_str *
parse_cookie_str(struct cookie_str *cstr)
{
	unsigned char *pos;
	int last_was_eq = 0;
	int last_was_ws = 0;

	cstr->nam_end = cstr->val_start = cstr->val_end = NULL;

	/* /NAME *= *VALUE *;/ */

	for (pos = cstr->str; *pos != ';' && *pos; pos++) {
		debug_cookie_parser(cstr, pos, last_was_ws, last_was_eq);

		if (*pos == '=') {
			/* End of name reached */
			if (!cstr->nam_end) {
				cstr->nam_end = pos;
				/* This inside the if is protection against
				 * broken sites sending '=' inside values. */
				last_was_eq = 1;
			}
			if (!cstr->val_start) {
				last_was_eq = 1;
			}
			last_was_ws = 0;

		} else if (isspace(*pos)) {
			if (!cstr->nam_end) {
				/* Just after name - end of name reached */
				cstr->nam_end = pos;
			}
			last_was_ws = 1;

		} else if (last_was_eq) {
			/* Start of value reached */
			/* LESS priority than isspace() */
			cstr->val_start = pos;
			last_was_eq = 0;
			last_was_ws = 0;

		} else if (last_was_ws) {
			/* Non-whitespace after whitespace and not just after
			 * '=' - error */
			return 0;
		}

		if (!last_was_ws) {
			/* The NEXT char is ending it! */
			cstr->val_end = pos + 1;
		}
	}

	if (*pos == ';' && last_was_eq && !cstr->val_start) {
		/* Empty value */
		cstr->val_start = pos;
		cstr->val_end = pos;
	}

	if (cstr->str == cstr->nam_end
	    || !cstr->nam_end || !cstr->val_start || !cstr->val_end)
		return NULL;

	return cstr;
}

#endif /* CONFIG_COOKIES */
