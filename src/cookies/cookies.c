/* Internal cookies implementation */
/* $Id: cookies.c,v 1.8 2002/04/01 20:10:00 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <links.h>

/* #define COOKIES_DEBUG */

#include <cookies/cookies.h>
#include <config/default.h>
#include <document/session.h>
#include <lowlevel/terminal.h>
#include <protocol/http/date.h>
#include <protocol/http/header.h>
#include <protocol/url.h>
#ifdef COOKIES_DEBUG
#include <util/error.h>
#endif

tcount cookie_id = 0;

struct cookie {
	struct cookie *next;
	struct cookie *prev;
	unsigned char *name, *value;
	unsigned char *server;
	unsigned char *path, *domain;
	time_t expires; /* zero means undefined */
	int secure;
	int id;
};

struct list_head cookies = { &cookies, &cookies };

struct c_domain {
	struct c_domain *next;
	struct c_domain *prev;
	unsigned char domain[1];
};

struct list_head c_domains = { &c_domains, &c_domains };

struct c_server {
	struct c_server *next;
	struct c_server *prev;
	int accept;
	unsigned char server[1];
};

struct list_head c_servers = { &c_servers, &c_servers };

void accept_cookie(struct cookie *);
void delete_cookie(struct cookie *);

void load_cookies(), save_cookies();

void free_cookie(struct cookie *c)
{
	if (c->name) mem_free(c->name);
	if (c->value) mem_free(c->value);
	if (c->server) mem_free(c->server);
	if (c->path) mem_free(c->path);
	if (c->domain) mem_free(c->domain);
}

