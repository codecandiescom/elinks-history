/* Internal "http" protocol implementation */
/* $Id: http.c,v 1.106 2003/05/06 16:47:44 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "elinks.h"

#include "dialogs/menu.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "document/cache.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/connect.h"
#include "lowlevel/sysname.h"
#include "terminal/terminal.h"
#include "protocol/http/auth.h"
#include "protocol/http/header.h"
#include "protocol/http/http.h"
#include "protocol/url.h"
#include "sched/sched.h"
#include "sched/session.h"
#include "util/base64.h"
#include "util/blacklist.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


struct http_connection_info {
	enum blacklist_flags bl_flags;
	int http10;
	int close;

#define LEN_CHUNKED -2 /* == we get data in unknown number of chunks */
#define LEN_FINISHED 0
	int length;

	int version;

	/* Either bytes coming in this chunk yet or "parser state". */
#define CHUNK_DATA_END	-3
#define CHUNK_ZERO_SIZE	-2
#define CHUNK_SIZE	-1
	int chunk_remaining;
};

static void uncompress_shutdown(struct connection *);


static unsigned char *
subst_user_agent(unsigned char *fmt, unsigned char *version,
		 unsigned char *sysname, unsigned char *termsize)
{
	unsigned char *n = init_str();
	int l = 0;

	if (!n) return NULL;

	while (*fmt) {
		int p;

		for (p = 0; fmt[p] && fmt[p] != '%'; p++);

		add_bytes_to_str(&n, &l, fmt, p);
		fmt += p;

		if (*fmt == '%') {
			fmt++;
			switch (*fmt) {
				case 'v':
					add_to_str(&n, &l, version);
					break;
				case 's':
					add_to_str(&n, &l, sysname);
					break;
				case 't':
					if (termsize)
						add_to_str(&n, &l, termsize);
					break;
				default:
					add_bytes_to_str(&n, &l, fmt - 1, 2);
					break;
			}
			if (*fmt) fmt++;
		}
	}

	return n;
}

static void
add_url_to_http_str(unsigned char **hdr, int *l, unsigned char *url_data,
		    unsigned char *post)
{
	/* This block substitues spaces in URL by %20s. This is
	 * certainly not the right place where to do it, but now the
	 * behaviour is at least improved compared to what we had
	 * before. We should probably encode all URLs as early as
	 * possible, and possibly decode them back in protocol
	 * backends. --pasky */

	/* Nop, this doesn't stand for EuroURL, but Encoded URL. */
	unsigned char *eurl;
	unsigned char *p, *p1;

	if (!post) {
		eurl = stracpy(url_data);
	} else {
		eurl = memacpy(url_data, post - url_data - 1);
	}

	if (!eurl) return;

	p = p1 = eurl;
	while (*(p += strcspn(p, " \t\r\n\\"))) {
		unsigned char ch = *p;

		*p = '\0';
		add_to_str(hdr, l, p1);
		if (ch == '\\')
			add_to_str(hdr, l, "/");
		else
			add_to_str(hdr, l, "%20");
		p++;
		p1 = p;
	}

	add_to_str(hdr, l, p1);
	mem_free(eurl);
}


static int
get_http_code(unsigned char *head, int *code, int *version)
{
	/* \s* */
	while (head[0] == ' ')
		head++;

	/* HTTP */
	if (upcase(head[0]) != 'H' || upcase(head[1]) != 'T' ||
	    upcase(head[2]) != 'T' || upcase(head[3]) != 'P')
		return -1;

	/* /\d\.\d\s */
	if (head[4] == '/' && head[5] >= '0' && head[5] <= '9' &&
	    head[6] == '.' && head[7] >= '0' && head[7] <= '9' &&
	    head[8] <= ' ') {
		*version = (head[5] - '0') * 10 + head[7] - '0';
	} else {
		*version = 0;
	}

	/* \s+ */
	for (head += 4; *head > ' '; head++);
	if (*head++ != ' ')
		return -1;

	/* \d\d\d */
	if (head[0] < '1' || head[0] > '9' ||
	    head[1] < '0' || head[1] > '9' ||
	    head[2] < '0' || head[2] > '9')
		return -1;
	*code = (head[0] - '0') * 100 + (head[1] - '0') * 10 + head[2] - '0';

	return 0;
}


