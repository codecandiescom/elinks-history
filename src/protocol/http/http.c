/* Internal "http" protocol implementation */
/* $Id: http.c,v 1.343 2004/10/13 15:52:31 zas Exp $ */

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

#include "config/options.h"
#include "cookies/cookies.h"
#include "cache/cache.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/connect.h"
#include "lowlevel/sysname.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "protocol/auth/auth.h"
#include "protocol/header.h"
#include "protocol/http/codes.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "ssl/connect.h"
#include "ssl/ssl.h"
#include "terminal/terminal.h"
#include "util/base64.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"

static void uncompress_shutdown(struct connection *);


unsigned char *
subst_user_agent(unsigned char *fmt, unsigned char *version,
		 unsigned char *sysname, unsigned char *termsize)
{
	struct string agent;

	if (!init_string(&agent)) return NULL;

	while (*fmt) {
		int p;

		for (p = 0; fmt[p] && fmt[p] != '%'; p++);

		add_bytes_to_string(&agent, fmt, p);
		fmt += p;

		if (*fmt != '%') continue;

		fmt++;
		switch (*fmt) {
			case 'b':
				if (!list_empty(sessions)) {
					unsigned char bs[4] = "";
					int blen = 0;
					struct session *ses = sessions.prev;
					int bars = ses->status.show_status_bar
						+ ses->status.show_tabs_bar
						+ ses->status.show_title_bar;

					ulongcat(bs, &blen, bars, 2, 0);
					add_to_string(&agent, bs);
				}
				break;
			case 'v':
				add_to_string(&agent, version);
				break;
			case 's':
				add_to_string(&agent, sysname);
				break;
			case 't':
				if (termsize)
					add_to_string(&agent, termsize);
				break;
			default:
				add_bytes_to_string(&agent, fmt - 1, 2);
				break;
		}
		if (*fmt) fmt++;
	}

	return agent.source;
}

static void
add_url_to_http_string(struct string *header, struct uri *uri, int components)
{
	/* This block substitues spaces in URL by %20s. This is
	 * certainly not the right place where to do it, but now the
	 * behaviour is at least improved compared to what we had
	 * before. We should probably encode all URLs as early as
	 * possible, and possibly decode them back in protocol
	 * backends. --pasky */
	unsigned char *string = get_uri_string(uri, components);
	unsigned char *data = string;

	if (!string) return;

	while (*data) {
		int len = strcspn(data, " \t\r\n\\");

		add_bytes_to_string(header, data, len);

		if (!data[len]) break;

		if (data[len++] == '\\')
			add_char_to_string(header, '/');
		else
			add_to_string(header, "%20");

		data	+= len;
	}

	mem_free(string);
}

/* Parse from @end - 1 to @start and set *@value to integer found.
 * It returns -1 if not a number, 0 otherwise.
 * @end should be > @start. */
static int
revstr2num(unsigned char *start, unsigned char *end, int *value)
{
	int q = 1, val = 0;

	do {
		--end;
		if (!isdigit(*end)) return -1; /* NaN */
		val += (*end - '0') * q;
		q *= 10;
	} while (end > start);

	*value = val;
	return 0;
}

/* This function extracts code, major and minor version from string
 * "\s*HTTP/\d+.\d+\s+\d\d\d..."
 * It returns a negative value on error, 0 on success.
 */
static int
get_http_code(unsigned char *head, int *code, struct http_version *version)
{
	unsigned char *start;

	*code = 0;
	version->major = 0;
	version->minor = 0;

	/* Ignore spaces. */
	while (*head == ' ') head++;

	/* HTTP/ */
	if (toupper(*head) != 'H' || toupper(*++head) != 'T' ||
	    toupper(*++head) != 'T' || toupper(*++head) != 'P'
	    || *++head != '/')
		return -1;

	/* Version */
	start = ++head;
	/* Find next '.' */
	while (*head && *head != '.') head++;
	/* Sanity check. */
	if (!*head || !(head - start)
	    || (head - start) > 4
	    || !isdigit(*(head + 1)))
		return -2;

	/* Extract major version number. */
	if (revstr2num(start, head, &version->major)) return -3; /* NaN */

	start = head + 1;

	/* Find next ' '. */
	while (*head && *head != ' ') head++;
	/* Sanity check. */
	if (!*head || !(head - start) || (head - start) > 4) return -4;

	/* Extract minor version number. */
	if (revstr2num(start, head, &version->minor)) return -5; /* NaN */

	/* Ignore spaces. */
	while (*head == ' ') head++;

	/* Sanity check for code. */
	if (head[0] < '1' || head[0] > '9' ||
	    !isdigit(head[1]) ||
	    !isdigit(head[2]))
		return -6; /* Invalid code. */

	/* Extract code. */
	*code = (head[0] - '0') * 100 + (head[1] - '0') * 10 + head[2] - '0';

	return 0;
}

