/* Download dialogs */
/* $Id: download.c,v 1.12 2003/11/29 20:55:05 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
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
	if (file_download->dlg_data) cancel_dialog(file_download->dlg_data, NULL);
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
	struct file_download *file_download = dlg_data->dlg->udata;

	object_unlock(file_download);
	register_bottom_half((void (*)(void *)) do_abort_download,
			     dlg_data->dlg->udata);
	return 0;
}

static int
push_delete_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->delete = 1;
	object_unlock(file_download);
	register_bottom_half((void (*)(void *)) do_abort_download,
			     dlg_data->dlg->udata);
	return 0;
}

static int
dlg_undisplay_download(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	object_unlock(file_download);
	register_bottom_half((void (*)(void *)) undisplay_download,
			     dlg_data->dlg->udata);
	return 0;
}


static void
download_abort_function(struct dialog_data *dlg_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->dlg_data = NULL;
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
	int url_len;
	unsigned char *url;
	struct download *download = &file_download->download;
	struct color_pair *dialog_text_color = get_bfu_color(term, "dialog.text");
	int t = (download->state == S_TRANS && (download->prg->elapsed / 100));
	unsigned char *msg = get_stat_msg(download, term, 1, 1, "\n");

	redraw_below_window(dlg_data->win);
	file_download->dlg_data = dlg_data;

	if (!msg) return;

	url = get_no_post_url(file_download->url, &url_len);
	if (!url) {
		mem_free(msg);
		return;
	}

	if (t && download->prg->size >= 0) {
		int_lower_bound(&w, DOWN_DLG_MIN);
	}

	dlg_format_text_do(NULL, url, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);

	y++;
	if (t && download->prg->size >= 0) y += 2;
	dlg_format_text_do(NULL, msg, 0, &y, w, &rw,
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
	dlg_format_text_do(term, msg, x, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data, dlg_data->n, x, &y, w,
			   NULL, AL_CENTER);

	mem_free(url);
	mem_free(msg);
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

#define DOWNLOAD_WIDGETS_COUNT 4
found:
	dlg = calloc_dialog(DOWNLOAD_WIDGETS_COUNT, 0);
	if (!dlg) return;

	undisplay_download(down);
	down->ses = ses;
	dlg->title = _("Download", term);
	dlg->layouter = download_dialog_layouter;
	dlg->abort = download_abort_function;
	dlg->udata = down;

	object_lock(down);

	add_dlg_button(dlg, B_ENTER | B_ESC, dlg_undisplay_download, _("Background", term), NULL);
	add_dlg_button(dlg, B_ENTER | B_ESC, dlg_set_notify, _("Background with notify", term), NULL);
	add_dlg_button(dlg, 0, dlg_abort_download, _("Abort", term), NULL);
	add_dlg_button(dlg, 0, push_delete_button, _("Abort and delete file", term), NULL);

	add_dlg_end(dlg, DOWNLOAD_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
}


/* The download manager */

static void
lock_file_download(struct listbox_item *item)
{
	object_lock((struct file_download *)item->udata);
}

static void
unlock_file_download(struct listbox_item *item)
{
	object_unlock((struct file_download *)item->udata);
}

static int
is_file_download_used(struct listbox_item *item)
{
	return is_object_used((struct file_download *)item->udata);
}

static unsigned char *
get_file_download_info(struct listbox_item *item, struct terminal *term,
		enum listbox_info listbox_info)
{
	struct file_download *file_download = item->udata;

	return (listbox_info == LISTBOX_URI)
		? stracpy(file_download->url) : NULL;
}

static int
can_delete_file_download(struct listbox_item *item)
{
	return 1;
}

static void
delete_file_download(struct listbox_item *item, int last)
{
	struct file_download *file_download = item->udata;

	assert(!is_object_used(file_download));

	abort_download(file_download, 1);
}