static int
check_http_server_bugs(unsigned char *url,
		       struct http_connection_info *info,
		       unsigned char *head)
{
	unsigned char *server, **s;
	static unsigned char *buggy_servers[] = {
		"mod_czech/3.1.0",
		"Purveyor",
		"Netscape-Enterprise",
		NULL
	};

	if (!get_opt_int("protocol.http.bugs.allow_blacklist") || info->http10)
		return 0;

	server = parse_http_header(head, "Server", NULL);
	if (!server)
		return 0;

	for (s = buggy_servers; *s; s++)
		if (strstr(server, *s)) {
			mem_free(server);
			server = get_host_name(url);
			if (server) {
				add_blacklist_entry(server, BL_HTTP10);
				mem_free(server);
				return 1;
			}
			return 0;
		}

	mem_free(server);
	return 0;
}

static void
http_end_request(struct connection *c, int state)
{
	setcstate(c, state);
	uncompress_shutdown(c);

	if (c->state == S_OK) {
		if (c->cache) {
			truncate_entry(c->cache, c->from, 1);
			c->cache->incomplete = 0;
#ifdef HAVE_SCRIPTING
			c->cache->done_pre_format_html_hook = 0;
#endif
		}
	}

	if (c->info && !((struct http_connection_info *) c->info)->close
	    && (!c->ssl) /* We won't keep alive ssl connections */
	    && (!get_opt_int("protocol.http.bugs.post_no_keepalive")
	        || !strchr(c->url, POST_CHAR))) {
		add_keepalive_socket(c, HTTP_KEEPALIVE_TIMEOUT);
	} else {
		abort_connection(c);
	}
}

static void http_send_header(struct connection *);

void
http_func(struct connection *c)
{
	/* setcstate(c, S_CONN); */
	set_timeout(c);

	if (get_keepalive_socket(c)) {
		int p = get_port(c->url);

		if (p == -1) {
			abort_conn_with_state(c, S_INTERNAL);
			return;
		}

		make_connection(c, p, &c->sock1, http_send_header);
	} else {
		http_send_header(c);
	}
}

void
proxy_func(struct connection *c)
{
	http_func(c);
}

static void http_get_header(struct connection *);

#define IS_PROXY_URL(x) (upcase((x)[0]) == 'P')
#define GET_REAL_URL(x) (IS_PROXY_URL((x)) ? get_url_data((x)) : (x))

