/* Display of downloads progression stuff. */
/* $Id: progress.c,v 1.1 2005/04/18 17:00:25 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "sched/progress.h"
#include "intl/gettext/libintl.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *
get_progress_msg(struct progress *progress, struct terminal *term,
		 int wide, int full, unsigned char *separator)
{
	struct string msg;
	int newlines = separator[strlen(separator) - 1] == '\n';

	if (!init_string(&msg)) return NULL;

	/* FIXME: The following is a PITA from the l10n standpoint. A *big*
	 * one, _("of")-like pearls are a nightmare. Format strings need to
	 * be introduced to this fuggy corner of code as well. --pasky */

	add_to_string(&msg, _("Received", term));
	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, progress->pos);
	if (progress->size >= 0) {
		add_char_to_string(&msg, ' ');
		add_to_string(&msg, _("of", term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, progress->size);
	}

	add_to_string(&msg, separator);

	if (wide) {
		/* Do the following only if there is room */

		add_to_string(&msg,
			      _(full ? (newlines ? N_("Average speed")
					         : N_("average speed"))
				     : N_("avg"),
				term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, average_speed(progress));
		add_to_string(&msg, "/s");

		add_to_string(&msg, ", ");
		add_to_string(&msg,
			      _(full ? N_("current speed") : N_("cur"), term));
		add_char_to_string(&msg, ' '),
		add_xnum_to_string(&msg, current_speed(progress));
		add_to_string(&msg, "/s");

		add_to_string(&msg, separator);

		add_to_string(&msg, _(full ? (newlines ? N_("Elapsed time")
						       : N_("elapsed time"))
					   : N_("ETT"),
				   term));
		add_char_to_string(&msg, ' ');
		add_time_to_string(&msg, progress->elapsed);

	} else {
		add_to_string(&msg, _(newlines ? N_("Speed") : N_("speed"),
					term));

		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, average_speed(progress));
		add_to_string(&msg, "/s");
	}

	if (progress->size >= 0 && progress->loaded > 0) {
		add_to_string(&msg, ", ");
		add_to_string(&msg, _(full ? N_("estimated time")
					   : N_("ETA"),
				      term));
		add_char_to_string(&msg, ' ');
		add_time_to_string(&msg, estimated_time(progress));
	}

	return msg.source;
}
