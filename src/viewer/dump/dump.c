/* Support for dumping to the file on startup (w/o bfu) */
/* $Id: dump.c,v 1.138 2004/07/03 16:28:02 jonas Exp $ */

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
#include "cache/cache.h"
#include "document/document.h"
#include "document/options.h"
#include "document/renderer.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/error.h"
#include "sched/connection.h"
#include "terminal/color.h"
#include "terminal/hardio.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/dump/dump.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


static int dump_pos;
static struct download dump_download;
static int dump_redir_count = 0;


/* This dumps the given @cached's source onto @fd nothing more. It returns 0 if it
 * all went fine and 1 if something isn't quite right and we should terminate
 * ourselves ASAP. */
static int
dump_source(int fd, struct download *status, struct cache_entry *cached)
{
	struct fragment *frag;

	if (!cached) return 0;

nextfrag:
	foreach (frag, cached->frag) {
		int d = dump_pos - frag->offset;
		int l, w;

		if (d < 0 || frag->length <= d)
			continue;

		l = frag->length - d;
		w = hard_write(fd, frag->data + d, l);

		if (w != l) {
			detach_connection(status, dump_pos);

			if (w < 0)
				ERROR(G_("Can't write to stdout: %s"),
				      (unsigned char *) strerror(errno));
			else
				ERROR(G_("Can't write to stdout."));

			retval = RET_ERROR;
			return 1;
		}

		dump_pos += w;
		detach_connection(status, dump_pos);
		goto nextfrag;
	}

	return 0;
}

/* This dumps the given @cached's formatted output onto @fd. */
static void
dump_formatted(int fd, struct download *status, struct cache_entry *cached)
{
	struct document_options o;
	struct document_view formatted;
	struct view_state vs;
	int width;

	if (!cached) return;

	memset(&vs, 0, sizeof(struct view_state));
	memset(&formatted, 0, sizeof(struct document_view));

	get_opt_bool("document.browse.links.numbering") =
		!get_cmd_opt_bool("no-numbering");

	init_document_options(&o);
	width = get_opt_int("document.dump.width");
	set_box(&o.box, 0, 1, width, DEFAULT_TERMINAL_HEIGHT);

	o.cp = get_opt_int("document.dump.codepage");
	o.color_mode = COLOR_MODE_DUMP;
	o.plain = 0;
	o.frames = 0;

	init_vs(&vs, cached->uri, -1);

	render_document(&vs, &formatted, &o);
	dump_to_file(formatted.document, fd);

	detach_formatted(&formatted);
	destroy_vs(&vs);
}

static unsigned char *
subst_url(unsigned char *str, struct string *url)
{
	struct string string;

	if (!init_string(&string)) return NULL;

	while (*str) {
		int p;

		for (p = 0; str[p] && str[p] != '%' && str[p] != '\\'; p++);

		add_bytes_to_string(&string, str, p);
		str += p;

		if (*str == '\\') {
			unsigned char ch;

			str++;
			switch (*str) {
				case 'f':
					ch = '\f';
					break;
				case 'n':
					ch = '\n';
					break;
				case 't':
					ch = '\t';
					break;
				default:
					ch = *str;
			}
			if (*str) {
				add_char_to_string(&string, ch);
				str++;
			}
			continue;

		} else if (*str != '%') {
			break;
		}

		str++;
		switch (*str) {
			case 'u':
				if (url) add_string_to_string(&string, url);
				break;
		}

		if (*str) str++;
	}

	return string.source;
}

static void
dump_print(unsigned char *option, struct string *url)
{
	unsigned char *str = get_opt_str(option);

	if (str) {
		unsigned char *realstr = subst_url(str, url);

		if (realstr) {
			printf("%s", realstr);
			fflush(stdout);
			mem_free(realstr);
		}
	}
}

static void
dump_end(struct download *status, void *p)
{
	struct cache_entry *cached = status->cached;
	int fd = get_output_handle();

	if (fd == -1) return;
	if (cached && cached->redirect && dump_redir_count++ < MAX_REDIRECTS) {
		struct uri *uri = cached->redirect;

		if (is_in_progress_state(status->state))
			change_connection(status, NULL, PRI_CANCEL, 0);

		load_uri(uri, get_cache_uri(cached), status, PRI_MAIN, 0, -1);
		return;
	}

	if (is_in_queued_state(status->state)) return;

	if (get_cmd_opt_int("dump")) {
		if (is_in_transfering_state(status->state))
			return;

		dump_formatted(fd, status, cached);

	} else {
		if (dump_source(fd, status, cached) > 0)
			goto terminate;

		if (is_in_progress_state(status->state))
			return;

	}

	if (status->state != S_OK) {
		usrerror(get_err_msg(status->state, NULL));
		retval = RET_ERROR;
		goto terminate;
	}

terminate:
	terminate = 1;
	dump_next(NULL);
}