static void
http_send_header(struct connection *c)
{
	static unsigned char *accept_charset = NULL;
	unsigned char *host = GET_REAL_URL(c->url);
	struct http_connection_info *info;
	int http10 = get_opt_int("protocol.http.bugs.http10");
	int trace = get_opt_bool("protocol.http.trace");
	unsigned char *post;

	struct cache_entry *e = NULL;
	unsigned char *hdr;
	unsigned char *host_data, *url_data;
	int l = 0;
	unsigned char *optstr;

	set_timeout(c);

	info = mem_calloc(1, sizeof(struct http_connection_info));
	if (!info) {
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}
	c->info = info;

	host_data = get_host_name(host);
	if (host_data) {
		info->bl_flags = get_blacklist_flags(host_data);
		mem_free(host_data);
	}

	if (info->bl_flags & BL_HTTP10) {
		http10 = 1;
	}

	info->http10 = http10;

	post = strchr(c->url, POST_CHAR);
	if (post) post++;

	hdr = init_str();
	if (!hdr) {
		http_end_request(c, S_OUT_OF_MEM);
		return;
	}

	if (trace) {
		add_to_str(&hdr, &l, "TRACE ");
	} else if (!post) {
		add_to_str(&hdr, &l, "GET ");
	} else {
		add_to_str(&hdr, &l, "POST ");
		c->unrestartable = 2;
	}

	if (!IS_PROXY_URL(c->url)) {
		add_to_str(&hdr, &l, "/");
	}

	url_data = get_url_data(c->url);
	if (!url_data) {
		http_end_request(c, S_BAD_URL);
		return;
	}

	add_url_to_http_str(&hdr, &l, url_data, post);

	if (http10) {
		add_to_str(&hdr, &l, " HTTP/1.0\r\n");
	} else {
		add_to_str(&hdr, &l, " HTTP/1.1\r\n");
	}

	host_data = get_host_name(host);

	if (host_data) {
		add_to_str(&hdr, &l, "Host: ");
#ifdef IPV6
		if (strchr(host_data, ':') != strrchr(host_data, ':')) {
			/* IPv6 address */
			add_to_str(&hdr, &l, "[");
			add_to_str(&hdr, &l, host_data);
			add_to_str(&hdr, &l, "]");
		} else
#endif
			add_to_str(&hdr, &l, host_data);

		mem_free(host_data);

		host_data = get_port_str(host);
		if (host_data) {
			if (*host_data) {
				add_to_str(&hdr, &l, ":");
				add_to_str(&hdr, &l, host_data);
			}
			mem_free(host_data);
		}

		add_to_str(&hdr, &l, "\r\n");
	}

	optstr = get_opt_str("protocol.http.proxy.user");
	if (optstr[0]) {
		unsigned char *proxy_data;

		proxy_data = straconcat(optstr, ":",
					get_opt_str("protocol.http.proxy.passwd"),
					NULL);
		if (proxy_data) {
			unsigned char *proxy_64 = base64_encode(proxy_data);

			if (proxy_64) {
				add_to_str(&hdr, &l, "Proxy-Authorization: Basic ");
				add_to_str(&hdr, &l, proxy_64);
				add_to_str(&hdr, &l, "\r\n");
				mem_free(proxy_64);
			}
			mem_free(proxy_data);
		}
	}

	optstr = get_opt_str("protocol.http.user_agent");
	if (*optstr && strcmp(optstr, " ")) {
		unsigned char *ustr, ts[64] = "";

                add_to_str(&hdr, &l, "User-Agent: ");

		if (!list_empty(terminals)) {
			struct terminal *term = terminals.prev;

			snprintf(ts, 64, "%dx%d", term->x, term->y);
		}
		ustr = subst_user_agent(optstr, VERSION_STRING, system_name,
					ts);

		if (ustr) {
			add_to_str(&hdr, &l, ustr);
			mem_free(ustr);
		}

		add_to_str(&hdr, &l, "\r\n");
        }

	switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			break;

		case REFERER_FAKE:
			optstr = get_opt_str("protocol.http.referer.fake");
			if (optstr[0]) {
				add_to_str(&hdr, &l, "Referer: ");
				add_to_str(&hdr, &l, optstr);
				add_to_str(&hdr, &l, "\r\n");
			}
			break;

		case REFERER_TRUE:
			if (c->prev_url && c->prev_url[0]) {
				unsigned char *tmp_post = strchr(c->prev_url, POST_CHAR);

				if (tmp_post) tmp_post++;
				add_to_str(&hdr, &l, "Referer: ");
				add_url_to_http_str(&hdr, &l, c->prev_url, tmp_post);
				add_to_str(&hdr, &l, "\r\n");
			}
			break;

		case REFERER_SAME_URL:
			add_to_str(&hdr, &l, "Referer: ");

			add_to_str(&hdr, &l, "http://");
			host_data = get_host_name(extract_proxy(host));
			if (host_data) {
				add_to_str(&hdr, &l, host_data);
				mem_free(host_data);

				host_data = get_port_str(extract_proxy(host));
				if (host_data) {
					if (*host_data) {
						add_to_str(&hdr, &l, ":");
						add_to_str(&hdr, &l, host_data);
					}
					mem_free(host_data);
				}
			}

			if (!IS_PROXY_URL(c->url) || hdr[l - 1] != '/') {
				add_to_str(&hdr, &l, "/");
			}

			url_data = get_url_data(extract_proxy(c->url));
			if (url_data) {
				add_url_to_http_str(&hdr, &l, url_data, post);
			}

			add_to_str(&hdr, &l, "\r\n");
			break;
	}

	add_to_str(&hdr, &l, "Accept: */*\r\n");

	/* TODO: Make this encoding.c function. */
#if defined(HAVE_BZLIB_H) || defined(HAVE_ZLIB_H)
	add_to_str(&hdr, &l, "Accept-Encoding: ");

#ifdef HAVE_BZLIB_H
	add_to_str(&hdr, &l, "bzip2");
#endif

#ifdef HAVE_ZLIB_H
#ifdef HAVE_BZLIB_H
	add_to_str(&hdr, &l, ", ");
#endif
	add_to_str(&hdr, &l, "gzip");
#endif
	add_to_str(&hdr, &l, "\r\n");