static int
check_http_server_bugs(struct uri *uri, struct http_connection_info *info,
		       unsigned char *head)
{
	unsigned char *server, **s;
	static unsigned char *buggy_servers[] = {
		"mod_czech/3.1.0",
		"Purveyor",
		"Netscape-Enterprise",
		NULL
	};

	if (!get_opt_int("protocol.http.bugs.allow_blacklist")
	    || HTTP_1_0(info->sent_version))
		return 0;

	server = parse_header(head, "Server", NULL);
	if (!server)
		return 0;

	for (s = buggy_servers; *s; s++) {
		if (strstr(server, *s)) {
			add_blacklist_entry(uri, SERVER_BLACKLIST_HTTP10);
			break;
		}
	}

	mem_free(server);
	return (*s != NULL);
}

static void
http_end_request(struct connection *conn, enum connection_state state,
		 int notrunc)
{
	set_connection_state(conn, state);
	uncompress_shutdown(conn);

	if (conn->state == S_OK && conn->cached) {
		if (!notrunc) truncate_entry(conn->cached, conn->from, 1);
		conn->cached->incomplete = 0;
		conn->cached->preformatted = 0;
	}

	if (conn->info && !((struct http_connection_info *) conn->info)->close
	    && (!conn->socket.ssl) /* We won't keep alive ssl connections */
	    && (!get_opt_int("protocol.http.bugs.post_no_keepalive")
		|| !conn->uri->post)) {
		add_keepalive_connection(conn, HTTP_KEEPALIVE_TIMEOUT, NULL);
	} else {
		abort_connection(conn);
	}
}

static void http_send_header(struct connection *);

void
http_protocol_handler(struct connection *conn)
{
	/* setcstate(conn, S_CONN); */
	set_connection_timeout(conn);

	if (!has_keepalive_connection(conn)) {
		make_connection(conn, &conn->socket, http_send_header);
	} else {
		http_send_header(conn);
	}
}

void
proxy_protocol_handler(struct connection *conn)
{
	http_protocol_handler(conn);
}

static void http_get_header(struct connection *);

#define IS_PROXY_URI(x) ((x)->protocol == PROTOCOL_PROXY)

#define connection_is_https_proxy(conn) \
	(IS_PROXY_URI((conn)->uri) && (conn)->proxied_uri->protocol == PROTOCOL_HTTPS)

static void
http_send_header(struct connection *conn)
{
	static unsigned char *accept_charset = NULL;
	struct http_connection_info *info;
	int trace = get_opt_bool("protocol.http.trace");
	struct string header;
	unsigned char *host_data, *post_data = NULL;
	struct uri *uri = conn->proxied_uri; /* Set to the real uri */
	unsigned char *optstr;
	int use_connect, talking_to_proxy;

	set_connection_timeout(conn);

	/* Sanity check for a host */
	if (!uri || !uri->host || !*uri->host || !uri->hostlen) {
		http_end_request(conn, S_BAD_URL, 0);
		return;
	}

	info = mem_calloc(1, sizeof(struct http_connection_info));
	if (!info) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	/* If called from HTTPS proxy connection the connection info might have
	 * already been allocated. */
	mem_free_set(&conn->info, info);

	info->sent_version.major = 1;
	info->sent_version.minor = 1;
	info->bl_flags = get_blacklist_flags(uri);

	if (info->bl_flags & SERVER_BLACKLIST_HTTP10
	    || get_opt_int("protocol.http.bugs.http10")) {
		info->sent_version.major = 1;
		info->sent_version.minor = 0;
	}

	if (!init_string(&header)) {
		http_end_request(conn, S_OUT_OF_MEM, 0);
		return;
	}

	talking_to_proxy = IS_PROXY_URI(conn->uri) && !conn->socket.ssl;
	use_connect = connection_is_https_proxy(conn) && !conn->socket.ssl;

	if (trace) {
		add_to_string(&header, "TRACE ");
	} else if (use_connect) {
		add_to_string(&header, "CONNECT ");
	} else if (uri->post) {
		add_to_string(&header, "POST ");
		conn->unrestartable = 1;
	} else {
		add_to_string(&header, "GET ");
	}

	if (!talking_to_proxy) {
		add_char_to_string(&header, '/');
	}

	if (use_connect) {
		/* Add port if it was specified or the default port */
		add_uri_to_string(&header, uri, URI_HTTP_CONNECT);
	} else {
		if (connection_is_https_proxy(conn) && conn->socket.ssl) {
			add_url_to_http_string(&header, uri, URI_DATA);

		} else if (talking_to_proxy) {
			add_url_to_http_string(&header, uri, URI_PROXY);

		} else {
			add_url_to_http_string(&header, conn->uri, URI_DATA);
		}
	}

	add_to_string(&header, " HTTP/");
	add_long_to_string(&header, info->sent_version.major);
	add_char_to_string(&header, '.');
	add_long_to_string(&header, info->sent_version.minor);
	add_crlf_to_string(&header);

	add_to_string(&header, "Host: ");
	add_uri_to_string(&header, uri, URI_HTTP_HOST);
	add_crlf_to_string(&header);

	optstr = get_opt_str("protocol.http.proxy.user");
	if (optstr[0] && talking_to_proxy) {
		unsigned char *proxy_data;

		proxy_data = straconcat(optstr, ":",
					get_opt_str("protocol.http.proxy.passwd"),
					NULL);
		if (proxy_data) {
			unsigned char *proxy_64 = base64_encode(proxy_data);

			if (proxy_64) {
				add_to_string(&header, "Proxy-Authorization: Basic ");
				add_to_string(&header, proxy_64);
				add_crlf_to_string(&header);
				mem_free(proxy_64);
			}
			mem_free(proxy_data);
		}
	}

	optstr = get_opt_str("protocol.http.user_agent");
	if (*optstr && strcmp(optstr, " ")) {
		unsigned char *ustr, ts[64] = "";

		add_to_string(&header, "User-Agent: ");

		if (!list_empty(terminals)) {
			unsigned int tslen = 0;
			struct terminal *term = terminals.prev;

			ulongcat(ts, &tslen, term->width, 3, 0);
			ts[tslen++] = 'x';
			ulongcat(ts, &tslen, term->height, 3, 0);
		}
		ustr = subst_user_agent(optstr, VERSION_STRING, system_name,
					ts);

		if (ustr) {
			add_to_string(&header, ustr);
			mem_free(ustr);
		}

		add_crlf_to_string(&header);
	}

	switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			break;

		case REFERER_FAKE:
			optstr = get_opt_str("protocol.http.referer.fake");
			if (!optstr[0]) break;
			add_to_string(&header, "Referer: ");
			add_to_string(&header, optstr);
			add_crlf_to_string(&header);
			break;

		case REFERER_TRUE:
			if (!conn->referrer) break;
			add_to_string(&header, "Referer: ");
			add_url_to_http_string(&header, conn->referrer, URI_HTTP_REFERRER);
			add_crlf_to_string(&header);
			break;

		case REFERER_SAME_URL:
			add_to_string(&header, "Referer: ");
			add_url_to_http_string(&header, uri, URI_HTTP_REFERRER);
			add_crlf_to_string(&header);
			break;
	}

	add_to_string(&header, "Accept: */*");
	add_crlf_to_string(&header);

	/* TODO: Make this encoding.c function. */
