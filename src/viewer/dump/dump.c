/* Support for dumping to the file on startup (w/o bfu) */
/* $Id: dump.c,v 1.28 2003/07/07 22:27:17 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> /* NetBSD flavour */
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "main.h"
#include "config/options.h"
#include "document/cache.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "osdep/os_dep.h"
#include "protocol/url.h"
#include "sched/error.h"
#include "sched/connection.h"
#include "terminal/hardio.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/dump/dump.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


int dump_pos;

static struct download dump_download;
static int dump_redir_count = 0;


/* This dumps the given @ce's source onto @fd nothing more. It returns 0 if it
 * all went fine and 1 if something isn't quite right and we should terminate
 * ourselves ASAP. */
static int
dump_source(int fd, struct download *status, struct cache_entry *ce)
{
	struct fragment *frag;

	if (!ce) return 0;

nextfrag:
	foreach (frag, ce->frag) {
		int d = dump_pos - frag->offset;
		int l, w;

		if (d < 0 || frag->length <= d)
			continue;

		l = frag->length - d;
		w = hard_write(fd, frag->data + d, l);

		if (w != l) {
			detach_connection(status, dump_pos);

			if (w < 0)
				error(gettext("Can't write to stdout: %s"),
				      (unsigned char *) strerror(errno));
			else
				error(gettext("Can't write to stdout."));

			retval = RET_ERROR;
			return 1;
		}

		dump_pos += w;
		detach_connection(status, dump_pos);
		goto nextfrag;
	}

	return 0;
}

/* This dumps the given @ce's formatted output onto @fd. It returns 0 if it all
 * went fine and 1 if something isn't quite right and we should terminate
 * ourselves ASAP. */
static int
dump_formatted(int fd, struct download *status, struct cache_entry *ce)
{
	struct document_options o;
	struct f_data_c formatted;
	struct view_state *vs;

	if (!ce) return 0;

	vs = mem_alloc(sizeof(struct view_state) + strlen(ce->url) + 1);
	if (!vs) return 1;

	memset(&o, 0, sizeof(struct document_options));
	memset(vs, 0, sizeof(struct view_state));
	memset(&formatted, 0, sizeof(struct f_data_c));

	o.xp = 0;
	o.yp = 1;
	o.xw = get_opt_int("document.dump.width");
	o.yw = 25;
	o.col = 0;
	o.cp = get_opt_int("document.dump.codepage");
	mk_document_options(&o);
	o.plain = 0;
	o.frames = 0;
	memcpy(&o.default_fg, get_opt_ptr("document.colors.text"), sizeof(struct rgb));
	memcpy(&o.default_bg, get_opt_ptr("document.colors.background"), sizeof(struct rgb));
	memcpy(&o.default_link, get_opt_ptr("document.colors.link"), sizeof(struct rgb));
	memcpy(&o.default_vlink, get_opt_ptr("document.colors.vlink"), sizeof(struct rgb));
	o.framename = "";

	init_vs(vs, ce->url);
	cached_format_html(vs, &formatted, &o);
	dump_to_file(formatted.f_data, fd);
	detach_formatted(&formatted);
	destroy_vs(vs);
	mem_free(vs);

	return 0;
}

void
dump_end(struct download *status, void *p)
{
	struct cache_entry *ce = status->ce;
	int fd = get_output_handle();

	if (fd == -1) return;
	if (ce && ce->redirect && dump_redir_count++ < MAX_REDIRECTS) {
		unsigned char *u;

		if (status->state >= 0)
			change_connection(status, NULL, PRI_CANCEL, 0);

		u = join_urls(ce->url, ce->redirect);
		if (!u) return;

		if (!ce->redirect_get
		    && !get_opt_int("protocol.http.bugs.broken_302_redirect")) {
			unsigned char *pc = strchr(ce->url, POST_CHAR);

			if (pc) add_to_strn(&u, pc);
		}

		load_url(u, ce->url, status, PRI_MAIN, 0, -1);
		mem_free(u);
		return;
	}

	if (status->state >= 0 && status->state < S_TRANS) return;
	if (status->state >= S_TRANS
	    && get_opt_int_tree(&cmdline_options, "dump"))
		return;

	if (get_opt_int_tree(&cmdline_options, "source")) {
		if (dump_source(fd, status, ce) > 0)
			goto terminate;

		if (status->state >= 0)
			return;

	} else {
		if (dump_formatted(fd, status, ce) > 0)
			goto terminate;
	}

	if (status->state != S_OK) {
		error(get_err_msg(status->state, NULL));
		retval = RET_ERROR;
		goto terminate;
	}

terminate:
	terminate = 1;
}

void
dump_start(unsigned char *url)
{
	unsigned char *real_url = NULL;
	unsigned char *wd;

	if (!*url) {
		error(gettext("URL expected after %s."),
			get_opt_int_tree(&cmdline_options, "source")
			? "-source" : "-dump");
		goto terminate;
	}

	dump_download.end = dump_end;
	dump_pos = 0;

	wd = get_cwd();
	real_url = translate_url(url, wd);
	if (wd) mem_free(wd);

	if (!real_url) real_url = stracpy(url);
	if (!real_url
	    || load_url(real_url, NULL, &dump_download, PRI_MAIN, 0, -1)) {
terminate:
		terminate = 1;
		retval = RET_SYNTAX;
	}

	if (real_url) mem_free(real_url);
}


int
dump_to_file(struct f_data *formatted, int fd)
{
#define D_BUF	65536

	int x, y;
	int bptr = 0;
	unsigned char *buf = mem_alloc(D_BUF);

	if (!buf) return -1;

	for (y = 0; y < formatted->y; y++) {
		for (x = 0; x <= formatted->data[y].l; x++) {
			int c;

			if (x == formatted->data[y].l) {
				c = '\n';
			} else {
				c = formatted->data[y].d[x];
				if ((c & 0xff) == 1) c += ' ' - 1;
				if ((c >> 15) && (c & 0xff) >= 176
				    && (c & 0xff) < 224)
					c = frame_dumb[(c & 0xff) - 176];
			}
			buf[bptr++] = c;
			if (bptr >= D_BUF) {
				if (hard_write(fd, buf, bptr) != bptr)
					goto fail;
				bptr = 0;
			}
		}
	}

	if (hard_write(fd, buf, bptr) != bptr) {
fail:
		mem_free(buf);
		return -1;
	}

	mem_free(buf);
	return 0;

#undef D_BUF
}
