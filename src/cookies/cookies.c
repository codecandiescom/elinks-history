/* Internal cookies implementation */
/* $Id: cookies.c,v 1.62 2003/07/08 13:49:14 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "elinks.h"

/* #define COOKIES_DEBUG */

#include "cookies/cookies.h"
#include "cookies/parser.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "lowlevel/home.h"
#include "lowlevel/ttime.h"
#include "protocol/http/date.h"
#include "protocol/http/header.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#ifdef COOKIES_DEBUG
#include "util/error.h"
#endif
#include "util/file.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"

static int cookies_nosave = 0;

static unsigned int cookie_id = 0;

struct cookie {
	LIST_HEAD(struct cookie);

	unsigned char *name, *value;
	unsigned char *server;
	unsigned char *path, *domain;
	ttime expires; /* zero means undefined */
	int secure;
	int id;
};

static INIT_LIST_HEAD(cookies);

struct c_domain {
	LIST_HEAD(struct c_domain);

	unsigned char domain[1]; /* Must be at end of struct. */
};

static INIT_LIST_HEAD(c_domains);

struct c_server {
	LIST_HEAD(struct c_server);

	int accept;
	unsigned char server[1]; /* Must be at end of struct. */
};

static INIT_LIST_HEAD(c_servers);

static int cookies_dirty = 0;

#ifdef COOKIES

void load_cookies(void);

static void accept_cookie(struct cookie *);
static void save_cookies(void);


static void
free_cookie(struct cookie *c)
{
	if (c->name) mem_free(c->name);
	if (c->value) mem_free(c->value);
	if (c->server) mem_free(c->server);
	if (c->path) mem_free(c->path);
	if (c->domain) mem_free(c->domain);
}


static int
check_domain_security(unsigned char *domain, unsigned char *server, int server_len)
{
	int i, j, domain_len;
	int need_dots;

	if (domain[0] == '.') domain++;
	domain_len = strlen(domain);

	if (domain_len > server_len) return 0;

	/* Match domain and server.. */

	if (!strncasecmp(domain, server, server_len)) {
		/* We should probably allow domains which are same as servers.
		 * --<rono@sentuny.com.au> */
		/* Mozilla does it as well ;))) and I can't figure out any
		 * security risk. --pasky */
		return 0;
	}

	for (i = server_len - domain_len, j = 0; server[i]; i++, j++)
		if (upcase(server[i]) != upcase(domain[j]))
			return 0;

	/* Also test if domain is secure enough.. */

	need_dots = 1;

	if (get_opt_int("cookies.paranoid_security")) {
		/* This is somehow controversial attempt (by the way violating
		 * RFC) to increase cookies security in national domains, done
		 * by Mikulas. As it breaks a lot of sites, I decided to make
		 * this optional and off by default. I also don't think this
		 * improves security considerably, as it's SITE'S fault and
		 * also no other browser probably does it. --pasky */
		/* Mikulas' comment: Some countries have generic 2-nd level
		 * domains (like .com.pl, .co.uk ...) and it would be very bad
		 * if someone set cookies for these genegic domains.  Imagine
		 * for example that server http://brutalporn.com.pl sets cookie
		 * Set-Cookie: user_is=perverse_pig; domain=.com.pl -- then
		 * this cookies would be sent to all commercial servers in
		 * Poland. */
		need_dots = 2;
		if (domain_len > 4 && domain[domain_len - 4] == '.') {
			unsigned char *tld[] = { "com", "edu", "net", "org",
						 "gov", "mil", "int", NULL };

			for (i = 0; tld[i]; i++) {
				if (!strncasecmp(tld[i], &domain[domain_len - 3], 3)) {
					need_dots = 1;
					break;
				}
			}
		}
	}

	for (i = 0; domain[i]; i++)
		if (domain[i] == '.' && !--need_dots)
			break;

	if (need_dots > 0) return 0;
	return 1;
}