#if defined(CONFIG_GZIP) || defined(CONFIG_BZIP2)
	add_to_string(&header, "Accept-Encoding: ");

#ifndef BUG_517
#ifdef CONFIG_BZIP2
	add_to_string(&header, "bzip2");
#endif
#endif

#ifdef CONFIG_GZIP

#ifndef BUG_517
#ifdef CONFIG_BZIP2
	add_to_string(&header, ", ");
#endif
#endif

	add_to_string(&header, "gzip");
#endif
	add_crlf_to_string(&header);
#endif

	if (!accept_charset) {
		struct string ac;

		if (init_string(&ac)) {
			unsigned char *cs;
			int i;

			for (i = 0; (cs = get_cp_mime_name(i)); i++) {
				if (ac.length) {
					add_to_string(&ac, ", ");
				} else {
					add_to_string(&ac, "Accept-Charset: ");
				}
				add_to_string(&ac, cs);
			}

			if (ac.length) {
				add_crlf_to_string(&ac);
			}

			/* Never freed until exit(), if you found a  better solution,
			 * let us now ;)
			 * Do not use mem_alloc() here. */
			accept_charset = malloc(ac.length + 1);
			if (accept_charset) {
				strcpy(accept_charset, ac.source);
			} else {
				accept_charset = "";
			}

			done_string(&ac);
		}
	}

	if (!(info->bl_flags & SERVER_BLACKLIST_NO_CHARSET)
	    && !get_opt_int("protocol.http.bugs.accept_charset")) {
		add_to_string(&header, accept_charset);
	}

	optstr = get_opt_str("protocol.http.accept_language");
	if (optstr[0]) {
		add_to_string(&header, "Accept-Language: ");
		add_to_string(&header, optstr);
		add_crlf_to_string(&header);
	}
#ifdef ENABLE_NLS
	else if (get_opt_bool("protocol.http.accept_ui_language")) {
		unsigned char *code = language_to_iso639(current_language);

		if (code) {
			add_to_string(&header, "Accept-Language: ");
			add_to_string(&header, code);
			add_crlf_to_string(&header);
		}
	}
