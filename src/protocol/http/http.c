/* Internal "http" protocol implementation */
/* $Id: http.c,v 1.22 2002/06/09 16:09:55 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "dialogs/menu.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "document/cache.h"
#include "document/session.h"
#include "intl/charsets.h"
#include "lowlevel/connect.h"
#include "lowlevel/sched.h"
#include "lowlevel/sysname.h"
#include "lowlevel/terminal.h"
#include "protocol/http/auth.h"
#include "protocol/http/header.h"
#include "protocol/http/http.h"
#include "protocol/url.h"
#include "util/base64.h"
#include "util/blacklist.h"

struct http_connection_info {
	enum blacklist_flags bl_flags;
	int http10;
	int close;
	int length;
	int version;
	int chunk_remaining;
};

static int get_http_code(unsigned char *head, int *code, int *version)
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

unsigned char *buggy_servers[] = {
	"mod_czech/3.1.0",
	"Purveyor",
	"Netscape-Enterprise",
	NULL
};

int check_http_server_bugs(unsigned char *url,
			   struct http_connection_info *info,
			   unsigned char *head)
{
	unsigned char *server, **s;

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

void http_end_request(struct connection *c)
{
	if (c->state == S_OK) {
		if (c->cache) {
			truncate_entry(c->cache, c->from, 1);
			c->cache->incomplete = 0;
#ifdef HAVE_SCRIPTING
			c->cache->done_pre_format_html_hook = 0;
#endif
		}
	}
	if (c->info && !((struct http_connection_info *)c->info)->close
#ifdef HAVE_SSL
	&& (!c->ssl) /* We won't keep alive ssl connections */
#endif
	&& (!get_opt_int("protocol.http.bugs.post_no_keepalive")
	    || !strchr(c->url, POST_CHAR))) {
		add_keepalive_socket(c, HTTP_KEEPALIVE_TIMEOUT);
	} else {
		abort_connection(c);
	}
}

void http_send_header(struct connection *);

void http_func(struct connection *c)
{
	/* setcstate(c, S_CONN); */
	set_timeout(c);
	
	if (get_keepalive_socket(c)) {
		int p = get_port(c->url);
		
		if (p == -1) {
			setcstate(c, S_INTERNAL);
			abort_connection(c);
			return;
		}
		
		make_connection(c, p, &c->sock1, http_send_header);
	} else {
		http_send_header(c);
	}
}

void proxy_func(struct connection *c)
{
	http_func(c);
}

void http_get_header(struct connection *);

