/* Download dialogs */
/* $Id: download.c,v 1.1 2003/11/26 01:29:20 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "dialogs/menu.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "sched/download.h"
#include "sched/error.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"


static void
undisplay_download(struct file_download *file_download)
{
	if (file_download->win) delete_window(file_download->win);
}

static void
do_abort_download(struct file_download *file_download)
{
	abort_download(file_download, 1);
}

static int
dlg_set_notify(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->notify = 1;
	undisplay_download(file_download);
	return 0;
}

static int
dlg_abort_download(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	register_bottom_half((void (*)(void *)) do_abort_download,
			     dlg_data->dlg->udata);
	return 0;
}


static int
dlg_undisplay_download(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	register_bottom_half((void (*)(void *)) undisplay_download,
			     dlg_data->dlg->udata);
	return 0;
}


static void
download_abort_function(struct dialog_data *dlg_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->win = NULL;
}

static int
download_progress_string(struct terminal *term,
			 struct download *download,
			 struct string *msg)
{
	if (download->state != S_TRANS || !(download->prg->elapsed / 100)) {
		add_to_string(msg, get_err_msg(download->state, term));
		return 0;
	}

	/* FIXME: The following is a PITA from the l10n standpoint. A *big*
	 * one, _("of")-like pearls are a nightmare. Format strings need to
	 * be introduced to this fuggy corner of code as well. --pasky */

	add_to_string(msg, _("Received", term));
	add_char_to_string(msg, ' ');
	add_xnum_to_string(msg, download->prg->pos);

	if (download->prg->size >= 0) {
		add_char_to_string(msg, ' ');
		add_to_string(msg, _("of",term));
		add_char_to_string(msg, ' ');
		add_xnum_to_string(msg, download->prg->size);
		add_char_to_string(msg, ' ');
	}
	if (download->prg->start > 0) {
		add_char_to_string(msg, '(');
		add_xnum_to_string(msg, download->prg->pos
					- download->prg->start);
		add_char_to_string(msg, ' ');
		add_to_string(msg, _("after resume", term));
		add_char_to_string(msg, ')');
	}
	add_char_to_string(msg, '\n');

	if (download->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME)
		add_to_string(msg, _("Average speed", term));
	else
		add_to_string(msg, _("Speed", term));

	add_char_to_string(msg, ' ');
	add_xnum_to_string(msg, (longlong) download->prg->loaded * 10
			        / (download->prg->elapsed / 100));
	add_to_string(msg, "/s");

	if (download->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(msg, ", ");
		add_to_string(msg, _("current speed", term));
		add_char_to_string(msg, ' ');
		add_xnum_to_string(msg, download->prg->cur_loaded
					/ (CURRENT_SPD_SEC *
					   SPD_DISP_TIME / 1000));
		add_to_string(msg, "/s");
	}

	add_char_to_string(msg, '\n');
	add_to_string(msg, _("Elapsed time", term));
	add_char_to_string(msg, ' ');
	add_time_to_string(msg, download->prg->elapsed);

	if (download->prg->size >= 0 && download->prg->loaded > 0) {
		add_to_string(msg, ", ");
		add_to_string(msg, _("estimated time", term));
		add_char_to_string(msg, ' ');
		add_time_to_string(msg, (download->prg->size - download->prg->pos)
					/ ((longlong) download->prg->loaded * 10
					   / (download->prg->elapsed / 100))
					* 1000);
	}

	return 1;
}