int
set_cookie(struct terminal *term, struct uri *uri, unsigned char *str)
{
	unsigned char *date, *secure;
	struct cookie *cookie;
	struct c_server *cs;
	struct cookie_str cstr;

	if (get_opt_int("cookies.accept_policy") == COOKIES_ACCEPT_NONE)
		return 0;

#ifdef COOKIES_DEBUG
	debug("set_cookie -> (%s) %s", uri->protocol, str);
#endif

	cstr.str = str;
	if (!parse_cookie_str(&cstr)) return 0;

	cookie = mem_alloc(sizeof(struct cookie));
	if (!cookie) return 0;

	/* Fill main fields */

	cookie->name = memacpy(str, cstr.nam_end - str);
	if (!cookie->name) {
free_cookie:
		mem_free(cookie);
		return 0;
	}
	cookie->value = memacpy(cstr.val_start, cstr.val_end - cstr.val_start);
	if (!cookie->value) {
free_cookie_name:
		mem_free(cookie->name);
		goto free_cookie;
	}
	cookie->server = memacpy(uri->host, uri->hostlen);
	if (!cookie->server) {
free_cookie_value:
		mem_free(cookie->value);
		goto free_cookie_name;
	}

	/* Get expiration date */

	date = parse_http_header_param(str, "expires");
	if (date) {
		cookie->expires = parse_http_date(date);
#if 0
		/* I decided not to expire such cookies (possibly ones with
		 * date we can't parse properly), but instead just keep their
		 * expire value at zero, thus making them expire on ELinks'
		 * quit. --pasky */
		if (!cookie->expires) {
			/* We use zero for cookies which expire with
			 * browser shutdown. */
			cookie->expires = (ttime) 1;
		}
#endif
		mem_free(date);

	} else {
		cookie->expires = (ttime) 0;
	}

	cookie->path = parse_http_header_param(str, "path");
	if (!cookie->path) {
		unsigned char *path_end;
		int len = 1;

		cookie->path = stracpy("/");
		if (!cookie->path) {
free_cookie_server:
			mem_free(cookie->server);
			goto free_cookie_value;
		}

		add_bytes_to_str(&cookie->path, &len, uri->data, uri->datalen);

		for (path_end = cookie->path; *path_end; path_end++) {
			if (end_of_dir(*path_end)) {
				*path_end = '\0';
				break;
			}
		}

		for (path_end = cookie->path + strlen(cookie->path) - 1;
		     path_end >= cookie->path; path_end--) {
			if (*path_end == '/') {
				path_end[1] = '\0';
				break;
			}
		}

	} else {
		if (!cookie->path[0]
		    || cookie->path[strlen(cookie->path) - 1] != '/')
			add_to_strn(&cookie->path, "/");

		if (cookie->path[0] != '/') {
			add_to_strn(&cookie->path, "x");
			memmove(cookie->path + 1, cookie->path,
				strlen(cookie->path) - 1);
			cookie->path[0] = '/';
		}
	}

	cookie->domain = parse_http_header_param(str, "domain");
	if (!cookie->domain) {
		cookie->domain = memacpy(uri->host, uri->hostlen);
		if (!cookie->domain) {
			mem_free(cookie->path);
			goto free_cookie_server;
		}
	}
	if (cookie->domain[0] == '.')
		memmove(cookie->domain, cookie->domain + 1, uri->hostlen);

	secure = parse_http_header_param(str, "secure");
	if (secure) {
		cookie->secure = 1;
		mem_free(secure);
	} else {
		cookie->secure = 0;
	}

#ifdef COOKIES_DEBUG
	{
		unsigned char *server = memacpy(uri->host, uri->hostlen);

		debug("Got cookie %s = %s from %s (%s), domain %s, "
		      "expires at %d, secure %d\n", cookie->name,
		      cookie->value, cookie->server, server, cookie->domain,
		      cookie->expires, cookie->secure);
		if (server) mem_free(server);
	}
#endif

	if (!check_domain_security(cookie->domain, uri->host, uri->hostlen)) {
#ifdef COOKIES_DEBUG
		debug("Domain security violated.");
#endif
		mem_free(cookie->domain);
		cookie->domain = memacpy(uri->host, uri->hostlen);
	}

	cookie->id = cookie_id++;

	foreach (cs, c_servers) {
		if (strncasecmp(cs->server, uri->host, uri->hostlen)) continue;

		if (cs->accept)	goto ok;

#ifdef COOKIES_DEBUG
		debug("Dropped.");
#endif
		free_cookie(cookie);
		mem_free(cookie);
		return 0;
	}

	if (get_opt_int("cookies.accept_policy") != COOKIES_ACCEPT_ALL) {
		/* TODO */
		free_cookie(cookie);
		mem_free(cookie);
		return 1;
	}

ok:
	cookies_dirty = 1;
	accept_cookie(cookie);

	return 0;
}