void http_send_header(struct connection *c)
{
	static unsigned char *accept_charset = NULL;
	unsigned char *host = upcase(c->url[0]) != 'P' ? c->url
						       : get_url_data(c->url);
	struct http_connection_info *info;
	int http10 = get_opt_int("protocol.http.bugs.http10");
	unsigned char *post;

	struct cache_entry *e = NULL;
	unsigned char *hdr;
	unsigned char *host_data, *url_data;
	int l = 0;

	set_timeout(c);

	info = mem_alloc(sizeof(struct http_connection_info));
	if (!info) {
		setcstate(c, S_OUT_OF_MEM);
		abort_connection(c);
		return;
	}
	memset(info, 0, sizeof(struct http_connection_info));
	c->info = info;

	if ((host_data = get_host_name(host))) {
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
		setcstate(c, S_OUT_OF_MEM);
		http_end_request(c);
		return;
	}

	if (!post) {
		add_to_str(&hdr, &l, "GET ");
	} else {
		add_to_str(&hdr, &l, "POST ");
		c->unrestartable = 2;
	}

	if (upcase(c->url[0]) != 'P') {
		add_to_str(&hdr, &l, "/");
	}

	url_data = get_url_data(c->url);
	if (!url_data) {
		setcstate(c, S_BAD_URL);
		http_end_request(c);
		return;
	}

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
		
#if 0
		/* XXX: This is pretty ugly and ineffective (read as slow). */
 		while (strchr(eurl, ' ')) {
			unsigned char *space = strchr(eurl, ' ');
			unsigned char *neurl = mem_alloc(strlen(eurl) + 3);
			
			if (!neurl) break;
			memcpy(neurl, eurl, space - eurl);
			neurl[space - eurl] = 0;
			strcat(neurl, "%20");
			strcat(neurl, space + 1);
			mem_free(eurl);
			eurl = neurl;
		}
		
		add_to_str(&hdr, &l, eurl);
		mem_free(eurl);
#endif

		p = eurl;
		while ((p1 = strchr(p, ' '))) {
			*p1 = 0;
			add_to_str(&hdr, &l, p);
			add_to_str(&hdr, &l, "%20");
			/* This is probably not needed, but who cares.. ;) */
			*p1 = ' ';
			p = p1 + 1;
		}
		add_to_str(&hdr, &l, p);
		mem_free(eurl);
	}

	if (http10) {
		add_to_str(&hdr, &l, " HTTP/1.0\r\n");
	} else {
		add_to_str(&hdr, &l, " HTTP/1.1\r\n");
	}

	if ((host_data = get_host_name(host))) {
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

		if ((host_data = get_port_str(host))) {
			add_to_str(&hdr, &l, ":");
			add_to_str(&hdr, &l, host_data);
			mem_free(host_data);
		}

		add_to_str(&hdr, &l, "\r\n");
	}

	if (get_opt_str("protocol.http.proxy.user")[0]) {
		unsigned char *proxy_data;

		proxy_data = straconcat(get_opt_str("protocol.http.proxy.user"),
				        ":",
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
	
	if (!get_opt_str("protocol.http.user_agent")[0]) {
                add_to_str(&hdr, &l,
			   "User-Agent: ELinks (" VERSION_STRING "; ");
                add_to_str(&hdr, &l, system_name);

		if (!list_empty(terminals)) {
			struct terminal *term = terminals.prev;

			add_to_str(&hdr, &l, "; ");
			add_num_to_str(&hdr, &l, term->x);
			add_to_str(&hdr, &l, "x");
			add_num_to_str(&hdr, &l, term->y);
		}
                add_to_str(&hdr, &l, ")\r\n");

        } else {
                add_to_str(&hdr, &l, "User-Agent: ");
                add_to_str(&hdr, &l, get_opt_str("protocol.http.user_agent"));
                add_to_str(&hdr, &l, "\r\n");
        }

	switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			break;

		case REFERER_FAKE:
			add_to_str(&hdr, &l, "Referer: ");
			add_to_str(&hdr, &l, get_opt_str("protocol.http.referer.fake"));
			add_to_str(&hdr, &l, "\r\n");
			break;

		case REFERER_TRUE:
			if (c->prev_url && c->prev_url[0]) {
				unsigned char *prev_post;

				add_to_str(&hdr, &l, "Referer: ");

				prev_post = strchr(c->prev_url, POST_CHAR);

				if (prev_post) {
					add_bytes_to_str(&hdr, &l, c->prev_url,
							 prev_post - c->prev_url);
				} else {
					add_to_str(&hdr, &l, c->prev_url);
				}

				add_to_str(&hdr, &l, "\r\n");
			}
			break;

		case REFERER_SAME_URL:
			add_to_str(&hdr, &l, "Referer: http://");
			if ((host_data = get_host_name(host))) {
				add_to_str(&hdr, &l, host_data);
				mem_free(host_data);

				if ((host_data = get_port_str(host))) {
					add_to_str(&hdr, &l, ":");
					add_to_str(&hdr, &l, host_data);
					mem_free(host_data);
				}
			}

			if (upcase(c->url[0]) != 'P') {
				add_to_str(&hdr, &l, "/");
			}

			if (!post) {
				add_to_str(&hdr, &l, url_data);
			} else {
				add_bytes_to_str(&hdr, &l, url_data,
						 post - url_data - 1);
			}

			add_to_str(&hdr, &l, "\r\n");
			break;
	}

	add_to_str(&hdr, &l, "Accept: */*\r\n");

	if (!accept_charset) {
		unsigned char *cs, *ac;
		int aclen = 0;
		int i;

		ac = init_str();
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

		accept_charset = malloc(strlen(ac) + 1);
		if (accept_charset) {
			strcpy(accept_charset, ac);
		} else {
			accept_charset = "";
		}

		mem_free(ac);
	}

	if (!(info->bl_flags & BL_NO_CHARSET)) {
		add_to_str(&hdr, &l, accept_charset);
	}

	if (get_opt_str("protocol.http.accept_language")[0]) {
		add_to_str(&hdr, &l, "Accept-Language: ");
		add_to_str(&hdr, &l, get_opt_str("protocol.http.accept_language"));
		add_to_str(&hdr, &l, "\r\n");
	}

	if (!http10) {
		if (upcase(c->url[0]) != 'P') {
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

	if ((e = c->cache)) {
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

	if (c->from) {
		add_to_str(&hdr, &l, "Range: bytes=");
		add_num_to_str(&hdr, &l, c->from);
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

	send_cookies(&hdr, &l, host);
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

	write_to_socket(c, c->sock1, hdr, strlen(hdr), http_get_header);

	mem_free(hdr);
	setcstate(c, S_SENT);
}

int is_line_in_buffer(struct read_buffer *rb)
{
	int l;
	for (l = 0; l < rb->len; l++) {
		if (rb->data[l] == 10) return l + 1;
		if (l < rb->len - 1 && rb->data[l] == 13 && rb->data[l + 1] == 10) return l + 2;
		if (l == rb->len - 1 && rb->data[l] == 13) return 0;
		if (rb->data[l] < ' ') return -1;
	}
	return 0;
}

void read_http_data(struct connection *c, struct read_buffer *rb)
{
	struct http_connection_info *info = c->info;
	set_timeout(c);
	if (rb->close == 2) {
		setcstate(c, S_OK);
		http_end_request(c);
		return;
	}
	if (info->length != -2) {
		int l = rb->len;
		if (info->length >= 0 && info->length < l) l = info->length;
		c->received += l;
		if (add_fragment(c->cache, c->from, rb->data, l) == 1) c->tries = 0;
		if (info->length >= 0) info->length -= l;
		c->from += l;
		kill_buffer_data(rb, l);
		if (!info->length && !rb->close) {
			setcstate(c, S_OK);
			http_end_request(c);
			return;
		}
	} else {
		next_chunk:
		if (info->chunk_remaining == -2) {
			int l;
			if ((l = is_line_in_buffer(rb))) {
				if (l == -1) {
					setcstate(c, S_HTTP_ERROR);
					abort_connection(c);
					return;
				}
				kill_buffer_data(rb, l);
				if (l <= 2) {
					setcstate(c, S_OK);
					http_end_request(c);
					return;
				}
				goto next_chunk;
			}
		} else if (info->chunk_remaining == -1) {
			int l;
			if ((l = is_line_in_buffer(rb))) {
				unsigned char *de;
				int n = 0;
				if (l != -1) n = strtol(rb->data, (char **)&de, 16);
				if (l == -1 || de == rb->data) {
					setcstate(c, S_HTTP_ERROR);
					abort_connection(c);
					return;
				}
				kill_buffer_data(rb, l);
				if (!(info->chunk_remaining = n)) info->chunk_remaining = -2;
				goto next_chunk;
			}
		} else {
			int l = info->chunk_remaining;
			if (l > rb->len) l = rb->len;
			c->received += l;
			if (add_fragment(c->cache, c->from, rb->data, l) == 1) c->tries = 0;
			info->chunk_remaining -= l;
			c->from += l;
			kill_buffer_data(rb, l);
			if (!info->chunk_remaining && rb->len >= 1) {
				if (rb->data[0] == 10) kill_buffer_data(rb, 1);
				else {
					if (rb->data[0] != 13 || (rb->len >= 2 && rb->data[1] != 10)) {
						setcstate(c, S_HTTP_ERROR);
						abort_connection(c);
						return;
					}
					if (rb->len < 2) goto read_more;
					kill_buffer_data(rb, 2);
				}
				info->chunk_remaining = -1;
				goto next_chunk;
			}
		}

	}
	read_more:
	read_from_socket(c, c->sock1, rb, read_http_data);
	setcstate(c, S_TRANS);
}

int get_header(struct read_buffer *rb)
{
	int i;
	for (i = 0; i < rb->len; i++) {
		unsigned char a = rb->data[i];
		if (/*a < ' ' && a != 10 && a != 13*/!a) return -1;
		if (i < rb->len - 1 && a == 10 && rb->data[i + 1] == 10) return i + 2;
		if (i < rb->len - 3 && a == 13) {
			if (rb->data[i + 1] != 10) return -1;
			if (rb->data[i + 2] == 13) {
				if (rb->data[i + 3] != 10) return -1;
				return i + 4;
			}
		}
	}
	return 0;
}

void http_got_header(struct connection *c, struct read_buffer *rb)
{
	int cf;
	int state = c->state != S_PROC ? S_GETH : S_PROC;
	unsigned char *head;
	unsigned char *cookie, *ch;
	int a, h, version;
	unsigned char *d;
	struct cache_entry *e;
	struct http_connection_info *info;
	unsigned char *host = upcase(c->url[0]) != 'P' ? c->url : get_url_data(c->url);

	set_timeout(c);
	info = c->info;
	if (rb->close == 2) {
		unsigned char *h;
		if (!c->tries && (h = get_host_name(host))) {
			if (info->bl_flags & BL_NO_CHARSET) {
				del_blacklist_entry(h, BL_NO_CHARSET);
			} else {
				add_blacklist_entry(h, BL_NO_CHARSET);
				c->tries = -1;
			}
			mem_free(h);
		}
		setcstate(c, S_CANT_READ);
		retry_connection(c);
		return;
	}
	rb->close = 0;
	again:
	if ((a = get_header(rb)) == -1) {
		setcstate(c, S_HTTP_ERROR);
		abort_connection(c);
		return;
	}
	if (!a) {
		read_from_socket(c, c->sock1, rb, http_got_header);
		setcstate(c, state);
		return;
	}
	if (get_http_code(rb->data, &h, &version) || h == 101) {
		setcstate(c, S_HTTP_ERROR);
		abort_connection(c);
		return;
	}
	if (!(head = mem_alloc(a + 1))) {
		setcstate(c, S_OUT_OF_MEM);
		abort_connection(c);
		return;
	}
	memcpy(head, rb->data, a); head[a] = 0;
	if (check_http_server_bugs(host, c->info, head)) {
		mem_free(head);
		setcstate(c, S_RESTART);
		retry_connection(c);
		return;
	}
	ch = head;
	while ((cookie = parse_http_header(ch, "Set-Cookie", &ch))) {
		unsigned char *host = upcase(c->url[0]) != 'P' ? c->url : get_url_data(c->url);
		set_cookie(NULL, host, cookie);
		mem_free(cookie);
	}
	if (h == 100) {
		mem_free(head);
		state = S_PROC;
		kill_buffer_data(rb, a);
		goto again;
	}
	if (h < 200) {
		mem_free(head);
		setcstate(c, S_HTTP_ERROR);
		abort_connection(c);
		return;
	}
	if (h == 304) {
		mem_free(head);
		setcstate(c, S_OK);
		http_end_request(c);
		return;
	}
	if (get_cache_entry(c->url, &e)) {
		mem_free(head);
		setcstate(c, S_OUT_OF_MEM);
		abort_connection(c);
		return;
	}
	if (e->head) mem_free(e->head);
	e->head = head;
#ifdef HAVE_SSL
	if (c->ssl) {
		int l = 0;
		if (e->ssl_info) mem_free(e->ssl_info);
		e->ssl_info = init_str();
		add_num_to_str(&e->ssl_info, &l, SSL_get_cipher_bits(c->ssl, NULL));
		add_to_str(&e->ssl_info, &l, "-bit ");
		add_to_str(&e->ssl_info, &l, SSL_get_cipher_version(c->ssl));
		add_to_str(&e->ssl_info, &l, " ");
		add_to_str(&e->ssl_info, &l, (unsigned  char *)SSL_get_cipher_name(c->ssl));
	}
#endif
	if (h == 204) {
		setcstate(c, S_OK);
		http_end_request(c);
		return;
	}
	if (h == 301 || h == 302 || h == 303) {
		if ((d = parse_http_header(e->head, "Location", NULL))) {
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
	if ((d = parse_http_header(e->head, "Connection", NULL)) || (d = parse_http_header(e->head, "Proxy-Connection", NULL))) {
		if (!strcasecmp(d, "close")) info->close = 1;
		mem_free(d);
	} else if (version < 11) info->close = 1;
	cf = c->from;
	c->from = 0;
	if ((d = parse_http_header(e->head, "Content-Range", NULL))) {
		if (strlen(d) > 6) {
			d[5] = 0;
			if (!(strcasecmp(d, "bytes")) && d[6] >= '0' && d[6] <= '9') {
				int f = strtol(d + 6, NULL, 10);
				if (f >= 0) c->from = f;
			}
		}
		mem_free(d);
	}
	if (cf && !c->from && !c->unrestartable) c->unrestartable = 1;
	if (c->from > cf || c->from < 0) {
		setcstate(c, S_HTTP_ERROR);
		abort_connection(c);
		return;
	}
	if ((d = parse_http_header(e->head, "Content-Length", NULL))) {
		unsigned char *ep;
		int l = strtol(d, (char **)&ep, 10);
		if (!*ep && l >= 0) {
			if (!info->close || version >= 11) info->length = l;
			c->est_length = c->from + l;
		}
		mem_free(d);
	}
	if ((d = parse_http_header(e->head, "Accept-Ranges", NULL))) {
		if (!strcasecmp(d, "none") && !c->unrestartable)
			c->unrestartable = 1;
		mem_free(d);
	} else if (!c->unrestartable && !c->from) c->unrestartable = 1;
	if ((d = parse_http_header(e->head, "Transfer-Encoding", NULL))) {
		if (!strcasecmp(d, "chunked")) {
			info->length = -2;
			info->chunk_remaining = -1;
		}
		mem_free(d);
	}
	if (!info->close && info->length == -1) info->close = 1;
	if ((d = parse_http_header(e->head, "Last-Modified", NULL))) {
		if (e->last_modified && strcasecmp(e->last_modified, d)) {
			delete_entry_content(e);
			if (c->from) {
				c->from = 0;
				mem_free(d);
				setcstate(c, S_MODIFIED);
				retry_connection(c);
				return;
			}
		}
		if (!e->last_modified) e->last_modified = d;
		else mem_free(d);
	}
	if (!e->last_modified && (d = parse_http_header(e->head, "Date", NULL)))
		e->last_modified = d;
	if (info->length == -1 || (version < 11 && info->close)) rb->close = 1;
	read_http_data(c, rb);
}

void http_get_header(struct connection *c)
{
	struct read_buffer *rb;
	set_timeout(c);
	if (!(rb = alloc_read_buffer(c))) return;
	rb->close = 1;
	read_from_socket(c, c->sock1, rb, http_got_header);
}
