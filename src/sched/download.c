/* Downloads managment */
/* $Id: download.c,v 1.22 2003/05/04 19:11:16 pasky Exp $ */

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

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "document/cache.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "protocol/http/date.h"
#include "protocol/mailcap.h"
#include "protocol/mime.h"
#include "protocol/url.h"
#include "sched/download.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/sched.h"
#include "sched/session.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/file.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: tp_*() should be in separate file, I guess? --pasky */


INIT_LIST_HEAD(downloads);


int
are_there_downloads()
{
	struct download *down;

	foreach (down, downloads)
		if (!down->prog)
			return 1;

	return 0;
}


static struct session *
get_download_ses(struct download *down)
{
	struct session *ses;

	foreach(ses, sessions)
		if (ses == down->ses)
			return ses;

	if (!list_empty(sessions))
		return sessions.next;

	return NULL;
}


static void
abort_download(struct download *down, int stop)
{
	if (down->win) delete_window(down->win);
	if (down->ask) delete_window(down->ask);
	if (down->stat.state >= 0)
		change_connection(&down->stat, NULL, PRI_CANCEL, stop);
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
	struct download *down;

	foreach (down, downloads) {
		if (!strcmp(down->file, file)) {
			down = down->prev;
			abort_download(down->next, 0);
		}
	}
}


void
abort_all_downloads()
{
	while (!list_empty(downloads))
		abort_download(downloads.next, 0 /* does it matter? */);
}


void
destroy_downloads(struct session *ses)
{
	struct download *d;

	foreach(d, downloads) {
		if (d->ses == ses && d->prog) {
			d = d->prev;
			abort_download(d->next, 0);
		}
	}
}


static void
undisplay_download(struct download *down)
{
	if (down->win) delete_window(down->win);
}

static void
do_abort_download(struct download *down)
{
	abort_download(down, 1);
}


static int
dlg_set_notify(struct dialog_data *dlg, struct widget_data *di)
{
	struct download *down = dlg->dlg->udata;

	down->notify = 1;
	undisplay_download(down);
	return 0;
}

static int
dlg_abort_download(struct dialog_data *dlg, struct widget_data *di)
{
	register_bottom_half((void (*)(void *)) do_abort_download,
			     dlg->dlg->udata);
	return 0;
}


static int
dlg_undisplay_download(struct dialog_data *dlg, struct widget_data *di)
{
	register_bottom_half((void (*)(void *)) undisplay_download,
			     dlg->dlg->udata);
	return 0;
}


static void
download_abort_function(struct dialog_data *dlg)
{
	struct download *down = dlg->dlg->udata;

	down->win = NULL;
}