static void
accept_cookie(struct cookie *c)
{
	struct c_domain *cd;
	struct cookie *d, *e;

	foreach (d, cookies) {
		if (strcasecmp(d->name, c->name)
		    || strcasecmp(d->domain, c->domain))
			continue;
		e = d;
		d = d->prev;
		del_from_list(e);
		free_cookie(e);
		mem_free(e);
	}

	add_to_list(cookies, c);

	foreach (cd, c_domains)
		if (!strcasecmp(cd->domain, c->domain))
			return;

	cd = mem_alloc(sizeof(struct c_domain) + strlen(c->domain) + 1);
	if (!cd) return;

	strcpy(cd->domain, c->domain);
	add_to_list(c_domains, cd);

	if (get_opt_int("cookies.save") && get_opt_int("cookies.resave"))
		save_cookies();
}

#if 0
static void
delete_cookie(struct cookie *c)
{
	struct c_domain *cd;
	struct cookie *d;

	foreach (d, cookies)
		if (!strcasecmp(d->domain, c->domain))
			goto end;

	foreach (cd, c_domains) {
	       	if (!strcasecmp(cd->domain, c->domain)) {
			del_from_list(cd);
			mem_free(cd);
			break;
		}
	}

end:
	del_from_list(c);
	free_cookie(c);
	mem_free(c);

	if (get_opt_int("cookies.save") && get_opt_int("cookies.resave"))
		save_cookies();
}


static struct
cookie *find_cookie_id(void *idp)
{
	int id = (int)idp;
	struct cookie *c;

	foreach (c, cookies)
		if (c->id == id)
			return c;

	return NULL;
}


static void
reject_cookie(void *idp)
{
	struct cookie *c = find_cookie_id(idp);

	if (!c)	return;

	delete_cookie(c);
}


static void
cookie_default(void *idp, int a)
{
	struct c_server *s;
	struct cookie *c = find_cookie_id(idp);

	if (!c) return;

	foreach (s, c_servers)
		if (!strcasecmp(s->server, c->server))
			goto found;

	s = mem_alloc(sizeof(struct c_server) + strlen(c->server) + 1);
	if (s) {
		strcpy(s->server, c->server);
		add_to_list(c_servers, s);
found:
		s->accept = a;
	}
}


static void
accept_cookie_always(void *idp)
{
	cookie_default(idp, 1);
}


static void
accept_cookie_never(void *idp)
{
	cookie_default(idp, 0);
	reject_cookie(idp);
}
#endif


static int
is_in_domain(unsigned char *d, unsigned char *s, int sl)
{
	int dl = strlen(d);

	if (dl > sl) return 0;
	if (dl == sl) return !strncasecmp(d, s, sl);
	if (s[sl - dl - 1] != '.') return 0;

	return !strncasecmp(d, s + sl - dl, dl);
}


static inline int
is_path_prefix(unsigned char *d, unsigned char *s, int sl)
{
	int dl = strlen(d);

	if (dl > sl) return 0;

	return !memcmp(d, s, dl);
}


static inline int
cookie_expired(struct cookie *c)
{
  	return (c->expires && c->expires < time(NULL));
}


void
send_cookies(unsigned char **s, int *l, struct uri *uri)
{
	int nc = 0;
	struct c_domain *cd;
	struct cookie *c, *d;
	unsigned char *data = NULL;
	int datalen = uri->datalen + 1;

	if (!uri->host || !uri->data) return;

	foreach (cd, c_domains)
		if (is_in_domain(cd->domain, uri->host, uri->hostlen)) {
			data = uri->data - 1;
			break;
		}

	if (!data) return;

	foreach (c, cookies) {
		if (!is_in_domain(c->domain, uri->host, uri->hostlen)
		    || !is_path_prefix(c->path, data, datalen))
			continue;

		if (cookie_expired(c)) {
#ifdef COOKIES_DEBUG
			debug("Cookie %s=%s (exp %d) expired.\n",
			      c->name, c->value, c->expires);
#endif
			d = c;
			c = c->prev;
			del_from_list(d);
			free_cookie(d);
			mem_free(d);

			cookies_dirty = 1;
			continue;
		}

		/* Not sure if this is 100% right..? --pasky */
		if (c->secure && strncmp("https://", uri->protocol, 8))
			continue;

		if (!nc) {
			add_to_str(s, l, "Cookie: ");
			nc = 1;
		} else {
			add_to_str(s, l, "; ");
		}

		add_to_str(s, l, c->name);
		add_chr_to_str(s, l, '=');
		add_to_str(s, l, c->value);
#ifdef COOKIES_DEBUG
		debug("Cookie: %s=%s", c->name, c->value);
#endif
	}

	if (nc)
		add_to_str(s, l, "\r\n");

	if (cookies_dirty && get_opt_int("cookies.save") && get_opt_int("cookies.resave"))
		save_cookies();
}