#endif

	/* FIXME: What about post-HTTP/1.1?? --Zas */
	if (HTTP_1_1(info->sent_version)) {
		if (!IS_PROXY_URI(conn->uri)) {
			add_to_string(&header, "Connection: ");
		} else {
			add_to_string(&header, "Proxy-Connection: ");
		}

		if (!uri->post || !get_opt_int("protocol.http.bugs.post_no_keepalive")) {
			add_to_string(&header, "Keep-Alive");
		} else {
			add_to_string(&header, "close");
		}
		add_crlf_to_string(&header);
	}

	if (conn->cached) {
		if (!conn->cached->incomplete && conn->cached->head && conn->cached->last_modified
		    && conn->cache_mode <= CACHE_MODE_CHECK_IF_MODIFIED) {
			add_to_string(&header, "If-Modified-Since: ");
			add_to_string(&header, conn->cached->last_modified);
			add_crlf_to_string(&header);
		}
	}

	if (conn->cache_mode >= CACHE_MODE_FORCE_RELOAD) {
		add_to_string(&header, "Pragma: no-cache");
		add_crlf_to_string(&header);
		add_to_string(&header, "Cache-Control: no-cache");
		add_crlf_to_string(&header);
	}

	if (conn->from || (conn->progress.start > 0)) {
		/* conn->from takes precedence. conn->progress.start is set only the first
		 * time, then conn->from gets updated and in case of any retries
		 * etc we have everything interesting in conn->from already. */
		add_to_string(&header, "Range: bytes=");
		add_long_to_string(&header, conn->from ? conn->from : conn->progress.start);
		add_char_to_string(&header, '-');
		add_crlf_to_string(&header);
	}

	host_data = find_auth(uri);
	if (host_data) {
		add_to_string(&header, "Authorization: Basic ");
		add_to_string(&header, host_data);
		add_crlf_to_string(&header);
		mem_free(host_data);
	}

	if (uri->post) {
		/* We search for first '\n' in uri->post to get content type
		 * as set by get_form_uri(). This '\n' is dropped if any
		 * and replaced by correct '\r\n' termination here. */
		unsigned char *postend = strchr(uri->post, '\n');

		if (postend) {
			add_to_string(&header, "Content-Type: ");
			add_bytes_to_string(&header, uri->post, postend - uri->post);
			add_crlf_to_string(&header);
		}

		post_data = postend ? postend + 1 : uri->post;
		add_to_string(&header, "Content-Length: ");
		add_long_to_string(&header, strlen(post_data) / 2);
		add_crlf_to_string(&header);
	}

#ifdef CONFIG_COOKIES
	{
		struct string *cookies = send_cookies(uri);

		if (cookies) {
			add_to_string(&header, "Cookie: ");
			add_string_to_string(&header, cookies);
			add_crlf_to_string(&header);
			done_string(cookies);
		}
	}
#endif

	add_crlf_to_string(&header);

	if (post_data) {
#define POST_BUFFER_SIZE 4096
		unsigned char *post = post_data;
		unsigned char buffer[POST_BUFFER_SIZE];
		int n = 0;

		while (post[0] && post[1]) {
			int h1, h2;

			h1 = unhx(post[0]);
			assert(h1 >= 0 && h1 < 16);
			if_assert_failed h1 = 0;

			h2 = unhx(post[1]);
			assert(h2 >= 0 && h2 < 16);
			if_assert_failed h2 = 0;

			buffer[n++] = (h1<<4) + h2;
			post += 2;
			if (n == POST_BUFFER_SIZE) {
				add_bytes_to_string(&header, buffer, n);
				n = 0;
			}
		}

		if (n)
			add_bytes_to_string(&header, buffer, n);
#undef POST_BUFFER_SIZE
	}

	write_to_socket(conn, &conn->socket, header.source, header.length,
			http_get_header);
	done_string(&header);

	set_connection_state(conn, S_SENT);
}


/* This function uncompresses the data block given in @data (if it was
 * compressed), which is long @len bytes. The uncompressed data block is given
 * back to the world as the return value and its length is stored into
 * @new_len.
 *
 * In this function, value of either info->chunk_remaining or info->length is
 * being changed (it depends on if chunked mode is used or not).
 *
 * Note that the function is still a little esotheric for me. Don't take it
 * lightly and don't mess with it without grave reason! If you dare to touch
 * this without testing the changes on slashdot, freshmeat and cvsweb
 * (including revision history), don't dare to send me any patches! ;) --pasky
 *
 * This function gotta die. */
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

#define BIG_READ 65536
	if (!*length_of_block) {
		/* Going to finish this decoding bussiness. */
		/* Some nicely big value - empty encoded output queue by reading
		 * big chunks from it. */
		to_read = BIG_READ;
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
			int written = safe_write(conn->stream_pipes[1], data,
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
					to_read = BIG_READ;
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
			else if (to_read != BIG_READ) init = 1;
		} else init = 0;

		output = (unsigned char *) mem_realloc(output, *new_len + to_read);
		if (!output) break;

		did_read = read_encoded(conn->stream, output + *new_len,
					init ? PIPE_BUF / 4 : to_read); /* on init don't read too much */
		if (did_read > 0) *new_len += did_read;
		else if (did_read == -1) {
			mem_free_set(&output, NULL);
			*new_len = 0;
			break; /* Loop prevention (bug 517), is this correct ? --Zas */
		}
	} while (!(!len && did_read != to_read));

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
		unsigned char a0 = rb->data[l];

		if (a0 == ASCII_LF)
			return l + 1;
		if (a0 == ASCII_CR) {
			if (rb->data[l + 1] == ASCII_LF
			    && l < rb->len - 1)
				return l + 2;
			if (l == rb->len - 1)
				return 0;
		}
		if (a0 < ' ')
			return -1;
	}
	return 0;
}

