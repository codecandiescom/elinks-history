/* Downloads managment */
/* $Id: download.c,v 1.125 2003/10/30 17:01:42 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_CYGWIN_H
#include <sys/cygwin.h>
#endif
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <utime.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "mime/mime.h"
#include "osdep/osdep.h"
#include "protocol/http/date.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/download.h"
#include "sched/error.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: tp_*() should be in separate file, I guess? --pasky */


INIT_LIST_HEAD(downloads);


int
are_there_downloads(void)
{
	struct file_download *down;

	foreach (down, downloads)
		if (!down->prog)
			return 1;

	return 0;
}


static struct session *
get_download_ses(struct file_download *down)
{
	struct session *ses;

	foreach (ses, sessions)
		if (ses == down->ses)
			return ses;

	if (!list_empty(sessions))
		return sessions.next;

	return NULL;
}


static void
abort_download(struct file_download *down, int stop)
{
	if (down->win) delete_window(down->win);
	if (down->ask) delete_window(down->ask);
	if (down->download.state >= 0)
		change_connection(&down->download, NULL, PRI_CANCEL, stop);
	if (down->url) mem_free(down->url);

	if (down->handle != -1) {
		prealloc_truncate(down->handle, down->last_pos);
		close(down->handle);
	}

	if (down->prog) {
		unlink(down->file);
		mem_free(down->prog);
	}

	if (down->file) mem_free(down->file);
	del_from_list(down);
	mem_free(down);
}


static void
kill_downloads_to_file(unsigned char *file)
{
	struct file_download *file_download;

	foreach (file_download, downloads) {
		if (!strcmp(file_download->file, file)) {
			file_download = file_download->prev;
			abort_download(file_download->next, 0);
		}
	}
}


void
abort_all_downloads(void)
{
	while (!list_empty(downloads))
		abort_download(downloads.next, 0 /* does it matter? */);
}


void
destroy_downloads(struct session *ses)
{
	struct file_download *file_download;

	foreach (file_download, downloads) {
		if (file_download->ses == ses && file_download->prog) {
			file_download = file_download->prev;
			abort_download(file_download->next, 0);
		}
	}
}


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
download_progress_bar(struct terminal *term, struct download *download,
	     	      int x, int *y, int w,
		      struct color_pair *text_color,
		      struct color_pair *meter_color)
{
	/* FIXME: not yet perfect, pasky will improve it later. --Zas */
	/* Note : values > 100% are theorically possible and were seen. */
	unsigned char q[] = "XXXX%"; /* Reduce or enlarge at will. */
	const unsigned int qwidth = sizeof(q) - 1;
	unsigned int qlen = 0;
	int p = w - qwidth; /* width for gauge meter */
	int progress = (int) ((longlong) 100 * (longlong) download->prg->pos
			      / (longlong) download->prg->size);
	int barprogress = p * progress / 100;

	int_upper_bound(&barprogress, p); /* Limit to preserve display. */

	if (ulongcat(q, &qlen, progress, qwidth - 1, 0) > 0)
		memset(q, '?', qlen); /* Too long, we limit to preserve display. */

	q[qlen++] = '%'; /* on error, will print '%' only, should not occur. */
	q[qlen] = '\0';

	(*y)++;
	draw_char_data(term, x, *y, '[');
	draw_char_data(term, x + w - qwidth, *y, ']');
	draw_area(term, x + 1, *y, barprogress, 1, ' ', 0, meter_color);
	draw_text(term, x + w - qlen + 1, *y, q, qlen, 0, text_color);
	(*y)++;
}