static int check_domain_security(unsigned char *server, unsigned char *domain)
{
	int i, j, domain_len;
	int need_dots;

	if (domain[0] == '.') domain++;
	domain_len = strlen(domain);

	if (domain_len > strlen(server))
		return 0;

	/* Match domain and server.. */

	if (!strcasecmp(domain, server)) {
		/* We should probably allow domains which are same as servers.
		 * --<rono@sentuny.com.au> */
		/* Mozilla does it as well ;))) and I can't figure out any
		 * security risk. --pasky */
		return 0;
	}

	for (i = strlen(server) - domain_len, j = 0; server[i]; i++, j++)
		if (upcase(server[i]) != upcase(domain[j]))
			return 0;

	/* Also test if domain is secure enough.. */

	need_dots = 1;

	if (cookies_paranoid_security) {
		/* This is somewhat controversial attempt (by the way violating
		 * RFC) to increase cookies security in national domains done
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
				if (!casecmp(tld[i], &domain[domain_len - 3], 3)) {
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

int set_cookie(struct terminal *term, unsigned char *url, unsigned char *str)
{
	struct cookie *cookie;
	struct c_server *cs;
	unsigned char *server, *document, *date, *secure;
	unsigned char *pos, *nam_end = NULL, *val_start = NULL, *val_end = NULL;
	int last_was_eq = 0, last_was_ws = 0;

	if (cookies_accept == COOKIES_ACCEPT_NONE)
		return 0;

#ifdef COOKIES_DEBUG
	debug("set_cookie -> (%s) %s", url, str);
#endif

	/* /NAME *= *VALUE *;/ */

	for (pos = str; *pos != ';' && *pos; pos++) {
		if (!last_was_ws)
			val_end = pos + 1; /* The NEXT char is ending it! */

		if (*pos == '=') {
			/* End of name reached */
			if (!nam_end) {
				nam_end = pos;
				/* This inside the if is protection against
				 * broken sites sending '=' inside values. */
				last_was_eq = 1;
			}

		} else if (WHITECHAR(*pos)) {
			if (!nam_end) {
				/* Just after name - end of name reached */
				nam_end = pos;
			}
			last_was_ws = 1;

		} else if (last_was_eq) {
			/* Start of value reached */
			val_start = pos;
			last_was_eq = 0;
			last_was_ws = 0;

		} else if (last_was_ws) {
			/* Non-whitespace after whitespace and not just after
			 * '=' - error */
			return 0;
		}
	}

	if (str == nam_end || !nam_end || !val_start || !val_end)
		return 0;

	cookie = mem_alloc(sizeof(struct cookie));
	if (!cookie)
		return 0;

	server = get_host_name(url);
	document = get_url_data(url);

	/* Fill main fields */

	cookie->name = memacpy(str, nam_end - str);
	cookie->value = memacpy(val_start, val_end - val_start);
	cookie->server = stracpy(server);

	/* Get expiration date */

	date = parse_http_header_param(str, "expires");
	if (date) {
		cookie->expires = parse_http_date(date);
		if (!cookie->expires) {
			/* We use zero for cookies which expire with
			 * browser shutdown. */
			cookie->expires++;
		}
		mem_free(date);

	} else {
		cookie->expires = 0;
	}

	cookie->path = parse_http_header_param(str, "path");
	if (!cookie->path) {
		unsigned char *path_end;

		cookie->path = stracpy("/");
		add_to_strn(&cookie->path, document);

		for (path_end = cookie->path; *path_end; path_end++) {
			if (end_of_dir(*path_end)) {
				*path_end = 0;
				break;
			}
		}

		for (path_end = cookie->path + strlen(cookie->path) - 1;
		     path_end >= cookie->path; path_end--) {
			if (*path_end == '/') {
				path_end[1] = 0;
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
	if (!cookie->domain)
		cookie->domain = stracpy(server);
	if (cookie->domain[0] == '.')
		memmove(cookie->domain, cookie->domain + 1,
			strlen(cookie->domain));

	secure = parse_http_header_param(str, "secure");
	if (secure) {
		cookie->secure = 1;
		mem_free(secure);
	} else {
		cookie->secure = 0;
	}

#ifdef COOKIES_DEBUG
	debug("Got cookie %s = %s from %s, domain %s, expires at %d, secure %d\n",
	      cookie->name, cookie->value, cookie->server, cookie->domain,
	      cookie->expires, cookie->secure);
#endif

	if (!check_domain_security(server, cookie->domain)) {
#ifdef COOKIES_DEBUG
		debug("Domain security violated.");
#endif
		mem_free(cookie->domain);
		cookie->domain = stracpy(server);
	}

	cookie->id = cookie_id++;

	foreach (cs, c_servers) {
		if (strcasecmp(cs->server, server))
			continue;

		if (cs->accept)
			goto ok;

#ifdef COOKIES_DEBUG
		debug("Dropped.");
#endif
		free_cookie(cookie);
		mem_free(cookie);
		mem_free(server);
		return 0;
	}

	if (cookies_accept != COOKIES_ACCEPT_ALL) {
		/* TODO */
		free_cookie(cookie);
		mem_free(cookie);
		mem_free(server);
		return 1;
	}

ok:
	accept_cookie(cookie);
	mem_free(server);
	return 0;
}

void accept_cookie(struct cookie *c)
{
	struct c_domain *cd;
	struct cookie *d, *e;
	foreach(d, cookies) if (!strcasecmp(d->name, c->name) && !strcasecmp(d->domain, c->domain)) {
		e = d;
		d = d->prev;
		del_from_list(e);
		free_cookie(e);
		mem_free(e);
	}
	add_to_list(cookies, c);
	foreach(cd, c_domains) if (!strcasecmp(cd->domain, c->domain)) return;
	if (!(cd = mem_alloc(sizeof(struct c_domain) + strlen(c->domain) + 1))) return;
	strcpy(cd->domain, c->domain);
	add_to_list(c_domains, cd);

	if (cookies_save && cookies_resave)
		save_cookies();
}

void delete_cookie(struct cookie *c)
{
	struct c_domain *cd;
	struct cookie *d;
	foreach(d, cookies) if (!strcasecmp(d->domain, c->domain)) goto x;
	foreach(cd, c_domains) if (!strcasecmp(cd->domain, c->domain)) {
		del_from_list(cd);
		mem_free(cd);
		break;
	}
	x:
	del_from_list(c);
	free_cookie(c);
	mem_free(c);

	if (cookies_save && cookies_resave)
		save_cookies();
}

struct cookie *find_cookie_id(void *idp)
{
	int id = (int)idp;
	struct cookie *c;
	foreach(c, cookies) if (c->id == id) return c;
	return NULL;
}

void reject_cookie(void *idp)
{
	struct cookie *c;
	if (!(c = find_cookie_id(idp))) return;
	delete_cookie(c);
}

void cookie_default(void *idp, int a)
{
	struct cookie *c;
	struct c_server *s;
	if (!(c = find_cookie_id(idp))) return;
	foreach(s, c_servers) if (!strcasecmp(s->server, c->server)) goto found;
	if ((s = mem_alloc(sizeof(struct c_server) + strlen(c->server) + 1))) {
		strcpy(s->server, c->server);
		add_to_list(c_servers, s);
		found:
		s->accept = a;
	}
}

void accept_cookie_always(void *idp)
{
	cookie_default(idp, 1);
}

void accept_cookie_never(void *idp)
{
	cookie_default(idp, 0);
	reject_cookie(idp);
}

int is_in_domain(unsigned char *d, unsigned char *s)
{
	int dl = strlen(d);
	int sl = strlen(s);
	if (dl > sl) return 0;
	if (dl == sl) return !strcasecmp(d, s);
	if (s[sl - dl - 1] != '.') return 0;
	return !casecmp(d, s + sl - dl, dl);
}

int is_path_prefix(unsigned char *d, unsigned char *s)
{
	int dl = strlen(d);
	int sl = strlen(s);
	if (dl > sl) return 0;
	return !memcmp(d, s, dl);
}

int cookie_expired(struct cookie *c)
{
  	return (c->expires && c->expires < time(NULL));
}

void send_cookies(unsigned char **s, int *l, unsigned char *url)
{
	int nc = 0;
	struct c_domain *cd;
	struct cookie *c, *d;
	unsigned char *server = get_host_name(url);
	unsigned char *data = get_url_data(url);
	if (data > url) data--;
	foreach (cd, c_domains) if (is_in_domain(cd->domain, server)) goto ok;
	mem_free(server);
	return;
	ok:
	foreach (c, cookies) if (is_in_domain(c->domain, server)) if (is_path_prefix(c->path, data)) {
		if (cookie_expired(c)) {
#ifdef COOKIES_DEBUG
			debug("Cookie %s=%s (exp %d) expired.\n", c->name, c->value, c->expires);
#endif
			d = c;
			c = c->prev;
			del_from_list(d);
			free_cookie(d);
			mem_free(d);

			if (cookies_save && cookies_resave)
				save_cookies();
			continue;
		}
		if (c->secure) continue;
		if (!nc) add_to_str(s, l, "Cookie: "), nc = 1;
		else add_to_str(s, l, "; ");
		add_to_str(s, l, c->name);
		add_to_str(s, l, "=");
		add_to_str(s, l, c->value);
	}
	if (nc) add_to_str(s, l, "\r\n");
	mem_free(server);
}

void load_cookies() {
	unsigned char in_buffer[MAX_STR_LEN];
	unsigned char *cookfile, *p, *q;
	FILE *fp;
	struct cookie *c;

	/* must be called after init_home */
	if (! links_home) return;

	cookfile = stracpy(links_home);
	if (! cookfile) return;
	add_to_strn(&cookfile, "cookies");

	/* do it here, as we will delete whole cookies list if the file was removed */
	free_list(c_domains);

	foreach(c, cookies)
		free_cookie(c);
	free_list(cookies);

	fp = fopen(cookfile, "r");
	mem_free(cookfile);
	if (fp == NULL) return;

	while (fgets(in_buffer, MAX_STR_LEN, fp)) {
		struct cookie *cookie;

		if (!(cookie = mem_alloc(sizeof(struct cookie)))) return;
		memset(cookie, 0, sizeof(struct cookie));

		q = in_buffer; p = strchr(in_buffer, ' ');
		if (p == NULL) goto inv;
		*p++ = '\0';
		cookie->name = stracpy(q);

		q = p; p = strchr(p, ' ');
		if (p == NULL) goto inv;
		*p++ = '\0';
		cookie->value = stracpy(q);

		q = p; p = strchr(p, ' ');
		if (p == NULL) goto inv;
		*p++ = '\0';
		cookie->server = stracpy(q);

		q = p; p = strchr(p, ' ');
		if (p == NULL) goto inv;
		*p++ = '\0';
		cookie->path = stracpy(q);

		q = p; p = strchr(p, ' ');
		if (p == NULL) goto inv;
		*p++ = '\0';
		cookie->domain = stracpy(q);

		q = p; p = strchr(p, ' ');
		if (p == NULL) goto inv;
		*p++ = '\0';
		cookie->expires = atol(q);

		cookie->secure = atoi(p);

		cookie->id = cookie_id++;

		{
			int cr = cookies_resave;

			/* XXX: We don't want to overwrite the cookies file
			 * periodically to our death. */
			cookies_resave = 0;
			accept_cookie(cookie);
			cookies_resave = cr;
		}

		continue;

inv:
		free_cookie(cookie);
		mem_free(cookie);
	}
	fclose(fp);
}

void save_cookies() {
	struct cookie *c;
	unsigned char *cookfile;
	FILE *fp;
	mode_t mask;

	cookfile = stracpy(links_home);
	if (! cookfile) return;
	add_to_strn(&cookfile, "cookies");

	mask = umask(066); /* 0600 permissions for cookies file */
	fp = fopen(cookfile, "w");
	umask(mask);
	mem_free(cookfile);
	if (fp == NULL) return;

	foreach (c, cookies) {
		if (c->expires && ! cookie_expired(c))
			fprintf(fp, "%s %s %s %s %s %ld %d\n", c->name, c->value,
			    c->server?c->server:(unsigned char *)"", c->path?c->path:(unsigned char *)"",
			    c->domain?c->domain:(unsigned char *)"", c->expires, c->secure);
	}

	fclose(fp);
}

void init_cookies()
{
	if (cookies_save)
		load_cookies();
}

void cleanup_cookies()
{
	struct cookie *c;

	free_list(c_domains);

	if (cookies_save)
		save_cookies();
	foreach (c, cookies) {
		free_cookie(c);
	}

	free_list(cookies);
}