#endif

	if (!accept_charset) {
		unsigned char *cs, *ac;
		int aclen = 0;
		int i;

		ac = init_str();
		if (ac) {
			for (i = 0; (cs = get_cp_mime_name(i)); i++) {
				if (aclen) {
					add_to_str(&ac, &aclen, ", ");
				} else {
					add_to_str(&ac, &aclen, "Accept-Charset: ");
				}
				add_to_str(&ac, &aclen, cs);
			}

			if (aclen) {
				add_to_str(&ac, &aclen, "\r\n");
			}

			/* Never freed until exit(), if you found a  better solution,
			 * let us now ;)
			 * Do not use mem_alloc() here. */
			accept_charset = malloc(strlen(ac) + 1);
			if (accept_charset) {
				strcpy(accept_charset, ac);
			} else {
				accept_charset = "";
			}

			mem_free(ac);
		}
	}

	if (!(info->bl_flags & BL_NO_CHARSET)
	    && !get_opt_int("protocol.http.bugs.accept_charset")) {
		add_to_str(&hdr, &l, accept_charset);
	}

	optstr = get_opt_str("protocol.http.accept_language");
	if (optstr[0]) {
		add_to_str(&hdr, &l, "Accept-Language: ");
		add_to_str(&hdr, &l, optstr);
		add_to_str(&hdr, &l, "\r\n");
	} else if (get_opt_bool("protocol.http.accept_ui_language")) {
/* FIXME */
#ifdef ENABLE_NLS
			unsigned char *code;

			code = language_to_iso639(current_language);
			add_to_str(&hdr, &l, "Accept-Language: ");
			add_to_str(&hdr, &l, code ? code : (unsigned char *) "");
			add_to_str(&hdr, &l, "\r\n");
#endif
	}

	if (!http10) {
		if (!IS_PROXY_URL(c->url)) {
			add_to_str(&hdr, &l, "Connection: ");
		} else {
			add_to_str(&hdr, &l, "Proxy-Connection: ");
		}

		if (!post || !get_opt_int("protocol.http.bugs.post_no_keepalive")) {
			add_to_str(&hdr, &l, "Keep-Alive\r\n");
		} else {
			add_to_str(&hdr, &l, "close\r\n");
		}
	}

	e = c->cache;
	if (e) {
		if (!e->incomplete && e->head && e->last_modified
		    && c->cache_mode <= NC_IF_MOD) {
			add_to_str(&hdr, &l, "If-Modified-Since: ");
			add_to_str(&hdr, &l, e->last_modified);
			add_to_str(&hdr, &l, "\r\n");
		}
	}

	if (c->cache_mode >= NC_PR_NO_CACHE) {
		add_to_str(&hdr, &l, "Pragma: no-cache\r\n");
		add_to_str(&hdr, &l, "Cache-Control: no-cache\r\n");
	}

	if (c->from || (c->prg.start > 0)) {
		/* c->from takes precedence. c->prg.start is set only the first
		 * time, then c->from gets updated and in case of any retries
		 * etc we have everything interesting in c->from already. */
		add_to_str(&hdr, &l, "Range: bytes=");
		add_num_to_str(&hdr, &l, c->from ? c->from : c->prg.start);
		add_to_str(&hdr, &l, "-\r\n");
	}

	host_data = find_auth(host);
	if (host_data) {
		add_to_str(&hdr, &l, "Authorization: Basic ");
		add_to_str(&hdr, &l, host_data);
		add_to_str(&hdr, &l, "\r\n");
		mem_free(host_data);
	}

	if (post) {
		unsigned char *pd = strchr(post, '\n');

		if (pd) {
			add_to_str(&hdr, &l, "Content-Type: ");
			add_bytes_to_str(&hdr, &l, post, pd - post);
			add_to_str(&hdr, &l, "\r\n");
			post = pd + 1;
		}
		add_to_str(&hdr, &l, "Content-Length: ");
		add_num_to_str(&hdr, &l, strlen(post) / 2);
		add_to_str(&hdr, &l, "\r\n");
	}

#ifdef COOKIES
	send_cookies(&hdr, &l, host);
#endif

	add_to_str(&hdr, &l, "\r\n");

	if (post) {
		while (post[0] && post[1]) {
			int h1, h2;

			h1 = post[0] <= '9' ? post[0] - '0'
					    : post[0] >= 'A' ? upcase(post[0])
					    		       - 'A' + 10
							     : 0;

			if (h1 < 0 || h1 >= 16) {
				h1 = 0;
			}

			h2 = post[1] <= '9' ? post[1] - '0'
					    : post[1] >= 'A' ? upcase(post[1])
					    		       - 'A' + 10
							     : 0;

			if (h2 < 0 || h2 >= 16) {
				h2 = 0;
			}

			add_chr_to_str(&hdr, &l, h1 * 16 + h2);
			post += 2;
		}
	}

	write_to_socket(c, c->sock1, hdr, l, http_get_header);
	mem_free(hdr);

	setcstate(c, S_SENT);
}


/** @func	uncompress_data(struct connection *conn, unsigned char *data,
		int len, int *dlen)
 * @brief	This function uncompress data blocks (if they were compressed).
 * @param	conn	standard structure
 * @param	data	block of data
 * @param	len	length of the block
 * @param	new_len	number of uncompressed bytes (length of returned block
			of data)
 * @ret		unsigned char *	address of uncompressed block
 * @remark	In this function, value of either info->chunk_remaining or
 *		info->length is being changed (it depends on if chunked mode is
 *		used or not).
 *		Note that the function is still a little esotheric for me. Don't
 *		take it lightly and don't mess with it without grave reason! If
 *		you dare to touch this without testing the changes on slashdot
 *		and cvsweb (including revision history), don't dare to send me
 *		any patches! ;) --pasky
 */
