/* Status/error messages managment */
/* $Id: state.c,v 1.1 2003/06/07 14:37:17 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "intl/gettext/intl.h"
#include "sched/sched.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: Move S_* constants to sched/error.h as well? I'm not sure, personally.
 * --pasky */

struct strerror_val {
	LIST_HEAD(struct strerror_val);

	unsigned char msg[1]; /* must be last */
};

static INIT_LIST_HEAD(strerror_buf); /* struct strerror_val */


unsigned char *
get_err_msg(int state)
{
	unsigned char *e;
	struct strerror_val *s;

	if (state <= S_OK || state >= S_WAIT) {
		int i;

		for (i = 0; msg_dsc[i].msg; i++)
			if (msg_dsc[i].n == state)
				return msg_dsc[i].msg;
unknown_error:
		return N_("Unknown error");
	}

	e = (unsigned char *) strerror(-state);
	if (!e || !*e) goto unknown_error;

	foreach (s, strerror_buf)
		if (!strcmp(s->msg, e))
			return s->msg;

	s = mem_alloc(sizeof(struct strerror_val) + strlen(e) + 1);
	if (!s) goto unknown_error;

	strcpy(s->msg, e);
	add_to_list(strerror_buf, s);

	return s->msg;
}

void
free_strerror_buf(void)
{
	free_list(strerror_buf);
}