static void read_http_data(struct connection *conn, struct read_buffer *rb);

static void
read_more_http_data(struct connection *conn, struct read_buffer *rb,
                    int already_got_anything)
{
	read_from_socket(conn, &conn->socket, rb, read_http_data);
	if (already_got_anything)
		set_connection_state(conn, S_TRANS);
}

static void
read_http_data_done(struct connection *conn)
{
	struct http_connection_info *info = conn->info;

	/* There's no content but an error so just print
	 * that instead of nothing. */
	if (!conn->from) {
		if (info->http_code >= 400) {
			http_error_document(conn, info->http_code);

		} else {
			/* This is not an error, thus fine. No need generate any
			 * document, as this may be empty and it's not a problem.
			 * In case of 3xx, we're probably just getting kicked to
			 * another page anyway. And in case of 2xx, the document
			 * may indeed be empty and thus the user should see it so. */
		}
	}

	http_end_request(conn, S_OK, 0);
}

/* Returns:
 * -1 on error
 * 0 if more to read
 * 1 if done
 */
static int
read_chunked_http_data(struct connection *conn, struct read_buffer *rb)
{
	struct http_connection_info *info = conn->info;
	int total_data_len = 0;

	while (1) {
		/* Chunked. Good luck! */
		/* See RFC2616, section 3.6.1. Basically, it looks like:
		 * 1234 ; a = b ; c = d\r\n
		 * aklkjadslkfjalkfjlkajkljfdkljdsfkljdf*1234\r\n
		 * 0\r\n
		 * \r\n */
		if (info->chunk_remaining == CHUNK_DATA_END) {
			int l = is_line_in_buffer(rb);

			if (l) {
				if (l == -1) {
					/* Invalid character in buffer. */
					return -1;
				}

				/* Remove everything to the EOLN. */
				kill_buffer_data(rb, l);
				if (l <= 2) {
					/* Empty line. */
					return 2;
				}
				continue;
			}

		} else if (info->chunk_remaining == CHUNK_SIZE) {
			int l = is_line_in_buffer(rb);

			if (l) {
				unsigned char *de;
				int n = 0;

				if (l != -1) {
					errno = 0;
					n = strtol(rb->data, (char **) &de, 16);
					if (errno || !*de) {
						return -1;
					}
				}

				if (l == -1 || de == rb->data) {
					return -1;
				}

				/* Remove everything to the EOLN. */
				kill_buffer_data(rb, l);
				info->chunk_remaining = n;
				if (!info->chunk_remaining)
					info->chunk_remaining = CHUNK_ZERO_SIZE;
				continue;
			}

		} else {
			unsigned char *data;
			int data_len;
			int len;
			int zero = (info->chunk_remaining == CHUNK_ZERO_SIZE);

			if (zero) info->chunk_remaining = 0;
			len = info->chunk_remaining;

			/* Maybe everything necessary didn't come yet.. */
			int_upper_bound(&len, rb->len);
			conn->received += len;

			data = uncompress_data(conn, rb->data, len, &data_len);

			if (add_fragment(conn->cached, conn->from,
					 data, data_len) == 1)
				conn->tries = 0;

			if (data && data != rb->data) mem_free(data);

			conn->from += data_len;
			total_data_len += data_len;

			kill_buffer_data(rb, len);

			if (zero) {
				/* Last chunk has zero length, so this is last
				 * chunk, we finished decompression just now
				 * and now we can happily finish reading this
				 * stuff. */
				info->chunk_remaining = CHUNK_DATA_END;
				continue;
			}

			if (!info->chunk_remaining && rb->len > 0) {
				/* Eat newline succeeding each chunk. */
				if (rb->data[0] == ASCII_LF) {
					kill_buffer_data(rb, 1);
				} else {
					if (rb->data[0] != ASCII_CR
					    || (rb->len >= 2
						&& rb->data[1] != ASCII_LF)) {
						return -1;
					}
					if (rb->len < 2) break;
					kill_buffer_data(rb, 2);
				}
				info->chunk_remaining = CHUNK_SIZE;
				continue;
			}
		}
		break;
	}

	/* More to read. */
	return !!total_data_len;
}