static void
download_window_function(struct dialog_data *dlg)
{
	struct download *down = dlg->dlg->udata;
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, x, y;
	int t = 0;
	unsigned char *m, *u;
	struct status *status = &down->stat;
	int dialog_text_color = get_bfu_color(term, "dialog.text");

	redraw_below_window(dlg->win);
	down->win = dlg->win;

	if (status->state == S_TRANS && status->prg->elapsed / 100) {
		int l = 0;

		m = init_str();
		if (!m) return;

		t = 1;
		add_to_str(&m, &l, _("Received", term));
		add_to_str(&m, &l, " ");
		add_xnum_to_str(&m, &l, status->prg->pos);

		if (status->prg->size >= 0) {
			add_to_str(&m, &l, " ");
			add_to_str(&m, &l, _("of",term));
			add_to_str(&m, &l, " ");
			add_xnum_to_str(&m, &l, status->prg->size);
			add_to_str(&m, &l, " ");
		}
		if (status->prg->start > 0) {
			add_to_str(&m, &l, "(");
			add_xnum_to_str(&m, &l, status->prg->pos
						- status->prg->start);
			add_to_str(&m, &l, " ");
			add_to_str(&m, &l, _("after resume", term));
			add_to_str(&m, &l, ")");
		}
		add_to_str(&m, &l, "\n");

		if (status->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME)
			add_to_str(&m, &l, _("Average speed", term));
		else add_to_str(&m, &l, _("Speed", term));

		add_to_str(&m, &l, " ");
		add_xnum_to_str(&m, &l, (longlong) status->prg->loaded * 10
					/ (status->prg->elapsed / 100));
		add_to_str(&m, &l, "/s");

		if (status->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_str(&m, &l, ", ");
			add_to_str(&m, &l, _("current speed", term));
			add_to_str(&m, &l, " ");
			add_xnum_to_str(&m, &l, status->prg->cur_loaded
						/ (CURRENT_SPD_SEC *
						   SPD_DISP_TIME / 1000));
			add_to_str(&m, &l, "/s");
		}

		add_to_str(&m, &l, "\n");
		add_to_str(&m, &l, _("Elapsed time", term));
		add_to_str(&m, &l, " ");
		add_time_to_str(&m, &l, status->prg->elapsed);

		if (status->prg->size >= 0 && status->prg->loaded > 0) {
			add_to_str(&m, &l, ", ");
			add_to_str(&m, &l, _("estimated time", term));
			add_to_str(&m, &l, " ");
#if 0
			add_time_to_str(&m, &l, stat->prg->elapsed
						/ 1000 * stat->prg->size
						/ 1000 * stat->prg->loaded
						- stat->prg->elapsed);
#endif
			add_time_to_str(&m, &l, (status->prg->size - status->prg->pos)
						/ ((longlong) status->prg->loaded * 10
						   / (status->prg->elapsed / 100))
						* 1000);
		}

	} else m = stracpy(_(get_err_msg(status->state), term));

	if (!m) return;

	u = stracpy(down->url);
	if (!u) {
		mem_free(m);
		return;
	} else {
		unsigned char *p = strchr(u, POST_CHAR);

		if (p) *p = '\0';
	}

	max_text_width(term, u, &max);
	min_text_width(term, u, &min);
	max_text_width(term, m, &max);
	min_text_width(term, m, &min);
	max_buttons_width(term, dlg->items, dlg->n, &max);
	min_buttons_width(term, dlg->items, dlg->n, &min);

	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w < min) w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB)
		w = dlg->win->term->x - 2 * DIALOG_LB;
	if (t && status->prg->size >= 0) {
		if (w < DOWN_DLG_MIN) w = DOWN_DLG_MIN;
	} else {
		if (w > max) w = max;
	}
	if (w < 1) w = 1;

	y = 0;
	dlg_format_text(NULL, term, u, 0, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	if (t && status->prg->size >= 0) y += 2;
	dlg_format_text(NULL, term, m, 0, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	dlg_format_buttons(NULL, term, dlg->items, dlg->n, 0, &y, w,
			   NULL, AL_CENTER);

	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;

	center_dlg(dlg);
	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	x = dlg->x + DIALOG_LB;
	dlg_format_text(term, term, u, x, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	if (t && status->prg->size >= 0) {
		unsigned char q[64];
		int p = w - 6;

		y++;
		set_only_char(term, x, y, '[');
		set_only_char(term, x + w - 5, y, ']');
		fill_area(term, x + 1, y,
			  (int) ((longlong) p * (longlong) status->prg->pos
				 / (longlong) status->prg->size),
			  1, get_bfu_color(term, "dialog.meter"));
		sprintf(q, "%3d%%",
			  (int) ((longlong) 100 * (longlong) status->prg->pos
				 / (longlong) status->prg->size));
		print_text(term, x + w - 4, y, strlen(q), q, dialog_text_color);
		y++;
	}

	y++;
	dlg_format_text(term, term, m, x, &y, w, NULL,
			dialog_text_color, AL_LEFT);

	y++;
	dlg_format_buttons(term, term, dlg->items, dlg->n, x, &y, w,
			   NULL, AL_CENTER);

	mem_free(u);
	mem_free(m);
}


void
display_download(struct terminal *term, struct download *down,
		 struct session *ses)
{
	struct dialog *dlg;
	struct download *dd;

	foreach (dd, downloads)
		if (dd == down)
			goto found;
	return;

found:
	dlg = mem_calloc(1, sizeof(struct dialog) + 3 * sizeof(struct widget));
	if (!dlg) return;

	undisplay_download(down);
	down->ses = ses;
	dlg->title = N_("Download");
	dlg->fn = download_window_function;
	dlg->abort = download_abort_function;
	dlg->udata = down;
	dlg->align = AL_CENTER;

	dlg->items[0].type = D_BUTTON;
	dlg->items[0].gid = B_ENTER | B_ESC;
	dlg->items[0].fn = dlg_undisplay_download;
	dlg->items[0].text = N_("Background");

	dlg->items[1].type = D_BUTTON;
	dlg->items[1].gid = B_ENTER | B_ESC;
	dlg->items[1].fn = dlg_set_notify;
	dlg->items[1].text = N_("Background with notify");

	dlg->items[2].type = D_BUTTON;
	dlg->items[2].gid = 0;
	dlg->items[2].fn = dlg_abort_download;
	dlg->items[2].text = N_("Abort");

	dlg->items[3].type = D_END;

	do_dialog(term, dlg, getml(dlg, NULL));
}


static void
download_data(struct status *status, struct download *down)
{
	struct cache_entry *ce;
	struct fragment *frag;

	if (status->state >= S_WAIT && status->state < S_TRANS)
		goto end_store;

	ce = status->ce;
	if (!ce) goto end_store;

	if (ce->last_modified)
		down->remotetime = parse_http_date(ce->last_modified);

	while (ce->redirect && down->redirect_cnt++ < MAX_REDIRECTS) {
		unsigned char *u;

		if (status->state >= 0)
			change_connection(&down->stat, NULL, PRI_CANCEL, 0);

		u = join_urls(down->url, ce->redirect);
		if (!u) break;

		if (!get_opt_int("protocol.http.bugs.broken_302_redirect")
		    && !ce->redirect_get) {
			unsigned char *p = strchr(down->url, POST_CHAR);

			if (p) add_to_strn(&u, p);
		}

		mem_free(down->url);

		down->url = u;
		down->stat.state = S_WAIT_REDIR;

		if (down->win) {
			struct event ev = { EV_REDRAW, 0, 0, 0 };

			ev.x = down->win->term->x;
			ev.y = down->win->term->y;
			down->win->handler(down->win, &ev, 0);
		}

		load_url(down->url, ce->url, &down->stat, PRI_DOWNLOAD,
			 NC_CACHE, status->prg ? status->prg->start : 0);

		return;
	}

	if (down->stat.prg && down->stat.prg->seek) {
		down->last_pos = down->stat.prg->seek;
		down->stat.prg->seek = 0;
		/* This is exclusive with the prealloc, thus we can perform
		 * this in front of that thing safely. */
		if (lseek(down->handle, down->last_pos, SEEK_SET) < 0)
			goto write_error;
	}

	foreach (frag, ce->frag) {
		/* TODO: Separate function? --pasky */
		int remain = down->last_pos - frag->offset;

		if (remain >= 0 && frag->length > remain) {
			int w;

#ifdef USE_OPEN_PREALLOC
			if (!down->last_pos && (!down->stat.prg
						|| down->stat.prg->size > 0)) {
				close(down->handle);
				down->handle = open_prealloc(down->file, O_CREAT|O_WRONLY|O_TRUNC, 0666,
							     down->stat.prg ? down->stat.prg->size : ce->length);
				if (down->handle == -1)
					goto write_error;
				set_bin(down->handle);
			}
#endif

			w = write(down->handle, frag->data + remain,
				  frag->length - remain);
			if (w == -1) {
				int saved_errno;

write_error:
				saved_errno = errno; /* Saved in case of ... --Zas */

				detach_connection(status, down->last_pos);
				if (!list_empty(sessions)) {
					unsigned char *msg = stracpy(down->file);
					unsigned char *emsg = stracpy((unsigned char *) strerror(saved_errno));

					if (msg && emsg) {
						msg_box(get_download_ses(down)->term, getml(msg, emsg, NULL),
							N_("Download error"), AL_CENTER | AL_EXTD_TEXT,
							N_("Could not create file"),
							" ", msg, ": ", emsg, NULL,
							NULL, 1,
							N_("Cancel"), NULL, B_ENTER | B_ESC);
					} else {
						if (msg) mem_free(msg);
						if (emsg) mem_free(emsg);
					}
				}

				abort_download(down, 0);
				return;
			}
			down->last_pos += w;
		}
	}

	detach_connection(status, down->last_pos);

end_store:
	if (status->state < 0) {
		if (status->state != S_OK) {
			unsigned char *t = get_err_msg(status->state);

			if (t) {
				unsigned char *tt = stracpy(down->url);

				if (tt) {
					unsigned char *p = strchr(tt, POST_CHAR);
					if (p) *p = '\0';

					msg_box(get_download_ses(down)->term, getml(tt, NULL),
						N_("Download error"), AL_CENTER | AL_EXTD_TEXT,
						N_("Error downloading"), " ",
						tt, ":\n\n", t, NULL,
						get_download_ses(down), 1,
						N_("Cancel"), NULL, B_ENTER | B_ESC /*,
						N_(T_RETRY), NULL, 0 */ /* FIXME: retry */);
				}
			}

		} else {
			if (down->prog) {
				prealloc_truncate(down->handle,
						  down->last_pos);
				close(down->handle);
				down->handle = -1;
				exec_on_terminal(get_download_ses(down)->term,
						 down->prog, down->file,
						 !!down->prog_flags);
				mem_free(down->prog);
				down->prog = NULL;

			} else {
				if (down->notify) {
					unsigned char *url = stracpy(down->url);

					msg_box(get_download_ses(down)->term, getml(url, NULL),
						N_("Download"), AL_CENTER | AL_EXTD_TEXT,
						N_("Download complete"), ":\n", url, NULL,
						get_download_ses(down), 1,
						N_("OK"), NULL, B_ENTER | B_ESC);
				}

				if (get_opt_int("document.download.notify_bell") + down->notify >= 2) {
					beep_terminal(get_download_ses(down)->term);
				}

				if (down->remotetime && get_opt_int("document.download.set_original_time")) {
					struct utimbuf foo;

					foo.actime = foo.modtime = down->remotetime;
					utime(down->file, &foo);
				}
			}
		}

		abort_download(down, 0);
		return;
	}

	if (down->win) {
		struct event ev = { EV_REDRAW, 0, 0, 0 };

		ev.x = down->win->term->x;
		ev.y = down->win->term->y;
		down->win->handler(down->win, &ev, 0);
	}
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
	/* !overwrite means always silently overwrite, which may be admitelly
	 * indeed a little confusing ;-) */
	int overwrite = get_opt_int("document.download.overwrite");
	unsigned char *ofilex = expand_tilde(ofile);
	unsigned char *file;

	/* TODO: If the file already exists, possibly:
	 * * inform the user
	 * * allow to resume the download
	 * * allow to specify a new name
	 * * allow to rename the old file
	 * --pasky */

	if (!overwrite || resume) {
		/* Nothing special to do... */
		callback(term, ofilex, data, resume);
		return;
	}

	/* If ofilex is unique -> file == ofilex */
	file = get_unique_name(ofilex);

	if (!file || overwrite == 1 || !strcmp(ofilex, file)) {
		/* Still nothing special to do... */
		if (file != ofilex) mem_free(ofilex);
		callback(term, file, data, 0);
		return;
	}

	/* overwrite == 2 (ask) and file != ofilex (=> original file already
	 * exists) */

	lun_hop = mem_calloc(1, sizeof(struct lun_hop));
	if (!lun_hop) {
		if (file != ofilex) mem_free(file);
		mem_free(ofilex);
		callback(term, NULL, data, 0);
		return;
	}
	lun_hop->term = term;
	lun_hop->ofile = ofilex;
	lun_hop->file = (file != ofilex) ? file : stracpy(ofilex);
	lun_hop->callback = callback;
	lun_hop->data = data;

	msg_box(term, NULL,
		N_("File exists"), AL_CENTER | AL_EXTD_TEXT,
		N_("This file already exists:\n"),
		lun_hop->ofile ? lun_hop->ofile : (unsigned char *) "", "\n\n",
		N_("The alternative filename is:\n"), file ? file : (unsigned char *) "", NULL,
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
	unsigned char *download_dir = get_opt_str("document.download.directory");
	unsigned char *wd;
	int h = -1;
	int i;
	int saved_errno;
#ifdef NO_FILE_SECURITY
	int sf = 0;
#else
	int sf = cdf_hop->safe;
#endif

	if (!file) goto finish;

	wd = get_cwd();
	set_cwd(term->cwd);

	h = open(file, O_CREAT | O_WRONLY | (sf && !resume ? O_EXCL : 0),
		 sf ? 0600 : 0666);
	saved_errno = errno; /* Saved in case of ... --Zas */

	if (wd) {
		set_cwd(wd);
		mem_free(wd);
	}

	if (h == -1) {
		unsigned char *msg = stracpy(file);
		unsigned char *msge = stracpy((unsigned char *) strerror(saved_errno));

		if (msg && msge) {
			msg_box(term, getml(msg, msge, NULL),
				N_("Download error"), AL_CENTER | AL_EXTD_TEXT,
				N_("Could not create file"), " ", msg, ": ",
				msge, NULL,
				NULL, 1,
				N_("Cancel"), NULL, B_ENTER | B_ESC);
		} else {
			if (msg) mem_free(msg);
			if (msge) mem_free(msge);
		}

	} else {
		set_bin(h);

		if (!cdf_hop->safe) {
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
	int l, nl;
	unsigned char *name;
	unsigned char *fn, *fnn, *fnnn, *s;
	unsigned char *nm = tempnam(NULL, "elinks");

	if (!nm) return NULL;

	name = init_str();
	if (!name) {
		mem_free(nm);
		return NULL;
	}
	nl = 0;
	add_to_str(&name, &nl, nm);
	free(nm);

	get_filename_from_url(url, &fn, &l);

	fnnn = NULL;
	for (fnn = fn; fnn < fn + l; fnn++)
		if (*fnn == '.')
			fnnn = fnn;

	if (fnnn) {
		s = memacpy(fnnn, l - (fnnn - fn));
		if (s) {
			check_shell_security(&s);
			add_to_str(&name, &nl, s);
			mem_free(s);
		}
	}

	return name;
}


unsigned char *
subst_file(unsigned char *prog, unsigned char *file)
{
	unsigned char *n = init_str();
	int l = 0;

	if (!n) return NULL;

	while (*prog) {
		int p;

		for (p = 0; prog[p] && prog[p] != '%'; p++);

		add_bytes_to_str(&n, &l, prog, p);
		prog += p;

		if (*prog == '%') {
#if defined(HAVE_CYGWIN_CONV_TO_FULL_WIN32_PATH)
#ifdef MAX_PATH
			unsigned char new_path[MAX_PATH];
#else
			unsigned char new_path[1024];
#endif

			cygwin_conv_to_full_win32_path(file, new_path);
			add_to_str(&n, &l, new_path);
#else
			add_to_str(&n, &l, file);
#endif
			prog++;
		}
	}

	return n;
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

	create_download_file(ses->term, file, &cmdw_hop->real_file, 0, resume,
			common_download_do, cmdw_hop);
}

static void
common_download_do(struct terminal *term, int fd, void *data, int resume)
{
	struct cmdw_hop *cmdw_hop = data;
	struct download *down = NULL;
	unsigned char *url = cmdw_hop->ses->dn_url;
	struct stat buf;

	if (!cmdw_hop->real_file) goto error;

	down = mem_calloc(1, sizeof(struct download));
	if (!down) goto error;

	down->url = stracpy(url);
	if (!down->url) goto error;

	down->file = cmdw_hop->real_file;

	if (fstat(fd, &buf)) goto error;
	down->last_pos = resume ? (int) buf.st_size : 0;

	down->stat.end = (void (*)(struct status *, void *)) download_data;
	down->stat.data = down;
	down->handle = fd;
	down->ses = cmdw_hop->ses;
	down->remotetime = 0;

	add_to_list(downloads, down);
	load_url(url, cmdw_hop->ses->ref_url, &down->stat, PRI_DOWNLOAD, NC_CACHE,
		 (resume ? down->last_pos : 0));
	display_download(cmdw_hop->ses->term, down, cmdw_hop->ses);

	mem_free(cmdw_hop);
	return;

error:
	if (down) {
		if (down->url) mem_free(down->url);
		mem_free(down);
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

	create_download_file(ses->term, file, &codw_hop->real_file,
			!!ses->tq_prog, 0, continue_download_do, codw_hop);
}

static void
continue_download_do(struct terminal *term, int fd, void *data, int resume)
{
	struct codw_hop *codw_hop = data;
	struct download *down = NULL;
	unsigned char *url = codw_hop->ses->tq_url;

	if (!codw_hop->real_file) goto cancel;

	down = mem_calloc(1, sizeof(struct download));
	if (!down) goto cancel;

	down->url = stracpy(url);
	if (!down->url) goto cancel;

	down->file = codw_hop->real_file;

	down->stat.end = (void (*)(struct status *, void *)) download_data;
	down->stat.data = down;
	down->last_pos = 0;
	down->handle = fd;
	down->ses = codw_hop->ses;

	if (codw_hop->ses->tq_prog) {
		down->prog = subst_file(codw_hop->ses->tq_prog, codw_hop->file);
		mem_free(codw_hop->file);
		mem_free(codw_hop->ses->tq_prog);
		codw_hop->ses->tq_prog = NULL;
	}

	down->prog_flags = codw_hop->ses->tq_prog_flags;
	add_to_list(downloads, down);
	change_connection(&codw_hop->ses->tq, &down->stat, PRI_DOWNLOAD, 0);
	tp_free(codw_hop->ses);
	display_download(codw_hop->ses->term, down, codw_hop->ses);

	mem_free(codw_hop);
	return;

cancel:
	tp_cancel(codw_hop->ses);
	if (codw_hop->ses->tq_prog && codw_hop->file) mem_free(codw_hop->file);
	if (down) {
		if (down->url) mem_free(down->url);
		mem_free(down);
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


void
tp_save(struct session *ses)
{
	if (ses->tq_prog) {
		mem_free(ses->tq_prog);
		ses->tq_prog = NULL;
	}
	query_file(ses, ses->tq_url, continue_download, tp_cancel, 1);
}


void
tp_open(struct session *ses)
{
	continue_download(ses, "");
}


/* FIXME: We need to modify this function to take frame data instead, as we
 * want to use this function for frames as well (now, when frame has content
 * type text/plain, it is ignored and displayed as HTML). */
void
tp_display(struct session *ses)
{
	struct location *l;

	l = mem_alloc(sizeof(struct location) + strlen(ses->tq_url));
	if (!l) return;
	memset(l, 0, sizeof(struct location));

	init_list(l->frames);
	memcpy(&l->stat, &ses->tq, sizeof(struct status));

	init_vs(&l->vs, ses->tq_url);
	if (ses->tq_goto_position) {
		l->vs.goto_position = ses->tq_goto_position;
		ses->tq_goto_position = NULL;
	}

	add_to_history(ses, l);
	cur_loc(ses)->stat.end = (void (*)(struct status *, void *))
				 doc_end_load;
	cur_loc(ses)->stat.data = ses;

	if (ses->tq.state >= 0)
		change_connection(&ses->tq, &cur_loc(ses)->stat, PRI_MAIN, 0);
	else
		cur_loc(ses)->stat.state = ses->tq.state;

	cur_loc(ses)->vs.plain = 1;
	display_timer(ses);
	tp_free(ses);
}


static void
type_query(struct session *ses, struct cache_entry *ce, unsigned char *ct,
	   struct option *assoc, int mailcap)
{
	unsigned char *content_type;

	if (ses->tq_prog) {
		mem_free(ses->tq_prog);
		ses->tq_prog = NULL;
	}

	if (assoc) {
		ses->tq_prog = stracpy(get_opt_str_tree(assoc, "program"));
		ses->tq_prog_flags = get_opt_bool_tree(assoc, "block");
		if (!get_opt_bool_tree(assoc, "ask")) {
			tp_open(ses);
			return;
		}
	}

	content_type = stracpy(ct);
	if (!content_type) return;

	if (!assoc) {
		if (!get_opt_int_tree(&cmdline_options, "anonymous")) {
			msg_box(ses->term, getml(content_type, NULL),
				N_("Unknown type"), AL_CENTER | AL_EXTD_TEXT,
				N_("Content type is"), " ", content_type, ".\n",
				N_("Do you want to save or display this file?"), NULL,
				ses, 3,
				N_("Save"), tp_save, B_ENTER,
				N_("Display"), tp_display, 0,
				N_("Cancel"), tp_cancel, B_ESC);
		} else {
			msg_box(ses->term, getml(content_type, NULL),
				N_("Unknown type"), AL_CENTER | AL_EXTD_TEXT,
				N_("Content type is"), " ", content_type, ".\n",
				N_("Do you want to display this file?"), NULL,
				ses, 2,
				N_("Display"), tp_display, B_ENTER,
				N_("Cancel"), tp_cancel, B_ESC);
		}
	} else {
		unsigned char *name = NULL;

		if (mailcap) {
			name = stracpy(assoc->name);
		} else {
			/* XXX: This should be maybe handled generically by
			 * some function in protocol/mime.c. --pasky */
			unsigned char *mt = get_mime_type_name(ct);
			struct option *opt;

			if (mt) {
				opt = get_opt_rec_real(&root_options, mt);
				mem_free(mt);
				if (opt) name = stracpy(opt->ptr);
			}
		}
		if (!name) name = stracpy(""); /* FIXME: unchecked return value */
		if (!name) {
			mem_free(content_type);
			return;
		}

		if (!get_opt_int_tree(&cmdline_options, "anonymous")) {
			msg_box(ses->term, getml(content_type, name, NULL),
				N_("What to do?"), AL_CENTER | AL_EXTD_TEXT,
				N_("Content type is"), " ", content_type, ".\n",
				N_("Do you want to open file with"),
				" ", name, ", ", N_("save it or display it?"), NULL,
				ses, 4,
				N_("Open"), tp_open, B_ENTER,
				N_("Save"), tp_save, 0,
				N_("Display"), tp_display, 0,
				N_("Cancel"), tp_cancel, B_ESC);
		} else {
			msg_box(ses->term, getml(content_type, name, NULL),
				N_("What to do?"), AL_CENTER | AL_EXTD_TEXT,
				N_("Content type is"), " ", content_type, ".\n",
				N_("Do you want to open file with"),
				" ", name, ", ", N_("or display it?"), NULL,
				ses, 3,
				N_("Open"), tp_open, B_ENTER,
				N_("Display"), tp_display, 0,
				N_("Cancel"), tp_cancel, B_ESC);
		}
	}
}


int
ses_chktype(struct session *ses, struct status **status, struct cache_entry *ce)
{
	struct option *assoc;
#ifdef MAILCAP
	/* Used to see if association came from mailcap or not */
	struct option *mailcap = NULL;
#endif
	int r = 0;
	unsigned char *ct;

	ct = get_content_type(ce->head, ce->url);
	if (!ct) goto end;

	if (!strcasecmp(ct, "text/html")) goto free_ct;
	/* RFC 3236 */
	if (!strcasecmp(ct, "application/xhtml+xml")) goto free_ct;

	r = 1;
	if (!strcasecmp(ct, "text/plain")) goto free_ct;

	assoc = get_mime_type_handler(ses->term, ct);

#ifdef MAILCAP
	if (!assoc) {
		/*
		 * XXX: Mailcap handling goes here since it mimics the
		 * option system based mime handling. This requires that
		 * a new option is allocated and we want to control how
		 * it should be freed before returning.
		 */
		mailcap = mailcap_lookup(ct, NULL);
		assoc = mailcap;
	}
#endif

	if (!assoc && strlen(ct) >= 4 && !strncasecmp(ct, "text", 4)) goto free_ct;

	if (ses->tq_url)
		internal("Type query to %s already in progress.", ses->tq_url);

	ses->tq_url = stracpy(ses->loading_url);
	*status = &ses->tq;
	change_connection(&ses->loading, *status, PRI_MAIN, 0);

	ses->tq_ce = ce;
	ses->tq_ce->refcount++;

	if (ses->tq_goto_position) mem_free(ses->tq_goto_position);

	ses->tq_goto_position = ses->goto_position ? stracpy(ses->goto_position) : NULL;
#ifdef MAILCAP
	type_query(ses, ce, ct, assoc, !!mailcap);
#else
	type_query(ses, ce, ct, assoc, 0);
#endif
	mem_free(ct);
#ifdef MAILCAP
	if (mailcap) delete_option(mailcap);
#endif

	return 1;

free_ct:
	mem_free(ct);

end:
#ifdef MAILCAP
	if (mailcap) delete_option(mailcap);
#endif
	if (ses->wtd_target && r) *ses->wtd_target = 0;
	ses_forward(ses);
	cur_loc(ses)->vs.plain = r;
	return 0;
}
