/* Download dialogs */
/* $Id: download.c,v 1.63 2004/10/08 16:54:57 zas Exp $ */

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
#include "dialogs/download.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "protocol/uri.h"
#include "sched/download.h"
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
#include "util/ttime.h"


static void
undisplay_download(struct file_download *file_download)
{
	/* We are maybe called from bottom halve so check consistency */
	if (is_in_downloads_list(file_download) && file_download->dlg_data)
		cancel_dialog(file_download->dlg_data, NULL);
}

static void
do_abort_download(struct file_download *file_download)
{
	/* We are maybe called from bottom halve so check consistency */
	if (is_in_downloads_list(file_download)) {
		file_download->stop = 1;
		abort_download(file_download);
	}
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
			     file_download);
	return 0;
}

static int
push_delete_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->delete = 1;
	object_unlock(file_download);
	register_bottom_half((void (*)(void *)) do_abort_download,
			     file_download);
	return 0;
}

static int
dlg_undisplay_download(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	object_unlock(file_download);
	register_bottom_half((void (*)(void *)) undisplay_download,
			     file_download);
	return 0;
}


static void
download_abort_function(struct dialog_data *dlg_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->dlg_data = NULL;
}

void
download_progress_bar(struct terminal *term, int x, int y, int width,
		      unsigned char *text, struct color_pair *meter_color,
		      longlong current, longlong total)
{
	/* Note : values > 100% are theorically possible and were seen. */
	int progress = (int) ((longlong) 100 * current / total);
	struct box barprogress;

	/* Draw the progress meter part "[###    ]" */
	if (!text && width > 2) {
		width -= 2;
		draw_text(term, x++, y, "[", 1, 0, NULL);
		draw_text(term, x + width, y, "]", 1, 0, NULL);
	}

	if (!meter_color) meter_color = get_bfu_color(term, "dialog.meter");
	set_box(&barprogress,
		x, y, int_min(width * progress / 100, width), 1);
	draw_box(term, &barprogress, ' ', 0, meter_color);

	/* On error, will print '?' only, should not occur. */
	if (text) {
		width = int_min(width, strlen(text));

	} else if (width > 1) {
		static unsigned char percent[] = "????"; /* Reduce or enlarge at will. */
		unsigned int percent_len = 0;
		int max = int_min(sizeof(percent), width) - 1;

		if (ulongcat(percent, &percent_len, progress, max, 0)) {
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
	unsigned char *msg = get_download_msg(download, term, 1, 1, "\n");
	int show_meter = (download_is_progressing(download)
			  && download->progress->size >= 0);

	redraw_below_window(dlg_data->win);
	file_download->dlg_data = dlg_data;

	if (!msg) return;

	url = get_uri_string(file_download->uri, URI_PUBLIC);
	if (!url) {
		mem_free(msg);
		return;
	}
	url_len = strlen(url);

	if (show_meter) {
		int_lower_bound(&w, DOWN_DLG_MIN);
	}

	dlg_format_text_do(NULL, url, 0, &y, w, &rw,
			dialog_text_color, ALIGN_LEFT);

	y++;
	if (show_meter) y += 2;
	dlg_format_text_do(NULL, msg, 0, &y, w, &rw,
			dialog_text_color, ALIGN_LEFT);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data, dlg_data->n, 0, &y, w,
			   &rw, ALIGN_CENTER);

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
	y = dlg_data->box.y + DIALOG_TB + 1;
	x = dlg_data->box.x + DIALOG_LB;
	dlg_format_text_do(term, url, x, &y, w, NULL,
			dialog_text_color, ALIGN_LEFT);

	if (show_meter) {
		y++;
		download_progress_bar(term, x, y, w, NULL, NULL,
				      download->progress->pos,
				      download->progress->size);
		y++;
	}

	y++;
	dlg_format_text_do(term, msg, x, &y, w, NULL,
			dialog_text_color, ALIGN_LEFT);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data, dlg_data->n, x, &y, w,
			   NULL, ALIGN_CENTER);

	mem_free(url);
	mem_free(msg);
}

void
display_download(struct terminal *term, struct file_download *file_download,
		 struct session *ses)
{
	struct dialog *dlg;

	if (!is_in_downloads_list(file_download))
		return;

#define DOWNLOAD_WIDGETS_COUNT 4
	dlg = calloc_dialog(DOWNLOAD_WIDGETS_COUNT, 0);
	if (!dlg) return;

	undisplay_download(file_download);
	file_download->ses = ses;
	dlg->title = _("Download", term);
	dlg->layouter = download_dialog_layouter;
	dlg->abort = download_abort_function;
	dlg->udata = file_download;

	object_lock(file_download);

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
	object_lock((struct file_download *) item->udata);
}

static void
unlock_file_download(struct listbox_item *item)
{
	object_unlock((struct file_download *) item->udata);
}

static int
is_file_download_used(struct listbox_item *item)
{
	return is_object_used((struct file_download *) item->udata);
}

static unsigned char *
get_file_download_text(struct listbox_item *item, struct terminal *term)
{
	struct file_download *file_download = item->udata;

	return get_uri_string(file_download->uri, URI_PUBLIC);
}

static unsigned char *
get_file_download_info(struct listbox_item *item, struct terminal *term)
{
	return NULL;
}

static struct uri *
get_file_download_uri(struct listbox_item *item)
{
	struct file_download *file_download = item->udata;

	return get_uri_reference(file_download->uri);
}

static struct listbox_item *
get_file_download_root(struct listbox_item *item)
{
	return NULL;
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
	register_bottom_half((void (*)(void *)) do_abort_download,
			     file_download);
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
	unsigned char *text = struri(file_download->uri);
	int length = strlen(text);
	int trimmedlen;
	int meter = DOWNLOAD_METER_WIDTH;

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

	if (download->progress->size < 0
	    || download->state != S_TRANS
	    || !(download->progress->elapsed / 100)) {
		/* TODO: Show trimmed error message. */
		return;
	}

	if (!dialog_has_refresh(context->dlg_data))
		refresh_dialog(context->dlg_data, refresh_file_download, NULL);

	if (trimmedlen + meter >= width) return;

	x += width - meter;

	download_progress_bar(context->term, x, y, meter, NULL, NULL,
			      download->progress->pos,
			      download->progress->size);
}

static struct listbox_ops_messages download_messages = {
	/* cant_delete_item */
	N_("Sorry, but download \"%s\" cannot be interrupted."),
	/* cant_delete_used_item */
	N_("Sorry, but download \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Interrupt marked downloads"),
	/* delete_marked_items */
	N_("Interrupt marked downloads?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Interrupt download"),
	/* delete_item */
	N_("Interrupt this download?"),
	/* clear_all_items_title */
	N_("Interrupt all downloads"),
	/* clear_all_items_title */
	N_("Do you really want to interrupt all downloads?"),
};

static struct listbox_ops downloads_listbox_ops = {
	lock_file_download,
	unlock_file_download,
	is_file_download_used,
	get_file_download_text,
	get_file_download_info,
	get_file_download_uri,
	get_file_download_root,
	NULL,
	can_delete_file_download,
	delete_file_download,
	draw_file_download,
	&download_messages,
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

struct_hierbox_browser(
	download_browser,
	N_("Download manager"),
	download_buttons,
	&downloads_listbox_ops
);

void
download_manager(struct session *ses)
{
	hierbox_browser(&download_browser, ses);
}