/* Returns 0 if more data, 1 if done. */
static int
read_normal_http_data(struct connection *conn, struct read_buffer *rb)
{
	struct http_connection_info *info = conn->info;
	unsigned char *data;
	int data_len;
	int len = rb->len;

	if (info->length >= 0 && info->length < len) {
		/* We won't read more than we have to go. */
		len = info->length;
	}

	conn->received += len;

	data = uncompress_data(conn, rb->data, len, &data_len);

	if (add_fragment(conn->cached, conn->from, data, data_len) == 1)
		conn->tries = 0;

	if (data && data != rb->data) mem_free(data);

	conn->from += data_len;

	kill_buffer_data(rb, len);

	if (!info->length && !rb->close) {
		return 2;
	}

	return !!data_len;
}

static void
read_http_data(struct connection *conn, struct read_buffer *rb)
{
	struct http_connection_info *info = conn->info;
	int ret;

	set_connection_timeout(conn);

	if (rb->close == 2) {
		if (conn->content_encoding && info->length == -1) {
			/* Flush uncompression first. */
			info->length = 0;
		} else {
			read_http_data_done(conn);
			return;
		}
	}

	if (info->length != LEN_CHUNKED) {
		ret = read_normal_http_data(conn, rb);

	} else {
		ret = read_chunked_http_data(conn, rb);
	}

	switch (ret) {
	case -1:
		abort_conn_with_state(conn, S_HTTP_ERROR);
		break;
	case 0:
		read_more_http_data(conn, rb, 0);
		break;
	case 1:
		read_more_http_data(conn, rb, 1);
		break;
	case 2:
		read_http_data_done(conn);
		break;
	default:
		INTERNAL("Unexpected return value: %d", ret);
	}
}

/* Returns offset of the header end, zero if more data is needed, -1 when
 * incorrect data was received, -2 if this is HTTP/0.9 and no header is to
 * come. */
static int
get_header(struct read_buffer *rb)
{
	int i;

	/* XXX: We will have to do some guess about whether an HTTP header is
	 * coming or not, in order to support HTTP/0.9 reply correctly. This
	 * means a little code duplication with get_http_code(). --pasky */
	if (rb->len > 4 && strncasecmp(rb->data, "HTTP/", 5))
		return -2;

	for (i = 0; i < rb->len; i++) {
		unsigned char a0 = rb->data[i];
		unsigned char a1 = rb->data[i + 1];

		if (a0 == 0) {
			rb->data[i] = ' ';
			continue;
		}
		if (a0 == ASCII_LF && a1 == ASCII_LF
		    && i < rb->len - 1)
			return i + 2;
		if (a0 == ASCII_CR && i < rb->len - 3) {
			if (a1 == ASCII_CR) continue;
			if (a1 != ASCII_LF) return -1;
			if (rb->data[i + 2] == ASCII_CR) {
				if (rb->data[i + 3] != ASCII_LF) return -1;
				return i + 4;
			}
		}
	}

	return 0;
}

void
http_got_header(struct connection *conn, struct read_buffer *rb)
{
	struct http_connection_info *info = conn->info;
	unsigned char *head;
#ifdef CONFIG_COOKIES
	unsigned char *cookie, *ch;
#endif
	unsigned char *d;
	struct uri *uri = conn->proxied_uri; /* Set to the real uri */
	struct http_version version;
	enum connection_state state = (conn->state != S_PROC ? S_GETH : S_PROC);
	int a, h = 200;
	int cf;

	set_connection_timeout(conn);

	if (rb->close == 2) {
		if (!conn->tries && uri->host) {
			if (info->bl_flags & SERVER_BLACKLIST_NO_CHARSET) {
				del_blacklist_entry(uri, SERVER_BLACKLIST_NO_CHARSET);
			} else {
				add_blacklist_entry(uri, SERVER_BLACKLIST_NO_CHARSET);
				conn->tries = -1;
			}
		}
		retry_conn_with_state(conn, S_CANT_READ);
		return;
	}
	rb->close = 0;

again:
	a = get_header(rb);
	if (a == -1) {
		abort_conn_with_state(conn, S_HTTP_ERROR);
		return;
	}
	if (!a) {
		read_from_socket(conn, &conn->socket, rb, http_got_header);
		set_connection_state(conn, state);
		return;
	}
	if (a == -2) a = 0;
	if ((a && get_http_code(rb->data, &h, &version))
	    || h == 101) {
		abort_conn_with_state(conn, S_HTTP_ERROR);
		return;
	}

	/* When no header, HTTP/0.9 document. That's always text/html,
	 * according to
	 * http://www.w3.org/Protocols/HTTP/AsImplemented.html. */
	/* FIXME: This usage of fake protocol headers for setting up the
	 * content type has been obsoleted by the @content_type member of
	 * {struct cache_entry}. */
	head = (a ? memacpy(rb->data, a)
		  : stracpy("\r\nContent-Type: text/html\r\n"));
	if (!head) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	if (check_http_server_bugs(uri, info, head)) {
		mem_free(head);
		retry_conn_with_state(conn, S_RESTART);
		return;
	}

	if (uri->protocol == PROTOCOL_FILE) {
		/* ``Status'' is not a standard HTTP header field although some
		 * HTTP servers like www.php.net uses it for some reason. It should
		 * only be used for CGI scripts so that it does not interfere
		 * with status code depended handling for ``normal'' HTTP like
		 * redirects. */
		d = parse_header(head, "Status", NULL);
		if (d) {
			int h2 = atoi(d);

			mem_free(d);
			if (h2 >= 100 && h2 < 600) h = h2;
			if (h == 101) {
				mem_free(head);
				abort_conn_with_state(conn, S_HTTP_ERROR);
				return;
			}
		}
	}

#ifdef CONFIG_COOKIES
	ch = head;
	while ((cookie = parse_header(ch, "Set-Cookie", &ch))) {
		set_cookie(uri, cookie);
		mem_free(cookie);
	}
#endif
	info->http_code = h;

	if (h == 100) {
		mem_free(head);
		state = S_PROC;
		kill_buffer_data(rb, a);
		goto again;
	}
	if (h < 200) {
		mem_free(head);
		abort_conn_with_state(conn, S_HTTP_ERROR);
		return;
	}
	if (h == 304) {
		mem_free(head);
		http_end_request(conn, S_OK, 1);
		return;
	}
	if (h == 204) {
		mem_free(head);
		http_end_request(conn, S_OK, 0);
		return;
	}
	if (h == 200 && connection_is_https_proxy(conn) && !conn->socket.ssl) {
#ifdef CONFIG_SSL
		mem_free(head);
		conn->conn_info = init_connection_info(uri, &conn->socket,
						       http_send_header);
		if (!conn->conn_info) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		if (ssl_connect(conn, &conn->socket) == -1) return;
#else
		abort_conn_with_state(conn, S_SSL_ERROR);
#endif
		return;
	}

	conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		mem_free(head);
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}
	mem_free_set(&conn->cached->head, head);

	if (!get_opt_bool("document.cache.ignore_cache_control")) {
		if ((d = parse_header(conn->cached->head, "Cache-Control", NULL))
		    || (d = parse_header(conn->cached->head, "Pragma", NULL)))
		{
			if (strstr(d, "no-cache"))
				conn->cached->cache_mode = CACHE_MODE_NEVER;
			mem_free(d);
		}
	}

