/* Display of downloads progression stuff. */
/* $Id: progress.c,v 1.5 2005/04/18 17:31:56 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "intl/gettext/libintl.h"
#include "sched/progress.h"
#include "terminal/draw.h"
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

void
draw_progress_bar(struct progress *progress, struct terminal *term,
		  int x, int y, int width,
		  unsigned char *text, struct color_pair *meter_color)
{
	/* Note : values > 100% are theorically possible and were seen. */
	int current = (int) ((longlong) 100 * progress->pos / progress->size);
	struct box barprogress;

	/* Draw the progress meter part "[###    ]" */
	if (!text && width > 2) {
		width -= 2;
		draw_text(term, x++, y, "[", 1, 0, NULL);
		draw_text(term, x + width, y, "]", 1, 0, NULL);
	}

	if (!meter_color) meter_color = get_bfu_color(term, "dialog.meter");
	set_box(&barprogress,
		x, y, int_min(width * current / 100, width), 1);
	draw_box(term, &barprogress, ' ', 0, meter_color);

	/* On error, will print '?' only, should not occur. */
	if (text) {
		width = int_min(width, strlen(text));

	} else if (width > 1) {
		static unsigned char percent[] = "????"; /* Reduce or enlarge at will. */
		unsigned int percent_len = 0;
		int max = int_min(sizeof(percent), width) - 1;

		if (ulongcat(percent, &percent_len, current, max, 0)) {
			percent[0] = '?';
			percent_len = 1;
		}

		percent[percent_len++] = '%';

		/* Draw the percentage centered in the progress meter */
		x += (1 + width - percent_len) / 2;

		assert(percent_len <= width);
		width = percent_len;
		text = percent;
	}

	draw_text(term, x, y, text, width, 0, NULL);
}
