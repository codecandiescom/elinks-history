/* Support for dumping to the file on startup (w/o bfu) */
/* $Id: dump.c,v 1.4 2003/01/03 01:02:16 pasky Exp $ */

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
#include "document/options.h"
#include "document/html/renderer.h"
#include "intl/language.h"
#include "lowlevel/select.h"
#include "lowlevel/terminal.h"
#include "osdep/os_dep.h"
#include "protocol/url.h"
#include "sched/sched.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/dump/dump.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

int dump_pos;

static struct status dump_stat;
static int dump_redir_count = 0;

void
dump_end(struct status *stat, void *p)
{
	struct cache_entry *ce = stat->ce;
	int oh = get_output_handle();

	if (oh == -1) return;
	if (ce && ce->redirect && dump_redir_count++ < MAX_REDIRECTS) {
		unsigned char *u, *p;

		if (stat->state >= 0)
			change_connection(stat, NULL, PRI_CANCEL, 0);

		u = join_urls(ce->url, ce->redirect);
		if (!u) return;

		if (!ce->redirect_get
		    && !get_opt_int("protocol.http.bugs.broken_302_redirect")) {
			p = strchr(ce->url, POST_CHAR);
			if (p) add_to_strn(&u, p);
		}

		load_url(u, ce->url, stat, PRI_MAIN, 0, -1);
		mem_free(u);
		return;
	}

	if (stat->state >= 0 && stat->state < S_TRANS) return;
	if (stat->state >= S_TRANS
	    && get_opt_int_tree(&cmdline_options, "dump"))
		return;

	if (get_opt_int_tree(&cmdline_options, "source")) {
		if (ce) {
			struct fragment *frag;

nextfrag:
			foreach(frag, ce->frag) {
				int d = dump_pos - frag->offset;

				if (d >= 0 && frag->length > d) {
					int l = frag->length - d;
					int w = hard_write(oh, frag->data + d, l);

					if (w != l) {
						detach_connection(stat, dump_pos);

						if (w < 0)
							fprintf(stderr,
								"Error writing to stdout: %s.\n",
								strerror(errno));
						else
							fprintf(stderr,
								"Can't write to stdout.\n");

						retval = RET_ERROR;
						goto terminate;
					}

					dump_pos += w;
					detach_connection(stat, dump_pos);
					goto nextfrag;
				}
			}
		}

		if (stat->state >= 0) return;

	} else if (ce) {
		struct document_options o;
		struct f_data_c fd;
		struct view_state *vs = mem_alloc(sizeof(struct view_state)
						  + strlen(stat->ce->url) + 1);

		if (!vs) goto terminate;

		memset(&o, 0, sizeof(struct document_options));
		memset(vs, 0, sizeof(struct view_state));
		memset(&fd, 0, sizeof(struct f_data_c));

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

		init_vs(vs, stat->ce->url);
		cached_format_html(vs, &fd, &o);
		dump_to_file(fd.f_data, oh);
		detach_formatted(&fd);
		destroy_vs(vs);
		mem_free(vs);

	}

	if (stat->state != S_OK) {
		unsigned char *m = get_err_msg(stat->state);

		fprintf(stderr, "%s\n", N_(m)); /* TODO: -> gettext() */
		retval = RET_ERROR;
		goto terminate;
	}

terminate:
	terminate = 1;
}

void
dump_start(unsigned char *u)
{
	unsigned char *uu, *wd;

	if (!*u) {
		fprintf(stderr, "URL expected after %s.\n",
			get_opt_int_tree(&cmdline_options, "source")
			? "-source" : "-dump");
		goto terminate;
	}

	dump_stat.end = dump_end;
	dump_pos = 0;

	wd = get_cwd();
	uu = translate_url(u, wd);
	if (!uu) uu = stracpy(u);
	if (!uu) goto terminate;
	if (load_url(uu, NULL, &dump_stat, PRI_MAIN, 0, -1))
		goto terminate;

	mem_free(uu);
	if (wd) mem_free(wd);
	return;

terminate:
	terminate = 1;
	retval = RET_SYNTAX;
}

int
dump_to_file(struct f_data *fd, int h)
{
#define D_BUF	65536

	int x, y;
	int bptr = 0;
	unsigned char *buf = mem_alloc(D_BUF);

	if (!buf) return -1;

	for (y = 0; y < fd->y; y++) {
		for (x = 0; x <= fd->data[y].l; x++) {
			int c;

			if (x == fd->data[y].l) {
				c = '\n';
			} else {
				c = fd->data[y].d[x];
				if ((c & 0xff) == 1) c += ' ' - 1;
				if ((c >> 15) && (c & 0xff) >= 176
				    && (c & 0xff) < 224)
					c = frame_dumb[(c & 0xff) - 176];
			}
			buf[bptr++] = c;
			if (bptr >= D_BUF) {
				if (hard_write(h, buf, bptr) != bptr)
					goto fail;
				bptr = 0;
			}
		}
	}

	if (hard_write(h, buf, bptr) != bptr) {

fail:
		mem_free(buf);
		return -1;
	}

	mem_free(buf);
	return 0;
#undef D_BUF
}