static void
download_progress_bar(struct terminal *term,
	     	      int x, int *y, int width,
		      struct color_pair *text_color,
		      struct color_pair *meter_color,
		      longlong current, longlong total)
{
	/* FIXME: not yet perfect, pasky will improve it later. --Zas */
	/* Note : values > 100% are theorically possible and were seen. */
	unsigned char percent[] = "XXXX%"; /* Reduce or enlarge at will. */
	const unsigned int percent_width = sizeof(percent) - 1;
	unsigned int percent_len = 0;
	int gauge_width = width - percent_width; /* width for gauge meter */
	int progress = (int) ((longlong) 100 * current / total);
	int barprogress = gauge_width * progress / 100;

	int_upper_bound(&barprogress, gauge_width); /* Limit to preserve display. */

	if (ulongcat(percent, &percent_len, progress, percent_width - 1, 0) > 0)
		memset(percent, '?', percent_len); /* Too long, we limit to preserve display. */

	percent[percent_len++] = '%'; /* on error, will print '%' only, should not occur. */
	percent[percent_len] = '\0';

	(*y)++;
	draw_char_data(term, x, *y, '[');
	draw_area(term, x + 1, *y, barprogress, 1, ' ', 0, meter_color);
	draw_char_data(term, x + gauge_width, *y, ']');
	draw_text(term, x + width - percent_len + 1, *y, percent, percent_len, 0, text_color);
	(*y)++;
}

static void
download_dialog_layouter(struct dialog_data *dlg_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;
	struct terminal *term = dlg_data->win->term;
	int w = dialog_max_width(term);
	int rw = w;
	int x, y = 0;
	int t = 0;
	int url_len;
	unsigned char *url;
	struct string msg;
	struct download *download = &file_download->download;
	struct color_pair *dialog_text_color = get_bfu_color(term, "dialog.text");

	redraw_below_window(dlg_data->win);
	file_download->win = dlg_data->win;

	if (!init_string(&msg)) return;
	t = download_progress_string(term, download, &msg);

	url = get_no_post_url(file_download->url, &url_len);
	if (!url) {
		done_string(&msg);
		return;
	}

	if (t && download->prg->size >= 0) {
		int_lower_bound(&w, DOWN_DLG_MIN);
	}

	dlg_format_text_do(NULL, url, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);

	y++;
	if (t && download->prg->size >= 0) y += 2;
	dlg_format_text_do(NULL, msg.source, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data, dlg_data->n, 0, &y, w,
			   &rw, AL_CENTER);

	draw_dialog(dlg_data, w, y);

	w = rw;
	if (url_len > w) {
		/* Truncate too long urls */
		url_len = w;
		url[url_len] = '\0';
		if (url_len > 4) {
			url[--url_len] = '.';
			url[--url_len] = '.';
			url[--url_len] = '.';
		}
	}
	y = dlg_data->y + DIALOG_TB + 1;
	x = dlg_data->x + DIALOG_LB;
	dlg_format_text_do(term, url, x, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	if (t && download->prg->size >= 0)
		download_progress_bar(term, x, &y, w,
				      dialog_text_color,
			     	      get_bfu_color(term, "dialog.meter"),
				      download->prg->pos,
				      download->prg->size);

	y++;
	dlg_format_text_do(term, msg.source, x, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data, dlg_data->n, x, &y, w,
			   NULL, AL_CENTER);

	mem_free(url);
	done_string(&msg);
}


void
display_download(struct terminal *term, struct file_download *down,
		 struct session *ses)
{
	struct dialog *dlg;
	struct file_download *file_download;

	foreach (file_download, downloads)
		if (file_download == down)
			goto found;
	return;

#define DOWNLOAD_WIDGETS_COUNT 3
found:
	dlg = calloc_dialog(DOWNLOAD_WIDGETS_COUNT, 0);
	if (!dlg) return;

	undisplay_download(down);
	down->ses = ses;
	dlg->title = _("Download", term);
	dlg->layouter = download_dialog_layouter;
	dlg->abort = download_abort_function;
	dlg->udata = down;

	add_dlg_button(dlg, B_ENTER | B_ESC, dlg_undisplay_download, _("Background", term), NULL);
	add_dlg_button(dlg, B_ENTER | B_ESC, dlg_set_notify, _("Background with notify", term), NULL);
	add_dlg_button(dlg, 0, dlg_abort_download, _("Abort", term), NULL);

	add_dlg_end(dlg, DOWNLOAD_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
}
