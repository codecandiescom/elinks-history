/* Cookies name-value pairs parser  */
/* $Id: parser.c,v 1.6 2002/08/06 00:07:07 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "links.h"

#include "cookies/parser.h"
#include "util/string.h"


/* In order to be able to compile parsetst, you should try to use minimum
 * of foreign stuff here. */


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
#if 0
		printf("[%s :: %s] - (%s - %s - %s) %d,%d\n",
		       cstr->str, pos, cstr->nam_end, cstr->val_start,
		       cstr->val_end, last_was_ws, last_was_eq);
#endif

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

		} else if (WHITECHAR(*pos)) {
			if (!cstr->nam_end) {
				/* Just after name - end of name reached */
				cstr->nam_end = pos;
			}
			last_was_ws = 1;

		} else if (last_was_eq) {
			/* Start of value reached */
			/* LESS priority than WHITECHAR() */
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