static unsigned char *
uncompress_data(struct connection *conn, unsigned char *data, int len,
		int *new_len)
{
	struct http_connection_info *info = conn->info;
	/* to_read is number of bytes to be read from the decoder. It is 65536
	 * (then we are just emptying the decoder buffer as we finished the walk
	 * through the incoming stream already) or PIPE_BUF / 2 (when we are
	 * still walking through the stream - then we write PIPE_BUF / 2 to the
	 * pipe and read it back to the decoder ASAP; the point is that we can't
	 * write more than PIPE_BUF to the pipe at once, but we also have to
	 * never let read_encoded() (gzread(), in fact) to empty the pipe - that
	 * causes further malfunction of zlib :[ ... so we will make sure that
	 * we will always have at least PIPE_BUF / 2 + 1 in the pipe (returning
	 * early otherwise)). */
	int to_read = PIPE_BUF / 2, did_read = 0;
	int *length_of_block;
	unsigned char *output = NULL;

	length_of_block = (info->length == LEN_CHUNKED ? &info->chunk_remaining
						       : &info->length);
	if (!*length_of_block) {
		/* Going to finish this decoding bussiness. */
		/* Some nicely big value - empty encoded output queue by reading
		 * big chunks from it. */
		to_read = 65536;
	}

	if (conn->content_encoding == ENCODING_NONE) {
		*new_len = len;
		if (*length_of_block > 0) *length_of_block -= len;
		return data;
	}

	*new_len = 0; /* new_len must be zero if we would ever return NULL */

	if (conn->stream_pipes[0] == -1
	    && (c_pipe(conn->stream_pipes) < 0
		|| set_nonblocking_fd(conn->stream_pipes[0]) < 0
		|| set_nonblocking_fd(conn->stream_pipes[1]) < 0)) {
		return NULL;
	}

	do {
		int init = 0;

		if (to_read == PIPE_BUF / 2) {
			/* ... we aren't finishing yet. */
			int written = write(conn->stream_pipes[1], data,
						len > to_read ? to_read : len);

			if (written > 0) {
				data += written;
				len -= written;

				/* In non-keep-alive connections info->length == -1, so the test below */
				if (*length_of_block > 0)
					*length_of_block -= written;
				/* info->length is 0 at the end of block for all modes: keep-alive,
				 * non-keep-alive and chunked */
				if (!info->length) {
					/* That's all, folks - let's finish this. */
					to_read = 65536;
				} else if (!len) {
					/* We've done for this round (but not done
					 * completely). Thus we will get out with
					 * what we have and leave what we wrote to
					 * the next round - we have to do that since
					 * we MUST NOT ever empty the pipe completely
					 * - this would cause a disaster for
					 * read_encoded(), which would simply not
					 * work right then. */
					return output;
				}
			}
		}

		if (!conn->stream) {
			conn->stream = open_encoded(conn->stream_pipes[0],
					conn->content_encoding);
			if (!conn->stream) return NULL;
			/* On "startup" pipe is treated with care, but if everything
			 * was already written to the pipe, caution isn't necessary */
			else if (to_read != 65536) init = 1;
		} else init = 0;

		output = (unsigned char *) mem_realloc(output, *new_len + to_read);
		if (!output) break;

		did_read = read_encoded(conn->stream, output + *new_len,
					init ? PIPE_BUF / 4 : to_read); /* on init don't read too much */
		if (did_read > 0) *new_len += did_read;
	} while (!(!len && did_read != to_read));

	if (did_read < 0 && output) {
		mem_free(output);
		output = NULL;
	}

	uncompress_shutdown(conn);
	return output;
}

/* FIXME: Unfortunately, we duplicate this in free_connection_data(). */
static void
uncompress_shutdown(struct connection *conn)
{
	if (conn->stream) {
		close_encoded(conn->stream);
		conn->stream = NULL;
	}
	if (conn->stream_pipes[1] >= 0)
		close(conn->stream_pipes[1]);
	conn->stream_pipes[0] = conn->stream_pipes[1] = -1;
}

static int
is_line_in_buffer(struct read_buffer *rb)
{
	int l;

	for (l = 0; l < rb->len; l++) {
		if (rb->data[l] == ASCII_LF) return l + 1;
		if (l < rb->len - 1 && rb->data[l] == ASCII_CR
		    && rb->data[l + 1] == ASCII_LF)
			return l + 2;
		if (l == rb->len - 1 && rb->data[l] == ASCII_CR) return 0;
		if (rb->data[l] < ' ') return -1;
	}
	return 0;
}