static enum dlg_refresh_code
refresh_file_download(struct dialog_data *dlg_data, void *data)
{
	/* Always refresh (until we keep finished downloads) */
	return are_there_downloads() ? REFRESH_DIALOG : REFRESH_STOP;
}

/* TODO: Make it configurable */
#define DOWNLOAD_METER_WIDTH 15
#define DOWNLOAD_URI_PERCENTAGE 50

static void
draw_file_download(struct listbox_item *item, struct listbox_context *context,
		   int x, int y, int width)
{
	struct file_download *file_download = item->udata;
	struct download *download = &file_download->download;
	unsigned char *stylename;
	struct color_pair *color;
	unsigned char *text = file_download->url;
	int length = strlen(text);
	int trimmedlen;
	int meter = DOWNLOAD_METER_WIDTH;
	int temp_y;

	/* We have nothing to work with */
	if (width < 4) return;

	stylename = (item == context->box->sel) ? "menu.selected"
		  : ((item->marked)	        ? "menu.marked"
					        : "menu.normal");

	color = get_bfu_color(context->term, stylename);

	/* Show atleast the required percentage of the URI */
	if (length * DOWNLOAD_URI_PERCENTAGE / 100 < width - meter - 4) {
		trimmedlen = int_min(length, width - meter - 4);
	} else {
		trimmedlen = int_min(length, width - 3);
	}

	draw_text(context->term, x, y, text, trimmedlen, 0, color);
	if (trimmedlen < length) {
		draw_text(context->term, x + trimmedlen, y, "...", 3, 0, color);
		trimmedlen += 3;
	}

	if (download->prg->size < 0
	    || download->state != S_TRANS
	    || !(download->prg->elapsed / 100)) {
		/* TODO: Show trimmed error message. */
		return;
	}

	if (!dialog_has_refresh(context->dlg_data))
		refresh_dialog(context->dlg_data, refresh_file_download, NULL);

	if (trimmedlen + meter >= width) return;

	temp_y = y - 1;
	x += width - meter;

	download_progress_bar(context->term, x, &temp_y, meter,
			      get_bfu_color(context->term, "dialog.text"),
			      get_bfu_color(context->term, "dialog.meter"),
			      download->prg->pos,
			      download->prg->size);
}

static struct listbox_ops downloads_listbox_ops = {
	lock_file_download,
	unlock_file_download,
	is_file_download_used,
	get_file_download_info,
	can_delete_file_download,
	delete_file_download,
	draw_file_download,
};


static int
push_info_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct session *ses = dlg_data->dlg->udata;
	struct file_download *file_download = box->sel ? box->sel->udata : NULL;

	assert(ses);

	if (!file_download) return 0;

	/* Don't layer on top of the download manager */
	delete_window(dlg_data->win);

	display_download(term, file_download, ses);
	return 0;
}


static INIT_LIST_HEAD(download_box_items);

/* TODO: Ideas for buttons .. should be pretty trivial most of it
 *
 * - Resume or something that will use some goto like handler
 * - Open button that can be used to set file_download->prog.
 * - Toggle notify button
 */
static struct hierbox_browser_button download_buttons[] = {
	{ N_("Info"),			push_info_button		},
	{ N_("Abort"),			push_hierbox_delete_button	},
#if 0
	/* This requires more work to make locking work and query the user */
	{ N_("Abort and delete file"),	push_delete_button		},
#endif
	{ N_("Clear"),			push_hierbox_clear_button	},
};

struct hierbox_browser download_browser = {
	N_("Download manager"),
	download_buttons,
	HIERBOX_BROWSER_BUTTONS_SIZE(download_buttons),

	{ D_LIST_HEAD(download_browser.boxes) },
	&download_box_items,
	{ D_LIST_HEAD(download_browser.dialogs) },
	&downloads_listbox_ops,
};

void
menu_download_manager(struct terminal *term, void *fcp, struct session *ses)
{
	hierbox_browser(&download_browser, ses);
}