#ifdef CONFIG_SSL
	/* TODO: Move this to some more generic place like lowlevel/connect.c
	 * or sched/connection.c when other protocols will need it. --jonas */
	if (conn->socket.ssl)
		mem_free_set(&conn->cached->ssl_info, get_ssl_connection_cipher(conn));
#endif

	/* XXX: Is there some reason why NOT to follow the Location header
	 * for any status? If the server didn't mean it, it wouldn't send
	 * it, after all...? --pasky */
	if (h == 201 || h == 301 || h == 302 || h == 303 || h == 307) {
		d = parse_header(conn->cached->head, "Location", NULL);
		if (d) {
			int use_get_method = (h == 303);

			/* A note from RFC 2616 section 10.3.3:
			 * RFC 1945 and RFC 2068 specify that the client is not
			 * allowed to change the method on the redirected
			 * request. However, most existing user agent
			 * implementations treat 302 as if it were a 303
			 * response, performing a GET on the Location
			 * field-value regardless of the original request
			 * method. */
			/* So POST must not be redirected to GET, but some
			 * BUGGY message boards rely on it :-( */
	    		if (h == 302
			    && get_opt_int("protocol.http.bugs.broken_302_redirect"))
				use_get_method = 1;

			redirect_cache(conn->cached, d, use_get_method, -1);
			mem_free(d);
		}
	}

	if (h == 401) {
		d = parse_header(conn->cached->head, "WWW-Authenticate", NULL);
		if (d) {
			if (!strncasecmp(d, "Basic", 5)) {
				unsigned char *realm = get_header_param(d, "realm");

				if (realm) {
					add_auth_entry(uri, realm);
					mem_free(realm);
				}
			}
			mem_free(d);
		}
	}

	kill_buffer_data(rb, a);
	info->close = 0;
	info->length = -1;
	info->recv_version = version;

	if ((d = parse_header(conn->cached->head, "Connection", NULL))
	     || (d = parse_header(conn->cached->head, "Proxy-Connection", NULL))) {
		if (!strcasecmp(d, "close")) info->close = 1;
		mem_free(d);
	} else if (PRE_HTTP_1_1(version)) {
		info->close = 1;
	}

	cf = conn->from;
	conn->from = 0;
	d = parse_header(conn->cached->head, "Content-Range", NULL);
	if (d) {
		if (strlen(d) > 6) {
			d[5] = 0;
			if (isdigit(d[6]) && !strcasecmp(d, "bytes")) {
				int f;

				errno = 0;
				f = strtol(d + 6, NULL, 10);

				if (!errno && f >= 0) conn->from = f;
			}
		}
		mem_free(d);
	}
	if (cf && !conn->from && !conn->unrestartable) conn->unrestartable = 1;
	if ((conn->progress.start <= 0 && conn->from > cf) || conn->from < 0) {
		/* We don't want this if conn->progress.start because then conn->from will
		 * be probably value of conn->progress.start, while cf is 0. */
		abort_conn_with_state(conn, S_HTTP_ERROR);
		return;
	}