void
read_http_data(struct connection *conn, struct read_buffer *rb)
{
	struct http_connection_info *info = conn->info;

	set_timeout(conn);

	if (rb->close == 2) {
		if (conn->content_encoding && info->length == -1) {
			/* Flush uncompression first. */
			info->length = 0;
		} else {

thats_all_folks:
			http_end_request(conn, S_OK);
			return;
		}
	}

	if (info->length != LEN_CHUNKED) {
		unsigned char *data;
		int data_len;
		int len = rb->len;

		if (info->length >= 0 && info->length < len) {
			/* We won't read more than we have to go. */
			len = info->length;
		}

		conn->received += len;

		data = uncompress_data(conn, rb->data, len, &data_len);

		if (add_fragment(conn->cache, conn->from, data, data_len) == 1)
			conn->tries = 0;

		if (data && data != rb->data) mem_free(data);

		conn->from += data_len;

		kill_buffer_data(rb, len);

		if (!info->length && !rb->close)
			goto thats_all_folks;

	} else {
		/* Chunked. Good luck! */
		/* See RFC2616, section 3.6.1. Basically, it looks like:
		 * 1234 ; a = b ; c = d\r\n
		 * aklkjadslkfjalkfjlkajkljfdkljdsfkljdf*1234\r\n
		 * 0\r\n
		 * \r\n */
next_chunk:
		if (info->chunk_remaining == CHUNK_DATA_END) {
			int l = is_line_in_buffer(rb);

			if (l) {
				if (l == -1) {
					/* Invalid character in buffer. */
					abort_conn_with_state(conn,
							      S_HTTP_ERROR);
					return;
				}

				/* Remove everything to the EOLN. */
				kill_buffer_data(rb, l);
				if (l <= 2) {
					/* Empty line. */
					goto thats_all_folks;
				}
				goto next_chunk;
			}

		} else if (info->chunk_remaining == CHUNK_SIZE) {
			int l = is_line_in_buffer(rb);

			if (l) {
				unsigned char *de;
				int n = 0;

				if (l != -1) {
					errno = 0;
					n = strtol(rb->data, (char **)&de, 16);
					if (errno || !*de) {
						abort_conn_with_state(conn, S_HTTP_ERROR);
						return;
					}
				}

				if (l == -1 || de == rb->data) {
					abort_conn_with_state(conn, S_HTTP_ERROR);
					return;
				}

				/* Remove everything to the EOLN. */
				kill_buffer_data(rb, l);
				info->chunk_remaining = n;
				if (!info->chunk_remaining)
					info->chunk_remaining = CHUNK_ZERO_SIZE;
				goto next_chunk;
			}

		} else {
			unsigned char *data;
			int data_len;
			int len;
			int zero = 0;

			zero = (info->chunk_remaining == CHUNK_ZERO_SIZE);
			if (zero) info->chunk_remaining = 0;
			len = info->chunk_remaining;

			/* Maybe everything neccessary didn't come yet.. */
			if (len > rb->len) len = rb->len;
			conn->received += len;

			data = uncompress_data(conn, rb->data, len, &data_len);

			if (add_fragment(conn->cache, conn->from,
					 data, data_len) == 1)
				conn->tries = 0;

			if (data && data != rb->data) mem_free(data);

			conn->from += data_len;

			kill_buffer_data(rb, len);

			if (zero) {
				/* Last chunk has zero length, so this is last
				 * chunk, we finished decompression just now
				 * and now we can happily finish reading this
				 * stuff. */
				info->chunk_remaining = CHUNK_DATA_END;
				goto next_chunk;
			}

			if (!info->chunk_remaining && rb->len > 0) {
				/* Eat newline succeeding each chunk. */
				if (rb->data[0] == 10) {
					kill_buffer_data(rb, 1);
				} else {
					if (rb->data[0] != ASCII_CR
					    || (rb->len >= 2
						&& rb->data[1] != ASCII_LF)) {
						abort_conn_with_state(conn, S_HTTP_ERROR);
						return;
					}
					if (rb->len < 2) goto read_more;
					kill_buffer_data(rb, 2);
				}
				info->chunk_remaining = CHUNK_SIZE;
				goto next_chunk;
			}
		}
	}

read_more:
	read_from_socket(conn, conn->sock1, rb, read_http_data);
	setcstate(conn, S_TRANS);
}

static int
get_header(struct read_buffer *rb)
{
	int i;

	for (i = 0; i < rb->len; i++) {
		unsigned char a = rb->data[i];

		if (!a) return -1;
		if (i < rb->len - 1 && a == ASCII_LF && rb->data[i + 1] == ASCII_LF) return i + 2;
		if (i < rb->len - 3 && a == ASCII_CR) {
			if (rb->data[i + 1] == ASCII_CR) continue;
			if (rb->data[i + 1] != ASCII_LF) return -1;
			if (rb->data[i + 2] == ASCII_CR) {
				if (rb->data[i + 3] != ASCII_LF) return -1;
				return i + 4;
			}
		}
	}
	return 0;
}