static void
download_window_function(struct dialog_data *dlg_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, x, y;
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

	w = term->width * 9 / 10 - 2 * DIALOG_LB;
	int_lower_bound(&w, 0);

	url_len = strlen(file_download->url);
	url = memacpy(file_download->url, url_len);
	if (!url) {
		done_string(&msg);
		return;
	} else {
		unsigned char *p = strchr(url, POST_CHAR);

		if (p) {
			url_len = p - url;
			url[url_len + 1] = '\0';
		}

		/* Truncate too long urls */
		if (url_len > w) {
			url_len = w;
			url[url_len + 1] = '\0';
			if (url_len > 4) {
				url[url_len--] = '.';
				url[url_len--] = '.';
				url[url_len--] = '.';
			}
		}
	}

	text_width(term, url, &min, &max);
	text_width(term, msg.source, &min, &max);
	buttons_width(dlg_data->widgets_data, dlg_data->n, &min, &max);

	int_bounds(&w, min, term->width - 2 * DIALOG_LB);
	if (t && download->prg->size >= 0) {
		int_lower_bound(&w, DOWN_DLG_MIN);
	} else {
		int_upper_bound(&w, max);
	}
	int_lower_bound(&w, 1);

	y = 0;
	dlg_format_text(NULL, term, url, 0, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	if (t && download->prg->size >= 0) y += 2;
	dlg_format_text(NULL, term, msg.source, 0, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	dlg_format_buttons(NULL, term, dlg_data->widgets_data, dlg_data->n, 0, &y, w,
			   NULL, AL_CENTER);

	dlg_data->xw = w + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;

	center_dlg(dlg_data);
	draw_dlg(dlg_data);

	y = dlg_data->y + DIALOG_TB + 1;
	x = dlg_data->x + DIALOG_LB;
	dlg_format_text(term, term, url, x, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	if (t && download->prg->size >= 0)
		download_progress_bar(term, download, x, &y, w,
				      dialog_text_color,
			     	      get_bfu_color(term, "dialog.meter"));

	y++;
	dlg_format_text(term, term, msg.source, x, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	dlg_format_buttons(term, term, dlg_data->widgets_data, dlg_data->n, x, &y, w,
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
	int n = 0;

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
	dlg->fn = download_window_function;
	dlg->abort = download_abort_function;
	dlg->udata = down;
	dlg->align = AL_CENTER;

	add_dlg_button(dlg, n, B_ENTER | B_ESC, dlg_undisplay_download, _("Background", term), NULL);
	add_dlg_button(dlg, n, B_ENTER | B_ESC, dlg_set_notify, _("Background with notify", term), NULL);
	add_dlg_button(dlg, n, 0, dlg_abort_download, _("Abort", term), NULL);

	dlg->widgets_size = n;

	assert(n == DOWNLOAD_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
}

static void
set_file_download_win_handler(struct file_download *file_download)
{
	if (file_download->win) {
		struct term_event ev = INIT_TERM_EVENT(
					EV_REDRAW,
				   	file_download->win->term->width,
				    	file_download->win->term->height,
				    	0);

		file_download->win->handler(file_download->win, &ev, 0);
	}
}

static void
download_error_dialog(struct file_download *file_download, int saved_errno)
{
	unsigned char *msg = stracpy(file_download->file);
	unsigned char *emsg = stracpy((unsigned char *) strerror(saved_errno));

	if (msg && emsg) {
		struct terminal *term = get_download_ses(file_download)->tab->term;

		msg_box(term, getml(msg, emsg, NULL), MSGBOX_FREE_TEXT,
			N_("Download error"), AL_CENTER,
			msg_text(term, N_("Could not create file %s: %s"), msg, emsg),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
	} else {
		if (msg) mem_free(msg);
		if (emsg) mem_free(emsg);
	}
}

int
write_cache_entry_to_file(struct cache_entry *ce, struct file_download *file_download)
{
	struct fragment *frag;

	if (file_download->download.prg && file_download->download.prg->seek) {
		file_download->last_pos = file_download->download.prg->seek;
		file_download->download.prg->seek = 0;
		/* This is exclusive with the prealloc, thus we can perform
		 * this in front of that thing safely. */
		if (lseek(file_download->handle, file_download->last_pos, SEEK_SET) < 0)
			goto write_error;
	}

	foreach (frag, ce->frag) {
		int remain = file_download->last_pos - frag->offset;

		if (remain >= 0 && frag->length > remain) {
			int *h = &file_download->handle;
			int w;

#ifdef USE_OPEN_PREALLOC
			if (!file_download->last_pos
			    && (!file_download->stat.prg
				|| file_download->stat.prg->size > 0)) {
				close(*h);
				*h = open_prealloc(file_download->file,
						   O_CREAT|O_WRONLY|O_TRUNC,
						   0666,
						   file_download->stat.prg
						   ? file_download->stat.prg->size
						   : ce->length);
				if (*h == -1) goto write_error;
				set_bin(*h);
			}
#endif

			w = safe_write(*h, frag->data + remain, frag->length - remain);
			if (w == -1) goto write_error;

			file_download->last_pos += w;
		}
	}

	return 1;

write_error:
	if (!list_empty(sessions)) download_error_dialog(file_download, errno);

	return 0;
}

static void
download_data(struct download *download, struct file_download *file_download)
{
	struct cache_entry *ce;
	int broken_302_redirect;

	if (download->state >= S_WAIT && download->state < S_TRANS)
		goto end_store;

	ce = download->ce;
	if (!ce) goto end_store;

	if (ce->last_modified)
		file_download->remotetime = parse_http_date(ce->last_modified);

	broken_302_redirect = get_opt_int("protocol.http.bugs.broken_302_redirect");

	while (ce->redirect && file_download->redirect_cnt++ < MAX_REDIRECTS) {
		unsigned char *u;

		if (download->state >= 0)
			change_connection(&file_download->download, NULL, PRI_CANCEL, 0);

		u = join_urls(file_download->url, ce->redirect);
		if (!u) break;

		if (!broken_302_redirect && !ce->redirect_get) {
			unsigned char *p = strchr(file_download->url, POST_CHAR);

			if (p) add_to_strn(&u, p);
		}

		mem_free(file_download->url);

		file_download->url = u;
		file_download->download.state = S_WAIT_REDIR;

		set_file_download_win_handler(file_download);

		load_url(file_download->url, ce->url, &file_download->download,
			 PRI_DOWNLOAD, NC_CACHE,
			 download->prg ? download->prg->start : 0);

		return;
	}

	if (!write_cache_entry_to_file(ce, file_download)) {
		detach_connection(download, file_download->last_pos);
		abort_download(file_download, 0);
		return;
	}

	detach_connection(download, file_download->last_pos);

end_store:
	if (download->state < 0) {
		struct terminal *term = get_download_ses(file_download)->tab->term;

		if (download->state != S_OK) {
			unsigned char *t = get_err_msg(download->state, term);

			if (t) {
				unsigned char *tt = stracpy(file_download->url);

				if (tt) {
					unsigned char *p = strchr(tt, POST_CHAR);

					if (p) *p = '\0';

					msg_box(term, getml(tt, NULL), MSGBOX_FREE_TEXT,
						N_("Download error"), AL_CENTER,
						msg_text(term, N_("Error downloading %s:\n\n%s"), tt, t),
						get_download_ses(file_download), 1,
						N_("OK"), NULL, B_ENTER | B_ESC /*,
						N_(T_RETRY), NULL, 0 */ /* FIXME: retry */);
				}
			}

		} else {
			if (file_download->prog) {
				prealloc_truncate(file_download->handle,
						  file_download->last_pos);
				close(file_download->handle);
				file_download->handle = -1;
				exec_on_terminal(get_download_ses(file_download)->tab->term,
						 file_download->prog, file_download->file,
						 !!file_download->prog_flags);
				mem_free(file_download->prog);
				file_download->prog = NULL;

			} else {
				if (file_download->notify) {
					unsigned char *url = stracpy(file_download->url);

					msg_box(term, getml(url, NULL), MSGBOX_FREE_TEXT,
						N_("Download"), AL_CENTER,
						msg_text(term, N_("Download complete:\n%s"), url),
						get_download_ses(file_download), 1,
						N_("OK"), NULL, B_ENTER | B_ESC);
				}

				if (get_opt_int("document.download.notify_bell")
				    + file_download->notify >= 2) {
					beep_terminal(get_download_ses(file_download)->tab->term);
				}

				if (file_download->remotetime
				    && get_opt_int("document.download.set_original_time")) {
					struct utimbuf foo;

					foo.actime = foo.modtime = file_download->remotetime;
					utime(file_download->file, &foo);
				}
			}
		}

		abort_download(file_download, 0);
		return;
	}

	set_file_download_win_handler(file_download);
}


/* XXX: We assume that resume is everytime zero in lun's callbacks. */
struct lun_hop {
	struct terminal *term;
	unsigned char *ofile, *file;

	void (*callback)(struct terminal *, unsigned char *, void *, int);
	void *data;
};

static void
lun_alternate(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, lun_hop->file, lun_hop->data, 0);
	if (lun_hop->ofile) mem_free(lun_hop->ofile);
	mem_free(lun_hop);
}

static void
lun_overwrite(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, lun_hop->ofile, lun_hop->data, 0);
	if (lun_hop->file) mem_free(lun_hop->file);
	mem_free(lun_hop);
}

static void
lun_resume(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, lun_hop->ofile, lun_hop->data, 1);
	if (lun_hop->file) mem_free(lun_hop->file);
	mem_free(lun_hop);
}

static void
lun_cancel(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, NULL, lun_hop->data, 0);
	if (lun_hop->ofile) mem_free(lun_hop->ofile);
	if (lun_hop->file) mem_free(lun_hop->file);
	mem_free(lun_hop);
}

static void
lookup_unique_name(struct terminal *term, unsigned char *ofile, int resume,
		   void (*callback)(struct terminal *, unsigned char *, void *, int),
		   void *data)
{
	struct lun_hop *lun_hop;
	unsigned char *file;
	int overwrite;

	ofile = expand_tilde(ofile);

	/* Minor code duplication to prevent useless call to get_opt_int()
	 * if possible. --Zas */
	if (resume) {
		callback(term, ofile, data, resume);
		return;
	}

	/* !overwrite means always silently overwrite, which may be admitelly
	 * indeed a little confusing ;-) */
	overwrite = get_opt_int("document.download.overwrite");
	if (!overwrite) {
		/* Nothing special to do... */
		callback(term, ofile, data, resume);
		return;
	}

	/* Check if the file already exists (file != ofile). */
	file = get_unique_name(ofile);

	if (!file || overwrite == 1 || file == ofile) {
		/* Still nothing special to do... */
		if (file != ofile) mem_free(ofile);
		callback(term, file, data, 0);
		return;
	}

	/* overwrite == 2 (ask) and file != ofile (=> original file already
	 * exists) */

	lun_hop = mem_calloc(1, sizeof(struct lun_hop));
	if (!lun_hop) {
		if (file != ofile) mem_free(file);
		mem_free(ofile);
		callback(term, NULL, data, 0);
		return;
	}
	lun_hop->term = term;
	lun_hop->ofile = ofile;
	lun_hop->file = (file != ofile) ? file : stracpy(ofile);
	lun_hop->callback = callback;
	lun_hop->data = data;

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("File exists"), AL_CENTER,
		msg_text(term, N_("This file already exists:\n"
			"%s\n\n"
			"The alternative filename is:\n"
			"%s"),
			lun_hop->ofile ? lun_hop->ofile : (unsigned char *) "",
			file ? file : (unsigned char *) ""),
		lun_hop, 4,
		N_("Save under the alternative name"), lun_alternate, B_ENTER,
		N_("Overwrite the original file"), lun_overwrite, 0,
		N_("Resume download of the original file"), lun_resume, 0,
		N_("Cancel"), lun_cancel, B_ESC);
}


static void create_download_file_do(struct terminal *, unsigned char *, void *, int);

struct cdf_hop {
	unsigned char **real_file;
	int safe;

	void (*callback)(struct terminal *, int, void *, int);
	void *data;
};

void
create_download_file(struct terminal *term, unsigned char *fi,
		     unsigned char **real_file, int safe, int resume,
		     void (*callback)(struct terminal *, int, void *, int),
		     void *data)
{
	struct cdf_hop *cdf_hop = mem_calloc(1, sizeof(struct cdf_hop));
	unsigned char *wd;

	if (!cdf_hop) {
		callback(term, -1, data, 0);
		return;
	}

	cdf_hop->real_file = real_file;
	cdf_hop->safe = safe;
	cdf_hop->callback = callback;
	cdf_hop->data = data;

	/* FIXME: The wd bussiness is probably useless here? --pasky */
	wd = get_cwd();
	set_cwd(term->cwd);

	/* Also the tilde will be expanded here. */
	lookup_unique_name(term, fi, resume, create_download_file_do, cdf_hop);

	if (wd) {
		set_cwd(wd);
		mem_free(wd);
	}
}

static void
create_download_file_do(struct terminal *term, unsigned char *file, void *data,
			int resume)
{
	struct cdf_hop *cdf_hop = data;
	unsigned char *wd;
	int h = -1;
	int saved_errno;
#ifdef NO_FILE_SECURITY
	int sf = 0;
#else
	int sf = cdf_hop->safe;
#endif

	if (!file) goto finish;

	wd = get_cwd();
	set_cwd(term->cwd);

	/* O_APPEND means repositioning at the end of file before each write(),
	 * thus ignoring seek()s and that can hide mysterious bugs. IMHO.
	 * --pasky */
	h = open(file, O_CREAT | O_WRONLY | (resume ? 0 : O_TRUNC)
			| (sf && !resume ? O_EXCL : 0),
		 sf ? 0600 : 0666);
	saved_errno = errno; /* Saved in case of ... --Zas */

	if (wd) {
		set_cwd(wd);
		mem_free(wd);
	}

	if (h == -1) {
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Download error"), AL_CENTER,
			msg_text(term, N_("Could not create file '%s':\n%s"),
				file, strerror(saved_errno)),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);

		mem_free(file);
		goto finish;

	} else {
		set_bin(h);

		if (!cdf_hop->safe) {
			unsigned char *download_dir = get_opt_str("document.download.directory");
			int i;

			safe_strncpy(download_dir, file, MAX_STR_LEN);

			/* Find the used directory so it's available in history */
			for (i = strlen(download_dir); i >= 0; i--)
				if (dir_sep(download_dir[i]))
					break;
			download_dir[i + 1] = 0;
		}
	}

	if (cdf_hop->real_file)
		*cdf_hop->real_file = file;
	else
		mem_free(file);

finish:
	cdf_hop->callback(term, h, cdf_hop->data, resume);
	mem_free(cdf_hop);
	return;
}


static unsigned char *
get_temp_name(unsigned char *url)
{
	struct string name;
	unsigned char *extension;
	/* FIXME
	 * We use tempnam() here, which is unsafe (race condition), for now.
	 * This should be changed at some time, but it needs an in-depth work
	 * of whole download code. --Zas */
	unsigned char *nm = tempnam(NULL, ELINKS_TEMPNAME_PREFIX);

	if (!nm) return NULL;

	if (!init_string(&name)) {
		mem_free(nm);
		return NULL;
	}

	add_to_string(&name, nm);
	free(nm);

	extension = get_extension_from_url(url);
	if (extension) {
		add_char_to_string(&name, '.');
		add_shell_safe_to_string(&name, extension, strlen(extension));
		mem_free(extension);
	}

	return name.source;
}


unsigned char *
subst_file(unsigned char *prog, unsigned char *file)
{
	struct string name;

	if (!init_string(&name)) return NULL;

	while (*prog) {
		register int p;

		for (p = 0; prog[p] && prog[p] != '%'; p++);

		add_bytes_to_string(&name, prog, p);
		prog += p;

		if (*prog == '%') {
#if defined(HAVE_CYGWIN_CONV_TO_FULL_WIN32_PATH)
#ifdef MAX_PATH
			unsigned char new_path[MAX_PATH];
#else
			unsigned char new_path[1024];
#endif

			cygwin_conv_to_full_win32_path(file, new_path);
			add_to_string(&name, new_path);
#else
			add_to_string(&name, file);
#endif
			prog++;
		}
	}

	return name.source;
}


static void common_download_do(struct terminal *, int, void *, int);

struct cmdw_hop {
	struct session *ses;
	unsigned char *real_file;
};

static void
common_download(struct session *ses, unsigned char *file, int resume)
{
	struct cmdw_hop *cmdw_hop;

	if (!ses->dn_url) return;

	cmdw_hop = mem_calloc(1, sizeof(struct cmdw_hop));
	if (!cmdw_hop) return;
	cmdw_hop->ses = ses;

	kill_downloads_to_file(file);

	create_download_file(ses->tab->term, file, &cmdw_hop->real_file, 0,
			     resume, common_download_do, cmdw_hop);
}

static void
common_download_do(struct terminal *term, int fd, void *data, int resume)
{
	struct cmdw_hop *cmdw_hop = data;
	struct file_download *file_download = NULL;
	unsigned char *url = cmdw_hop->ses->dn_url;
	struct stat buf;

	if (!cmdw_hop->real_file) goto download_error;

	file_download = mem_calloc(1, sizeof(struct file_download));
	if (!file_download) goto download_error;

	file_download->url = stracpy(url);
	if (!file_download->url) goto download_error;

	file_download->file = cmdw_hop->real_file;

	if (fstat(fd, &buf)) goto download_error;
	file_download->last_pos = resume ? (int) buf.st_size : 0;

	file_download->download.end = (void (*)(struct download *, void *)) download_data;
	file_download->download.data = file_download;
	file_download->handle = fd;
	file_download->ses = cmdw_hop->ses;
	file_download->remotetime = 0;

	add_to_list(downloads, file_download);
	load_url(url, cmdw_hop->ses->ref_url, &file_download->download, PRI_DOWNLOAD, NC_CACHE,
		 (resume ? file_download->last_pos : 0));
	display_download(cmdw_hop->ses->tab->term, file_download, cmdw_hop->ses);

	mem_free(cmdw_hop);
	return;

download_error:
	if (file_download) {
		if (file_download->url) mem_free(file_download->url);
		mem_free(file_download);
	}
	mem_free(cmdw_hop);
}

void
start_download(struct session *ses, unsigned char *file)
{
	common_download(ses, file, 0);
}

void
resume_download(struct session *ses, unsigned char *file)
{
	common_download(ses, file, 1);
}


void tp_cancel(struct session *);
void tp_free(struct session *);


static void continue_download_do(struct terminal *, int, void *, int);

struct codw_hop {
	struct session *ses;
	unsigned char *real_file;
	unsigned char *file;
};

static void
continue_download(struct session *ses, unsigned char *file)
{
	struct codw_hop *codw_hop;
	unsigned char *url = ses->tq_url;

	if (!url) return;

	if (ses->tq_prog) {
		/* FIXME: get_temp_name() calls tempnam(). --Zas */
		file = get_temp_name(url);
		if (!file) {
			tp_cancel(ses);
			return;
		}
	}

	codw_hop = mem_calloc(1, sizeof(struct codw_hop));
	if (!codw_hop) return; /* XXX: Something for mem_free()...? --pasky */
	codw_hop->ses = ses;
	codw_hop->file = file;

	kill_downloads_to_file(file);

	create_download_file(ses->tab->term, file, &codw_hop->real_file,
			     !!ses->tq_prog, 0, continue_download_do, codw_hop);
}

static void
continue_download_do(struct terminal *term, int fd, void *data, int resume)
{
	struct codw_hop *codw_hop = data;
	struct file_download *file_download = NULL;
	unsigned char *url = codw_hop->ses->tq_url;

	if (!codw_hop->real_file) goto cancel;

	file_download = mem_calloc(1, sizeof(struct file_download));
	if (!file_download) goto cancel;

	file_download->url = stracpy(url);
	if (!file_download->url) goto cancel;

	file_download->file = codw_hop->real_file;

	file_download->download.end = (void (*)(struct download *, void *)) download_data;
	file_download->download.data = file_download;
	file_download->last_pos = 0;
	file_download->handle = fd;
	file_download->ses = codw_hop->ses;

	if (codw_hop->ses->tq_prog) {
		file_download->prog = subst_file(codw_hop->ses->tq_prog, codw_hop->file);
		mem_free(codw_hop->file);
		mem_free(codw_hop->ses->tq_prog);
		codw_hop->ses->tq_prog = NULL;
	}

	file_download->prog_flags = codw_hop->ses->tq_prog_flags;
	add_to_list(downloads, file_download);
	change_connection(&codw_hop->ses->tq, &file_download->download, PRI_DOWNLOAD, 0);
	tp_free(codw_hop->ses);
	display_download(codw_hop->ses->tab->term, file_download, codw_hop->ses);

	mem_free(codw_hop);
	return;

cancel:
	tp_cancel(codw_hop->ses);
	if (codw_hop->ses->tq_prog && codw_hop->file) mem_free(codw_hop->file);
	if (file_download) {
		if (file_download->url) mem_free(file_download->url);
		mem_free(file_download);
	}
	mem_free(codw_hop);
}


void
tp_free(struct session *ses)
{
	ses->tq_ce->refcount--;
	mem_free(ses->tq_url);
	ses->tq_url = NULL;
	if (ses->tq_goto_position) {
		mem_free(ses->tq_goto_position);
		ses->tq_goto_position = NULL;
	}
	ses->tq_ce = NULL;
}


void
tp_cancel(struct session *ses)
{
	/* XXX: Should we really abort? (1 vs 0 as the last param) --pasky */
	change_connection(&ses->tq, NULL, PRI_CANCEL, 1);
	tp_free(ses);
}


static void
tp_save(struct session *ses)
{
	if (ses->tq_prog) {
		mem_free(ses->tq_prog);
		ses->tq_prog = NULL;
	}
	query_file(ses, ses->tq_url, continue_download, tp_cancel, 1);
}


static void
tp_open(struct session *ses)
{
	continue_download(ses, "");
}


/* FIXME: We need to modify this function to take frame data instead, as we
 * want to use this function for frames as well (now, when frame has content
 * type text/plain, it is ignored and displayed as HTML). */
static void
tp_display(struct session *ses)
{
	struct location *l;

	/* strlen() is ok here, NUL char is in struct view_state */
	l = mem_alloc(sizeof(struct location) + strlen(ses->tq_url));
	if (!l) return;
	memset(l, 0, sizeof(struct location));

	init_list(l->frames);
	memcpy(&l->download, &ses->tq, sizeof(struct download));

	init_vs(&l->vs, ses->tq_url);
	if (ses->tq_goto_position) {
		l->vs.goto_position = ses->tq_goto_position;
		ses->tq_goto_position = NULL;
	}

	add_to_history(&ses->history, l);
	cur_loc(ses)->download.end = (void (*)(struct download *, void *))
				     doc_end_load;
	cur_loc(ses)->download.data = ses;

	if (ses->tq.state >= 0)
		change_connection(&ses->tq, &cur_loc(ses)->download, PRI_MAIN, 0);
	else
		cur_loc(ses)->download.state = ses->tq.state;

	/* This fixes a crash with GCC 3.2.3-6 on Debian
	 * when displaying a file of an unknown type.
	 *
	 * We used ...gcc_3_3 before I changed it to ...gcc_3_x,
	 * so presumably this fixes a crash with that, too.
	 *  -- Miciah
	 */
	do_not_optimize_here_gcc_3_x(ses);
	cur_loc(ses)->vs.plain = 1;
	display_timer(ses);
	tp_free(ses);
}


static void
type_query(struct session *ses, unsigned char *ct, struct mime_handler *handler)
{
	struct string filename;
	unsigned char *content_type;

	if (ses->tq_prog) {
		mem_free(ses->tq_prog);
		ses->tq_prog = NULL;
	}

	if (handler) {
		ses->tq_prog = stracpy(handler->program);
		ses->tq_prog_flags = handler->block;
		if (!handler->ask) {
			tp_open(ses);
			return;
		}
	}

	content_type = stracpy(ct);
	if (!content_type) return;

	if (init_string(&filename))
		add_string_uri_filename_to_string(&filename, ses->tq_url);

	/* @filename.source should be last in the getml()s ! (It terminates the
	 * pointers list in case of allocation failure.) */

	if (!handler) {
		if (!get_opt_int_tree(cmdline_options, "anonymous")) {
			msg_box(ses->tab->term, getml(content_type, filename.source, NULL), MSGBOX_FREE_TEXT,
				N_("Unknown type"), AL_CENTER,
				msg_text(ses->tab->term, N_("Would you like to "
					 "save the file '%s' (type: %s) "
					 "or display it?"),
					 filename.source, content_type),
				ses, 3,
				N_("Save"), tp_save, B_ENTER,
				N_("Display"), tp_display, 0,
				N_("Cancel"), tp_cancel, B_ESC);
		} else {
			msg_box(ses->tab->term, getml(content_type, filename.source, NULL), MSGBOX_FREE_TEXT,
				N_("Unknown type"), AL_CENTER,
				msg_text(ses->tab->term, N_("Would you like to "
					 "display the file '%s' (type: %s)?"),
					 filename.source, content_type),
				ses, 2,
				N_("Display"), tp_display, B_ENTER,
				N_("Cancel"), tp_cancel, B_ESC);
		}
	} else {
		unsigned char *description = handler->description;
		unsigned char *desc_sep;

		if (description) {
			desc_sep = "; ";
		} else {
			desc_sep = "";
			description = "";
		}

		if (!get_opt_int_tree(cmdline_options, "anonymous")) {
			/* TODO: Improve the dialog to let the user correct the
			 * used program. */
			msg_box(ses->tab->term, getml(content_type, filename.source, NULL), MSGBOX_FREE_TEXT,
				N_("What to do?"), AL_CENTER,
				msg_text(ses->tab->term, N_("Would you like to "
					 "open the file '%s' (type: %s%s%s)\n"
					 "with '%s', save it or display it?"),
					 filename.source, content_type, desc_sep,
					 description, handler->program),
				ses, 4,
				N_("Open"), tp_open, B_ENTER,
				N_("Save"), tp_save, 0,
				N_("Display"), tp_display, 0,
				N_("Cancel"), tp_cancel, B_ESC);
		} else {
			msg_box(ses->tab->term, getml(content_type, filename.source, NULL), MSGBOX_FREE_TEXT,
				N_("What to do?"), AL_CENTER,
				msg_text(ses->tab->term, N_("Would you like to "
					 "open the file '%s' (type: %s%s%s)\n"
					 "with '%s', or display it?"),
					 filename.source, content_type, desc_sep,
					 description, handler->program),
				ses, 3,
				N_("Open"), tp_open, B_ENTER,
				N_("Display"), tp_display, 0,
				N_("Cancel"), tp_cancel, B_ESC);
		}
	}
}

struct {
	unsigned char *type;
	unsigned int plain:1;
} static known_types[] = {
	{ "text/html",			0 },
	{ "application/xhtml+xml",	0 }, /* RFC 3236 */
	{ "text/plain",			1 },
	{ NULL,				1 },
};

int
ses_chktype(struct session *ses, struct download **download, struct cache_entry *ce)
{
	struct mime_handler *handler;
	unsigned char *ctype = get_content_type(ce->head, ce->url);
	int plaintext = 1;
	int xwin, i;

	if (!ctype) goto end;

	for (i = 0; known_types[i].type; i++)
		if (!strcasecmp(ctype, known_types[i].type)) {
			plaintext = known_types[i].plain;
			goto free_ct;
		}

	xwin = ses->tab->term->environment & ENV_XWIN;
	handler = get_mime_type_handler(ctype, xwin);

	if (!handler && strlen(ctype) >= 4 && !strncasecmp(ctype, "text", 4))
		goto free_ct;

	assertm(!ses->tq_url,
		"Type query to %s already in progress.", ses->tq_url);
	if_assert_failed mem_free(ses->tq_url);

	ses->tq_url = stracpy(ses->loading_url);
	*download = &ses->tq;
	change_connection(&ses->loading, *download, PRI_MAIN, 0);

	ses->tq_ce = ce;
	ses->tq_ce->refcount++;

	if (ses->tq_goto_position) mem_free(ses->tq_goto_position);

	ses->tq_goto_position = ses->goto_position ? stracpy(ses->goto_position) : NULL;
	type_query(ses, ctype, handler);
	mem_free(ctype);
	if (handler) {
		mem_free(handler->program);
		mem_free(handler);
	}

	return 1;

free_ct:
	mem_free(ctype);

end:
	if (plaintext && ses->task_target_frame) *ses->task_target_frame = 0;
	ses_forward(ses);
	cur_loc(ses)->vs.plain = plaintext;
	return 0;
}