#if 0
	{
		struct status *s;
		foreach (s, conn->downloads) {
			fprintf(stderr, "conn %p status %p pri %d st %d er %d :: ce %s",
				conn, s, s->pri, s->state, s->prev_error,
				s->cached ? s->cached->url : (unsigned char *) "N-U-L-L");
		}
	}
#endif

	if (conn->progress.start >= 0) {
		/* Update to the real value which we've got from Content-Range. */
		conn->progress.seek = conn->from;
	}
	conn->progress.start = conn->from;

	d = parse_header(conn->cached->head, "Content-Length", NULL);
	if (d) {
		unsigned char *ep;
		int l;

		errno = 0;
		l = strtol(d, (char **) &ep, 10);

		if (!errno && !*ep && l >= 0) {
			if (!info->close || POST_HTTP_1_0(version))
				info->length = l;
			conn->est_length = conn->from + l;
		}
		mem_free(d);
	}

	if (!conn->unrestartable) {
		d = parse_header(conn->cached->head, "Accept-Ranges", NULL);

		if (d) {
			if (!strcasecmp(d, "none"))
				conn->unrestartable = 1;
			mem_free(d);
		} else {
			if (!conn->from)
				conn->unrestartable = 1;
		}
	}

	d = parse_header(conn->cached->head, "Transfer-Encoding", NULL);
	if (d) {
		if (!strcasecmp(d, "chunked")) {
			info->length = LEN_CHUNKED;
			info->chunk_remaining = CHUNK_SIZE;
		}
		mem_free(d);
	}
	if (!info->close && info->length == -1) info->close = 1;

	d = parse_header(conn->cached->head, "Last-Modified", NULL);
	if (d) {
		if (conn->cached->last_modified && strcasecmp(conn->cached->last_modified, d)) {
			delete_entry_content(conn->cached);
			if (conn->from) {
				conn->from = 0;
				mem_free(d);
				retry_conn_with_state(conn, S_MODIFIED);
				return;
			}
		}
		if (!conn->cached->last_modified) conn->cached->last_modified = d;
		else mem_free(d);
	}
	if (!conn->cached->last_modified) {
		d = parse_header(conn->cached->head, "Date", NULL);
		if (d) conn->cached->last_modified = d;
	}

	/* FIXME: Parse only if HTTP/1.1 or later? --Zas */
	d = parse_header(conn->cached->head, "ETag", NULL);
	if (d) {
		if (conn->cached->etag)  {
			unsigned char *old_tag = conn->cached->etag;
			unsigned char *new_tag = d;

			/* http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.19 */

			if (new_tag[0] == 'W' && new_tag[1] == '/')
				new_tag += 2;

			if (old_tag[0] == 'W' && old_tag[1] == '/')
				old_tag += 2;

			if (strcmp(new_tag, old_tag)) {
				delete_entry_content(conn->cached);
				if (conn->from) {
					conn->from = 0;
					mem_free(d);
					retry_conn_with_state(conn, S_MODIFIED);
					return;
				}
			}
		}

		if (!conn->cached->etag)
			conn->cached->etag = d;
		else
			mem_free(d);
	}

	d = parse_header(conn->cached->head, "Content-Encoding", NULL);
	if (d) {
		unsigned char *extension = get_extension_from_uri(uri);
		enum stream_encoding file_encoding;

		file_encoding = extension ? guess_encoding(extension) : ENCODING_NONE;
		mem_free_if(extension);

		/* If the content is encoded, we want to preserve the encoding
		 * if it is implied by the extension, so that saving the URI
		 * will leave the saved file with the correct encoding. */
#ifdef HAVE_ZLIB_H
		if (file_encoding != ENCODING_GZIP
		    && (!strcasecmp(d, "gzip") || !strcasecmp(d, "x-gzip")))
		    	conn->content_encoding = ENCODING_GZIP;
#endif
#ifndef BUG_517
#ifdef HAVE_BZLIB_H
		if (file_encoding != ENCODING_BZIP2
		    && (!strcasecmp(d, "bzip2") || !strcasecmp(d, "x-bzip2")))
			conn->content_encoding = ENCODING_BZIP2;
#endif
#endif
		mem_free(d);
	}

	if (conn->content_encoding != ENCODING_NONE) {
		mem_free_if(conn->cached->encoding_info);
		conn->cached->encoding_info = stracpy(get_encoding_name(conn->content_encoding));
	}

	if (info->length == -1
	    || (PRE_HTTP_1_1(info->recv_version) && info->close))
		rb->close = 1;

	read_http_data(conn, rb);
}

static void
http_get_header(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);

	if (!rb) return;
	set_connection_timeout(conn);
	rb->close = 1;
	read_from_socket(conn, &conn->socket, rb, http_got_header);
}