void
load_cookies(void) {
	/* Buffer size is set to be enough to read long lines that
	 * save_cookies may write. 6 is choosen after the fprintf(..) call
	 * in save_cookies(). --Zas */
	unsigned char in_buffer[6 * MAX_STR_LEN];
	unsigned char *cookfile = "cookies";
	unsigned char *p, *q;
	FILE *fp;

	if (elinks_home) {
		cookfile = straconcat(elinks_home, cookfile, NULL);
		if (!cookfile) return;
	}

	/* Do it here, as we will delete whole cookies list if the file was
	 * removed */
	cookies_nosave = 1;
	cleanup_cookies();
	cookies_nosave = 0;

	fp = fopen(cookfile, "r");
	if (elinks_home) mem_free(cookfile);
	if (!fp) return;

	while (safe_fgets(in_buffer, 6 * MAX_STR_LEN, fp)) {
		struct cookie *cookie = mem_calloc(1, sizeof(struct cookie));

		if (!cookie) return;

		/* Drop ending '\n'. */
		if (*in_buffer) in_buffer[strlen(in_buffer) - 1] = 0;

		q = in_buffer;
		p = strchr(in_buffer, '\t');
		if (!p)	goto inv;
		*p++ = '\0';
		cookie->name = stracpy(q); /* FIXME: untested return value. */

		q = p;
		p = strchr(p, '\t');
		if (!p) goto inv;
		*p++ = '\0';
		cookie->value = stracpy(q); /* FIXME: untested return value. */

		q = p;
		p = strchr(p, '\t');
		if (!p) goto inv;
		*p++ = '\0';
		cookie->server = stracpy(q); /* FIXME: untested return value. */

		q = p;
		p = strchr(p, '\t');
		if (!p) goto inv;
		*p++ = '\0';
		cookie->path = stracpy(q); /* FIXME: untested return value. */

		q = p;
		p = strchr(p, '\t');
		if (!p) goto inv;
		*p++ = '\0';
		cookie->domain = stracpy(q); /* FIXME: untested return value. */

		q = p;
		p = strchr(p, '\t');
		if (!p) goto inv;
		*p++ = '\0';
		cookie->expires = atol(q);

		cookie->secure = atoi(p);

		cookie->id = cookie_id++;

		/* XXX: We don't want to overwrite the cookies file
		 * periodically to our death. */
		cookies_nosave = 1;
		accept_cookie(cookie);
		cookies_nosave = 0;

		continue;

inv:
		free_cookie(cookie);
		mem_free(cookie);
	}

	fclose(fp);
}


static void
save_cookies(void) {
	struct cookie *c;
	unsigned char *cookfile;
	struct secure_save_info *ssi;

	if (cookies_nosave || !elinks_home || !cookies_dirty) return;

	cookfile = straconcat(elinks_home, "cookies", NULL);
	if (!cookfile) return;

	ssi = secure_open(cookfile, 0177); /* rw for user only */
	mem_free(cookfile);
	if (!ssi) return;

	foreach (c, cookies) {
		if (c->expires && !cookie_expired(c)) {
			if (secure_fprintf(ssi, "%s\t%s\t%s\t%s\t%s\t%ld\t%d\n",
					   c->name, c->value,
					   c->server ? c->server : (unsigned char *) "",
					   c->path ? c->path : (unsigned char *) "",
					   c->domain ? c->domain: (unsigned char *) "",
	   				   c->expires, c->secure) < 0) break;
		}
	}

	if (!secure_close(ssi)) cookies_dirty = 0;
}

void
init_cookies(void)
{
	if (get_opt_int("cookies.save"))
		load_cookies();
}


void
cleanup_cookies(void)
{
	free_list(c_domains);

	if (!cookies_nosave && get_opt_int("cookies.save"))
		save_cookies();

	while (!list_empty(cookies)) {
		struct cookie *cookie = cookies.next;

		del_from_list(cookie);
		free_cookie(cookie);
		mem_free(cookie);
	}
}

#endif /* COOKIES */