static void
http_got_header(struct connection *c, struct read_buffer *rb)
{
	int cf;
	int state = c->state != S_PROC ? S_GETH : S_PROC;
	unsigned char *head;
#ifdef COOKIES
	unsigned char *cookie, *ch;
#endif
	int a, h, version;
	unsigned char *d;
	struct cache_entry *e;
	struct http_connection_info *info;
	unsigned char *host = GET_REAL_URL(c->url);

	set_timeout(c);
	info = c->info;
	if (rb->close == 2) {
		unsigned char *hstr;

		if (!c->tries && (hstr = get_host_name(host))) {
			if (info->bl_flags & BL_NO_CHARSET) {
				del_blacklist_entry(hstr, BL_NO_CHARSET);
			} else {
				add_blacklist_entry(hstr, BL_NO_CHARSET);
				c->tries = -1;
			}
			mem_free(hstr);
		}
		retry_conn_with_state(c, S_CANT_READ);
		return;
	}
	rb->close = 0;

again:
	a = get_header(rb);
	if (a == -1) {
		abort_conn_with_state(c, S_HTTP_ERROR);
		return;
	}
	if (!a) {
		read_from_socket(c, c->sock1, rb, http_got_header);
		setcstate(c, state);
		return;
	}
	if (get_http_code(rb->data, &h, &version) || h == 101) {
		abort_conn_with_state(c, S_HTTP_ERROR);
		return;
	}

	head = mem_alloc(a + 1);
	if (!head) {
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}
	memcpy(head, rb->data, a);
	head[a] = 0;
	if (check_http_server_bugs(host, c->info, head)) {
		mem_free(head);
		retry_conn_with_state(c, S_RESTART);
		return;
	}

#ifdef COOKIES
	ch = head;
	while ((cookie = parse_http_header(ch, "Set-Cookie", &ch))) {
		unsigned char *hstr = GET_REAL_URL(c->url);

		set_cookie(NULL, hstr, cookie);
		mem_free(cookie);
	}
#endif

	if (h == 100) {
		mem_free(head);
		state = S_PROC;
		kill_buffer_data(rb, a);
		goto again;
	}
	if (h < 200) {
		mem_free(head);
		abort_conn_with_state(c, S_HTTP_ERROR);
		return;
	}
	if (h == 304) {
		mem_free(head);
		http_end_request(c, S_OK);
		return;
	}
	if (get_cache_entry(c->url, &e)) {
		mem_free(head);
		abort_conn_with_state(c, S_OUT_OF_MEM);
		return;
	}
	if (e->head) mem_free(e->head);
	e->head = head;

	if (!get_opt_bool("document.cache.ignore_cache_control")) {
		if ((d = parse_http_header(e->head, "Cache-Control", NULL))
		    || (d = parse_http_header(e->head, "Pragma", NULL)))
		{
			if (strstr(d, "no-cache")) {
				e->cache_mode = NC_PR_NO_CACHE;
			}
			mem_free(d);
		}
	}

	if (c->ssl) {
		if (e->ssl_info) mem_free(e->ssl_info);
		e->ssl_info = get_ssl_cipher_str(c->ssl);
	}

	if (h == 204) {
		http_end_request(c, S_OK);
		return;
	}
	if (h == 301 || h == 302 || h == 303) {
		d = parse_http_header(e->head, "Location", NULL);
		if (d) {
			if (e->redirect) mem_free(e->redirect);
			e->redirect = d;
			e->redirect_get = h == 303;
		}
	}

	if (h == 401) {
		d = parse_http_header(e->head, "WWW-Authenticate", NULL);
		if (d) {
			if (!strncasecmp(d, "Basic", 5)) {
				unsigned char *realm = get_http_header_param(d, "realm");

				if (realm) {
					if (add_auth_entry(host, realm) > 0) {
						add_questions_entry(do_auth_dialog);
					}
					mem_free(realm);
				}
			}
			mem_free(d);
		}
	}

	kill_buffer_data(rb, a);
	c->cache = e;
	info->close = 0;
	info->length = -1;
	info->version = version;
	if ((d = parse_http_header(e->head, "Connection", NULL))
	     || (d = parse_http_header(e->head, "Proxy-Connection", NULL))) {
		if (!strcasecmp(d, "close")) info->close = 1;
		mem_free(d);
	} else if (version < 11) info->close = 1;

	cf = c->from;
	c->from = 0;
	d = parse_http_header(e->head, "Content-Range", NULL);
	if (d) {
		if (strlen(d) > 6) {
			d[5] = 0;
			if (!(strcasecmp(d, "bytes")) && d[6] >= '0' && d[6] <= '9') {
				int f;

				errno = 0;
			       	f = strtol(d + 6, NULL, 10);

				if (!errno && f >= 0) c->from = f;
			}
		}
		mem_free(d);
	}
	if (cf && !c->from && !c->unrestartable) c->unrestartable = 1;
	if ((c->prg.start <= 0 && c->from > cf) || c->from < 0) {
		/* We don't want this if c->prg.start because then c->from will
		 * be probably value of c->prg.start, while cf is 0. */
		abort_conn_with_state(c, S_HTTP_ERROR);
		return;
	}

#if 0
	{
		struct status *s;
		foreach (s, c->statuss) {
			fprintf(stderr, "c %p status %p pri %d st %d er %d :: ce %s",
				c, s, s->pri, s->state, s->prev_error,
				s->ce ? s->ce->url : (unsigned char *) "N-U-L-L");
		}
	}
#endif

	if (c->prg.start >= 0) {
		/* Update to the real value which we've got from Content-Range. */
		c->prg.seek = c->from;
	}
	c->prg.start = c->from;

	d = parse_http_header(e->head, "Content-Length", NULL);
	if (d) {
		unsigned char *ep;
		int l;

		errno = 0;
		l = strtol(d, (char **)&ep, 10);

		if (!errno && !*ep && l >= 0) {
			if (!info->close || version >= 11) info->length = l;
			c->est_length = c->from + l;
		}
		mem_free(d);
	}

	d = parse_http_header(e->head, "Accept-Ranges", NULL);
	if (d) {
		if (!strcasecmp(d, "none") && !c->unrestartable)
			c->unrestartable = 1;
		mem_free(d);
	} else if (!c->unrestartable && !c->from) c->unrestartable = 1;

	d = parse_http_header(e->head, "Transfer-Encoding", NULL);
	if (d) {
		if (!strcasecmp(d, "chunked")) {
			info->length = LEN_CHUNKED;
			info->chunk_remaining = CHUNK_SIZE;
		}
		mem_free(d);
	}
	if (!info->close && info->length == -1) info->close = 1;

	d = parse_http_header(e->head, "Last-Modified", NULL);
	if (d) {
		if (e->last_modified && strcasecmp(e->last_modified, d)) {
			delete_entry_content(e);
			if (c->from) {
				c->from = 0;
				mem_free(d);
				retry_conn_with_state(c, S_MODIFIED);
				return;
			}
		}
		if (!e->last_modified) e->last_modified = d;
		else mem_free(d);
	}
	if (!e->last_modified) {
	       	d = parse_http_header(e->head, "Date", NULL);
		if (d) e->last_modified = d;
	}

	d = parse_http_header(e->head, "ETag", NULL);
	if (d) {
		if (e->etag && strcasecmp(e->etag, d)) {
			delete_entry_content(e);
			if (c->from) {
				c->from = 0;
				mem_free(d);
				retry_conn_with_state(c, S_MODIFIED);
				return;
			}
		}
		if (!e->etag) e->etag = d;
		else mem_free(d);
	}

	if (info->length == -1 || (version < 11 && info->close)) rb->close = 1;

	d = parse_http_header(e->head, "Content-Type", NULL);
	if (d) {
		if (!strncmp(d, "text", 4)) {
			mem_free(d);
			d = parse_http_header(e->head, "Content-Encoding", NULL);
			if (d) {
#ifdef HAVE_ZLIB_H
				if (!strcasecmp(d, "gzip") || !strcasecmp(d, "x-gzip"))
					c->content_encoding = ENCODING_GZIP;
#endif
#ifdef HAVE_BZLIB_H
				if (!strcasecmp(d, "bzip2") || !strcasecmp(d, "x-bzip2"))
					c->content_encoding = ENCODING_BZIP2;
#endif
				mem_free(d);
			}
		} else {
			mem_free(d);
		}
	}
	if (c->content_encoding != ENCODING_NONE) {
		if (e->encoding_info) mem_free(e->encoding_info);
		e->encoding_info = stracpy(encoding_names[c->content_encoding]);
	}

	read_http_data(c, rb);
}

static void
http_get_header(struct connection *conn)
{
	struct read_buffer *rb;

	set_timeout(conn);

	rb = alloc_read_buffer(conn);
	if (!rb) return;
	rb->close = 1;
	read_from_socket(conn, conn->sock1, rb, http_got_header);
}
