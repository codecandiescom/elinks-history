/* Support for dumping to the file on startup (w/o bfu) */
/* $Id: dump.c,v 1.6 2002/04/16 18:34:57 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LUA
#include <lua.h>
#include <lualib.h>
#endif
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <links.h>

#include <main.h>
#include <config/default.h>
#include <document/cache.h>
#include <document/dump.h>
#include <document/options.h>
#include <document/view.h>
#include <document/vs.h>
#include <document/html/renderer.h>
#include <intl/language.h>
#include <lowlevel/sched.h>
#include <lowlevel/select.h>
#include <lowlevel/terminal.h>
#include <osdep/os_dep.h>
#include <protocol/url.h>


struct status dump_stat;
int dump_pos;
int dump_redir_count = 0;

void dump_end(struct status *stat, void *p)
{
	struct cache_entry *ce = stat->ce;
	int oh = get_output_handle();

	if (oh == -1) return;
	if (ce && ce->redirect && dump_redir_count++ < MAX_REDIRECTS) {
		unsigned char *u, *p;
		
		if (stat->state >= 0)
			change_connection(stat, NULL, PRI_CANCEL);
		
		u = join_urls(ce->url, ce->redirect);
		
		if (!http_bugs.bug_302_redirect && !ce->redirect_get
		    && (p = strchr(ce->url, POST_CHAR)))
			add_to_strn(&u, p);
		
		load_url(u, ce->url, stat, PRI_MAIN, 0);
		mem_free(u);
		return;
	}
	
	if (stat->state >= 0 && stat->state < S_TRANS) return;
	if (stat->state >= S_TRANS && dmp != D_SOURCE) return;
	
	if (dmp == D_SOURCE) {
		if (ce) {
			struct fragment *frag;

			foreach(frag, ce->frag) if (frag->offset <= dump_pos && frag->offset + frag->length > dump_pos) {
				int l = frag->length - (dump_pos - frag->offset);
				int w = hard_write(oh, frag->data + dump_pos - frag->offset, l);
				
				if (w != l) {
					detach_connection(stat, dump_pos);
					
					if (w < 0) fprintf(stderr, "Error writing to stdout: %s.\n", strerror(errno));
					else fprintf(stderr, "Can't write to stdout.\n");
					
					retval = RET_ERROR;
					goto terminate;
				}
				
				dump_pos += w;
				detach_connection(stat, dump_pos);
			}
		}
		
		if (stat->state >= 0) return;
		
	} else if (ce) {
		struct document_options o;
		struct view_state *vs;
		struct f_data_c fd;

		if (!(vs = mem_alloc(sizeof(struct view_state) + strlen(stat->ce->url) + 1)))
			goto terminate;
		
		memset(&o, 0, sizeof(struct document_options));
		memset(vs, 0, sizeof(struct view_state));
		memset(&fd, 0, sizeof(struct f_data_c));
		
		o.xp = 0;
		o.yp = 1;
		o.xw = dump_width;
		o.yw = 25;
		o.col = 0;
		o.cp = 0;
		ds2do(&dds, &o);
		o.plain = 0;
		o.frames = 0;
		memcpy(&o.default_fg, &default_fg, sizeof(struct rgb));
		memcpy(&o.default_bg, &default_bg, sizeof(struct rgb));
		memcpy(&o.default_link, &default_link, sizeof(struct rgb));
		memcpy(&o.default_vlink, &default_vlink, sizeof(struct rgb));
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
		
		fprintf(stderr, "%s\n", get_english_translation(m));
		retval = RET_ERROR;
		goto terminate;
	}
	
terminate:
	terminate = 1;
}

void dump_start(unsigned char *u)
{
	unsigned char *uu, *wd;
	
	if (!*u) {
		fprintf(stderr, "URL expected after %s\n.", dmp == D_DUMP ? "-dump" : "-source");
ttt:
		terminate = 1;
		retval = RET_SYNTAX;
		return;
	}
	dump_stat.end = dump_end;
	dump_pos = 0;
	if (!(uu = translate_url(u, wd = get_cwd()))) uu = stracpy(u);
	if (load_url(uu, NULL, &dump_stat, PRI_MAIN, 0)) goto ttt;
	mem_free(uu);
	if (wd) mem_free(wd);
}

int dump_to_file(struct f_data *fd, int h)
{
#define D_BUF	65536

	int x, y;
	unsigned char *buf;
	int bptr = 0;

	if (!(buf = mem_alloc(D_BUF))) return -1;
	for (y = 0; y < fd->y; y++) for (x = 0; x <= fd->data[y].l; x++) {
		int c;

		if (x == fd->data[y].l) c = '\n';
		else {
			if (((c = fd->data[y].d[x]) & 0xff) == 1) c += ' ' - 1;
			if ((c >> 15) && (c & 0xff) >= 176 && (c & 0xff) < 224) c = frame_dumb[(c & 0xff) - 176];
		}
		buf[bptr++] = c;
		if (bptr >= D_BUF) {
			if (hard_write(h, buf, bptr) != bptr) goto fail;
			bptr = 0;
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