static void
dump_start(unsigned char *url)
{
	unsigned char *wd = get_cwd();
	struct uri *uri = get_translated_uri(url, wd);

	mem_free_if(wd);
	if (!*url) {
		usrerror(G_("URL expected after %s."),
			get_cmd_opt_int("source")
			? "-source" : "-dump");
		goto terminate;

	} else if (!uri || get_protocol_external_handler(uri->protocol)) {
		usrerror(G_("URL protocol not supported (%s)."), url);
		goto terminate;
	}

	dump_download.end = dump_end;
	dump_pos = 0;

	if (load_uri(uri, NULL, &dump_download, PRI_MAIN, 0, -1)) {
terminate:
		dump_next(NULL);
		terminate = 1;
		retval = RET_SYNTAX;
	}

	if (uri) done_uri(uri);
}

void
dump_next(struct list_head *url_list)
{
	static INIT_LIST_HEAD(todo_list);
	static INIT_LIST_HEAD(done_list);
	struct string_list_item *item;

	if (url_list) {
		/* Steal all them nice list items but keep the same order */
		while (!list_empty(*url_list)) {
			item = url_list->next;
			del_from_list(item);
			add_to_list_end(todo_list, item);
		}
	}

	/* Dump each url list item one at a time */
	if (!list_empty(todo_list)) {
		static int first = 1;

		terminate = 0;

		item = todo_list.next;
		del_from_list(item);
		add_to_list(done_list, item);

		if (!first) {
			dump_print("document.dump.separator", NULL);
		} else {
			first = 0;
		}

		dump_print("document.dump.header", &item->string);
		dump_start(item->string.source);
		/* XXX: I think it ought to print footer at the end of
		 * the whole dump (not only this file). Testing required.
		 * --pasky */
		dump_print("document.dump.footer", &item->string);

	} else {
		free_string_list(&done_list);
		terminate = 1;
	}
}

/* Using this function in dump_to_file() is unfortunately slightly slower than
 * the current code.  However having this here instead of in the scripting
 * backends is better. */
struct string *
add_document_to_string(struct string *string, struct document *document)
{
	int y;

	assert(string && document);
	if_assert_failed return NULL;

	for (y = 0; y < document->height; y++) {
		struct screen_char *pos = document->data[y].chars;
		int x;

		for (x = 0; x < document->data[y].length; x++) {
			unsigned char data = pos->data;
			unsigned int frame = (pos->attr & SCREEN_ATTR_FRAME);

			if (data < ' ' || data == ASCII_DEL) {
				data = ' ';
			} else if (frame && data >= 176 && data < 224) {
				data = frame_dumb[data - 176];
			}

			add_char_to_string(string, data);
		}

		add_char_to_string(string, '\n');
	}

	return string;
}

int
dump_to_file(struct document *document, int fd)
{
#define D_BUF	65536

	int y;
	int bptr = 0;
	unsigned char *buf = mem_alloc(D_BUF);

	if (!buf) return -1;

	for (y = 0; y < document->height; y++) {
		int x;

		for (x = 0; x <= document->data[y].length; x++) {
			unsigned char c;

			if (x == document->data[y].length) {
				c = '\n';
			} else {
				unsigned char attr = document->data[y].chars[x].attr;

				c = document->data[y].chars[x].data;

				if ((attr & SCREEN_ATTR_FRAME)
				    && c >= 176 && c < 224)
					c = frame_dumb[c - 176];
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

	if (get_opt_bool("document.browse.links.numbering")
	    && document->nlinks) {
		int x;
		unsigned char *header = "\nReferences\n\n   Visible links\n";
		int headlen = strlen(header);

		if (hard_write(fd, header, headlen) != headlen)
			goto fail;

		for (x = 0; x < document->nlinks; x++) {
			struct link *link = &document->links[x];
			unsigned char *where = link->where;

			if (!where) continue;

			if (link->title && *link->title)
				snprintf(buf, D_BUF, "%4d. %s\n\t%s\n",
					 x + 1, link->title, where);
			else
				snprintf(buf, D_BUF, "%4d. %s\n",
					 x + 1, where);
			bptr = strlen(buf);
			if (hard_write(fd, buf, bptr) != bptr)
				goto fail;
		}
	}

	mem_free(buf);
	return 0;

#undef D_BUF
}
