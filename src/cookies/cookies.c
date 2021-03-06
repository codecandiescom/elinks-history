/* Internal cookies implementation */
/* $Id: cookies.c,v 1.207 2005/07/27 23:38:33 jonas Exp $ */

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

#if 0
#define DEBUG_COOKIES
#endif

#include "bfu/dialog.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "cookies/parser.h"
#include "config/home.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/object.h"
#include "protocol/date.h"
#include "protocol/header.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#ifdef DEBUG_COOKIES
#include "util/error.h"
#endif
#include "util/file.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"
#include "util/time.h"

#define COOKIES_FILENAME		"cookies"


static int cookies_nosave = 0;

static INIT_LIST_HEAD(cookies);

struct c_domain {
	LIST_HEAD(struct c_domain);

	unsigned char domain[1]; /* Must be at end of struct. */
};

static INIT_LIST_HEAD(c_domains);

static INIT_LIST_HEAD(cookie_servers);

static int cookies_dirty = 0;

enum cookies_option {
	COOKIES_TREE,

	COOKIES_ACCEPT_POLICY,
	COOKIES_MAX_AGE,
	COOKIES_PARANOID_SECURITY,
	COOKIES_SAVE,
	COOKIES_RESAVE,

	COOKIES_OPTIONS,
};

static struct option_info cookies_options[] = {
	INIT_OPT_TREE("", N_("Cookies"),
		"cookies", 0,
		N_("Cookies options.")),

	INIT_OPT_INT("cookies", N_("Accept policy"),
		"accept_policy", 0,
		COOKIES_ACCEPT_NONE, COOKIES_ACCEPT_ALL, COOKIES_ACCEPT_ALL,
		N_("Cookies accepting policy:\n"
		"0 is accept no cookies\n"
		"1 is ask for confirmation before accepting cookie\n"
		"2 is accept all cookies")),

	INIT_OPT_INT("cookies", N_("Maximum age"),
		"max_age", 0, -1, 10000, -1,
		N_("Cookie maximum age (in days):\n"
		"-1 is use cookie's expiration date if any\n"
		"0  is force expiration at the end of session, ignoring cookie's\n"
		"   expiration date\n"
		"1+ is use cookie's expiration date, but limit age to the given\n"
		"   number of days")),

	INIT_OPT_BOOL("cookies", N_("Paranoid security"),
		"paranoid_security", 0, 0,
		N_("When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Some countries\n"
		"have generic second level domains (eg. .com.pl, .co.uk) and allowing\n"
		"sites to set cookies for these generic domains could potentially be\n"
		"very bad. Note, it is off by default as it breaks a lot of sites.")),

	INIT_OPT_BOOL("cookies", N_("Saving"),
		"save", 0, 1,
		N_("Whether cookies should be loaded from and save to disk.")),

	INIT_OPT_BOOL("cookies", N_("Resaving"),
		"resave", 0, 1,
		N_("Save cookies after each change in cookies list? No effect when\n"
		"cookie saving (cookies.save) is off.")),

	NULL_OPTION_INFO,
};

#define get_opt_cookies(which)		cookies_options[(which)].option.value
#define get_cookies_accept_policy()	get_opt_cookies(COOKIES_ACCEPT_POLICY).number
#define get_cookies_max_age()		get_opt_cookies(COOKIES_MAX_AGE).number
#define get_cookies_paranoid_security()	get_opt_cookies(COOKIES_PARANOID_SECURITY).number
#define get_cookies_save()		get_opt_cookies(COOKIES_SAVE).number
#define get_cookies_resave()		get_opt_cookies(COOKIES_RESAVE).number

static struct cookie_server *
get_cookie_server(unsigned char *host, int hostlen)
{
	struct cookie_server *sort_spot = NULL;
	struct cookie_server *cs;

	foreach (cs, cookie_servers) {
		/* XXX: We must count with cases like "x.co" vs "x.co.uk"
		 * below! */
		int cslen = strlen(cs->host);
		int cmp = strncasecmp(cs->host, host, hostlen);

		if (!sort_spot && (cmp > 0 || (cmp == 0 && cslen > hostlen))) {
			/* This is the first @cs with name greater than @host,
			 * our dream sort spot! */
			sort_spot = cs->prev;
		}

		if (cmp || cslen != hostlen)
			continue;

		object_lock(cs);
		return cs;
	}

	cs = mem_calloc(1, sizeof(*cs) + hostlen);
	if (!cs) return NULL;

	memcpy(cs->host, host, hostlen);
	object_nolock(cs, "cookie_server");

	cs->box_item = add_listbox_folder(&cookie_browser, NULL, cs);

	object_lock(cs);

	if (!sort_spot) {
		/* No sort spot found, therefore this sorts at the end. */
		add_to_list_end(cookie_servers, cs);
		del_from_list(cs->box_item);
		add_to_list_end(cookie_browser.root.child, cs->box_item);
	} else {
		/* Sort spot found, sort after it. */
		add_at_pos(sort_spot, cs);
		if (sort_spot != (struct cookie_server *) &cookie_servers) {
			del_from_list(cs->box_item);
			add_at_pos(sort_spot->box_item, cs->box_item);
		} /* else we are already at the top anyway. */
	}

	return cs;
}

static void
done_cookie_server(struct cookie_server *cs)
{
	object_unlock(cs);
	if (is_object_used(cs)) return;

	if (cs->box_item) done_listbox_item(&cookie_browser, cs->box_item);
	del_from_list(cs);
	mem_free(cs);
}

void
done_cookie(struct cookie *c)
{
	if (c->box_item) done_listbox_item(&cookie_browser, c->box_item);
	if (c->server) done_cookie_server(c->server);
	mem_free_if(c->name);
	mem_free_if(c->value);
	mem_free_if(c->path);
	mem_free_if(c->domain);
	mem_free(c);
}

void
delete_cookie(struct cookie *c)
{
	del_from_list(c);
	done_cookie(c);
}


/* Check whether cookie's domain matches server.
 * It returns 1 if ok, 0 else. */
static int
is_domain_security_ok(unsigned char *domain, unsigned char *server, int server_len)
{
	int i;
	int domain_len;
	int need_dots;

	if (domain[0] == '.') domain++;
	domain_len = strlen(domain);

	/* Match domain and server.. */

	/* XXX: Hmm, can't we use strlcasecmp() here? --pasky */

	if (domain_len > server_len) return 0;

	/* Ensure that the domain is atleast a substring of the server before
	 * continuing. */
	if (strncasecmp(domain, server + server_len - domain_len, domain_len))
		return 0;

	/* Allow domains which are same as servers. --<rono@sentuny.com.au> */
	/* Mozilla does it as well ;))) and I can't figure out any security
	 * risk. --pasky */
	if (server_len == domain_len)
		return 1;

	/* Check whether the server is an IP address, and require an exact host
	 * match for the cookie, so any chance of IP address funkiness is
	 * eliminated (e.g. the alias 127.1 domain-matching 99.54.127.1). Idea
	 * from mozilla. (bug 562) */
	if (is_ip_address(server, server_len))
		return 0;

	/* Also test if domain is secure en ugh.. */

	need_dots = 1;

	if (get_cookies_paranoid_security()) {
		/* This is somehow controversial attempt (by the way violating
		 * RFC) to increase cookies security in national domains, done
		 * by Mikulas. As it breaks a lot of sites, I decided to make
		 * this optional and off by default. I also don't think this
		 * improves security considerably, as it's SITE'S fault and
		 * also no other browser probably does it. --pasky */
		/* Mikulas' comment: Some countries have generic 2-nd level
		 * domains (like .com.pl, .co.uk ...) and it would be very bad
		 * if someone set cookies for these generic domains.  Imagine
		 * for example that server http://brutalporn.com.pl sets cookie
		 * Set-Cookie: user_is=perverse_pig; domain=.com.pl -- then
		 * this cookie would be sent to all commercial servers in
		 * Poland. */
		need_dots = 2;

		if (domain_len > 0) {
			int pos = end_with_known_tld(domain, domain_len);

			if (pos >= 1 && domain[pos - 1] == '.')
				need_dots = 1;
		}
	}

	for (i = 0; domain[i]; i++)
		if (domain[i] == '.' && !--need_dots)
			break;

	if (need_dots > 0) return 0;
	return 1;
}

void
set_cookie(struct uri *uri, unsigned char *str)
{
	unsigned char *secure, *path;
	struct cookie *cookie;
	struct cookie_str cstr;
	int max_age;

	if (get_cookies_accept_policy() == COOKIES_ACCEPT_NONE)
		return;

#ifdef DEBUG_COOKIES
	DBG("set_cookie -> (%s) %s", struri(uri), str);
#endif

	if (!parse_cookie_str(&cstr, str)) return;

	cookie = mem_calloc(1, sizeof(*cookie));
	if (!cookie) return;

	object_nolock(cookie, "cookie"); /* Debugging purpose. */

	/* Fill main fields */

	cookie->name = memacpy(str, cstr.nam_end - str);
	cookie->value = memacpy(cstr.val_start, cstr.val_end - cstr.val_start);
	cookie->server = get_cookie_server(uri->host, uri->hostlen);
	cookie->domain = parse_header_param(str, "domain");
	if (!cookie->domain) cookie->domain = memacpy(uri->host, uri->hostlen);

	/* Now check that all is well */
	if (!cookie->domain
	    || !cookie->name
	    || !cookie->value
	    || !cookie->server) {
		done_cookie(cookie);
		return;
	}

#if 0
	/* We don't actually set ->accept at the moment. But I have kept it
	 * since it will maybe help to fix bug 77 - Support for more
	 * finegrained control upon accepting of cookies. */
	if (!cookie->server->accept) {
#ifdef DEBUG_COOKIES
		DBG("Dropped.");
#endif
		done_cookie(cookie);
		return;
	}
#endif

	/* Set cookie expiration if needed.
	 * Cookie expires at end of session by default,
	 * set to 0 by calloc().
	 *
	 * max_age:
	 * -1 is use cookie's expiration date if any
	 * 0  is force expiration at the end of session,
	 *    ignoring cookie's expiration date
	 * 1+ is use cookie's expiration date,
	 *    but limit age to the given number of days.
	 */

	max_age = get_cookies_max_age();
	if (max_age) {
		unsigned char *date = parse_header_param(str, "expires");

		if (date) {
			time_t expires = parse_date(&date, NULL, 0, 1); /* Convert date to seconds. */

			mem_free(date);

			if (expires) {
				if (max_age > 0) {
					int seconds = max_age*24*3600;
					time_t deadline = time(NULL) + seconds;

					if (expires > deadline) /* Over-aged cookie ? */
						expires = deadline;
				}

				cookie->expires = expires;
			}
		}
	}

	path = parse_header_param(str, "path");
	if (!path) {
		unsigned char *path_end;

		path = get_uri_string(uri, URI_PATH);
		if (!path) {
			done_cookie(cookie);
			return;
		}

		for (path_end = path + strlen(path) - 1;
		     path_end >= path; path_end--) {
			if (*path_end == '/') {
				path_end[1] = '\0';
				break;
			}
		}

	} else {
		if (!path[0]
		    || path[strlen(path) - 1] != '/')
			add_to_strn(&path, "/");

		if (path[0] != '/') {
			add_to_strn(&path, "x");
			memmove(path + 1, path, strlen(path) - 1);
			path[0] = '/';
		}
	}
	cookie->path = path;

	if (cookie->domain[0] == '.')
		memmove(cookie->domain, cookie->domain + 1,
			strlen(cookie->domain));

	/* cookie->secure is set to 0 by default by calloc(). */
	secure = parse_header_param(str, "secure");
	if (secure) {
		cookie->secure = 1;
		mem_free(secure);
	}

#ifdef DEBUG_COOKIES
	{
		DBG("Got cookie %s = %s from %s, domain %s, "
		      "expires at %d, secure %d", cookie->name,
		      cookie->value, cookie->server->host, cookie->domain,
		      cookie->expires, cookie->secure);
	}
#endif

	if (!is_domain_security_ok(cookie->domain, uri->host, uri->hostlen)) {
#ifdef DEBUG_COOKIES
		DBG("Domain security violated: %s vs %.*s", cookie->domain,
		    uri->hostlen, uri->host);
#endif
		mem_free(cookie->domain);
		cookie->domain = memacpy(uri->host, uri->hostlen);
	}

	/* We have already check COOKIES_ACCEPT_NONE */
	if (get_cookies_accept_policy() == COOKIES_ACCEPT_ASK) {
		add_to_list(cookie_queries, cookie);
		add_questions_entry(accept_cookie_dialog, cookie);
		return;
	}

	accept_cookie(cookie);
}

void
accept_cookie(struct cookie *cookie)
{
	struct c_domain *cd;
	struct listbox_item *root = cookie->server->box_item;
	int domain_len;

	if (root)
		cookie->box_item = add_listbox_leaf(&cookie_browser, root, cookie);

	/* Do not weed out duplicates when loading the cookie file. It doesn't
	 * scale at all, being O(N^2) and taking about 2s with my 500 cookies
	 * (so if you don't notice that 100ms with your 100 cookies, that's
	 * not an argument). --pasky */
	if (!cookies_nosave) {
		struct cookie *c, *next;

		foreachsafe (c, next, cookies) {
			if (strcasecmp(c->name, cookie->name)
			    || strcasecmp(c->domain, cookie->domain))
				continue;

			delete_cookie(c);
		}
	}

	add_to_list(cookies, cookie);
	cookies_dirty = 1;

	/* XXX: This crunches CPU too. --pasky */
	foreach (cd, c_domains)
		if (!strcasecmp(cd->domain, cookie->domain))
			return;

	domain_len = strlen(cookie->domain);
	/* One byte is reserved for domain in struct c_domain. */
	cd = mem_alloc(sizeof(*cd) + domain_len);
	if (!cd) return;

	memcpy(cd->domain, cookie->domain, domain_len + 1);
	add_to_list(c_domains, cd);

	if (get_cookies_save() && get_cookies_resave())
		save_cookies();
}

#if 0
static unsigned int cookie_id = 0;

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
	done_cookie(c);

	if (get_cookies_save() && get_cookies_resave())
		save_cookies();
}


static struct
cookie *find_cookie_id(void *idp)
{
	int id = (int) idp;
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
	struct cookie *c = find_cookie_id(idp);

	if (c) c->server->accept = a;
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

/* Check whether domain is matching server
 * Ie.
 * example.com matches www.example.com/
 * example.com doesn't match www.example.com.org/
 * example.com doesn't match www.example.comm/
 * example.com doesn't match example.co
 */
static int
is_in_domain(unsigned char *domain, unsigned char *server, int server_len)
{
	int domain_len = strlen(domain);
	int len;

	if (domain_len > server_len)
		return 0;

	if (domain_len == server_len)
		return !strncasecmp(domain, server, server_len);

	len = server_len - domain_len;
	if (server[len - 1] != '.')
		return 0;

	return !strncasecmp(domain, server + len, domain_len);
}


static inline int
is_path_prefix(unsigned char *d, unsigned char *s)
{
	int dl = strlen(d);

	/* TODO: strlcmp()? --pasky */

	if (dl > strlen(s)) return 0;

	return !memcmp(d, s, dl);
}


#define is_expired(t) ((t) && (t) <= time(NULL))
#define is_dead(t) (!(t) || (t) <= time(NULL))

struct string *
send_cookies(struct uri *uri)
{
	struct c_domain *cd;
	struct cookie *c, *next;
	unsigned char *path = NULL;
	static struct string header;

	if (!uri->host || !uri->data)
		return NULL;

	foreach (cd, c_domains)
		if (is_in_domain(cd->domain, uri->host, uri->hostlen)) {
			path = get_uri_string(uri, URI_PATH);
			break;
		}

	if (!path) return NULL;

	init_string(&header);

	foreachsafe (c, next, cookies) {
		if (!is_in_domain(c->domain, uri->host, uri->hostlen)
		    || !is_path_prefix(c->path, path))
			continue;

		if (is_expired(c->expires)) {
#ifdef DEBUG_COOKIES
			DBG("Cookie %s=%s (exp %d) expired.",
			    c->name, c->value, c->expires);
#endif
			delete_cookie(c);

			cookies_dirty = 1;
			continue;
		}

		/* Not sure if this is 100% right..? --pasky */
		if (c->secure && uri->protocol != PROTOCOL_HTTPS)
			continue;

		if (header.length)
			add_to_string(&header, "; ");

		add_to_string(&header, c->name);
		add_char_to_string(&header, '=');
		add_to_string(&header, c->value);
#ifdef DEBUG_COOKIES
		DBG("Cookie: %s=%s", c->name, c->value);
#endif
	}

	mem_free(path);

	if (cookies_dirty && get_cookies_save() && get_cookies_resave())
		save_cookies();

	if (!header.length) {
		done_string(&header);
		return NULL;
	}

	return &header;
}

static void done_cookies(struct module *module);


void
load_cookies(void) {
	/* Buffer size is set to be enough to read long lines that
	 * save_cookies may write. 6 is choosen after the fprintf(..) call
	 * in save_cookies(). --Zas */
	unsigned char in_buffer[6 * MAX_STR_LEN];
	unsigned char *cookfile = COOKIES_FILENAME;
	FILE *fp;

	if (elinks_home) {
		cookfile = straconcat(elinks_home, cookfile, NULL);
		if (!cookfile) return;
	}

	/* Do it here, as we will delete whole cookies list if the file was
	 * removed */
	cookies_nosave = 1;
	done_cookies(&cookies_module);
	cookies_nosave = 0;

	fp = fopen(cookfile, "rb");
	if (elinks_home) mem_free(cookfile);
	if (!fp) return;

	/* XXX: We don't want to overwrite the cookies file
	 * periodically to our death. */
	cookies_nosave = 1;

	while (fgets(in_buffer, 6 * MAX_STR_LEN, fp)) {
		struct cookie *cookie;
		unsigned char *p, *q = in_buffer;
		enum { NAME = 0, VALUE, SERVER, PATH, DOMAIN, EXPIRES, SECURE, MEMBERS } member;
		struct {
			unsigned char *pos;
			int len;
		} members[MEMBERS];
		time_t expires;

		/* First find all members. */
		for (member = NAME; member < MEMBERS; member++, q = ++p) {
			p = strchr(q, '\t');
			if (!p) {
				if (member + 1 != MEMBERS) break; /* last field ? */
				p = strchr(q, '\n');
				if (!p) break;
			}

			members[member].pos = q;
			members[member].len = p - q;
		}

		if (member != MEMBERS) continue;	/* Invalid line. */

		/* Skip expired cookies if any. */
		expires = str_to_time_t(members[EXPIRES].pos);
		if (is_dead(expires)) {
			cookies_dirty = 1;
			continue;
		}

		/* Prepare cookie if all members and fields was read. */
		cookie = mem_calloc(1, sizeof(*cookie));
		if (!cookie) continue;

		cookie->server  = get_cookie_server(members[SERVER].pos, members[SERVER].len);
		cookie->name	= memacpy(members[NAME].pos, members[NAME].len);
		cookie->value	= memacpy(members[VALUE].pos, members[VALUE].len);
		cookie->path	= memacpy(members[PATH].pos, members[PATH].len);
		cookie->domain	= memacpy(members[DOMAIN].pos, members[DOMAIN].len);

		/* Check whether all fields were correctly allocated. */
		if (!cookie->server || !cookie->name || !cookie->value
		    || !cookie->path || !cookie->domain) {
			done_cookie(cookie);
			continue;
		}

		cookie->expires = expires;
		cookie->secure  = !!atoi(members[SECURE].pos);

		accept_cookie(cookie);
	}

	cookies_nosave = 0;
	fclose(fp);
}

void
save_cookies(void) {
	struct cookie *c;
	unsigned char *cookfile;
	struct secure_save_info *ssi;

	if (cookies_nosave || !elinks_home || !cookies_dirty
	    || get_cmd_opt_bool("anonymous"))
		return;

	cookfile = straconcat(elinks_home, COOKIES_FILENAME, NULL);
	if (!cookfile) return;

	ssi = secure_open(cookfile, 0177); /* rw for user only */
	mem_free(cookfile);
	if (!ssi) return;

	foreach (c, cookies) {
		if (is_dead(c->expires)) continue;
		if (secure_fprintf(ssi, "%s\t%s\t%s\t%s\t%s\t%ld\t%d\n",
				   c->name, c->value,
				   c->server->host,
				   empty_string_or_(c->path),
				   empty_string_or_(c->domain),
				   c->expires, c->secure) < 0)
			break;
	}

	if (!secure_close(ssi)) cookies_dirty = 0;
}

static void
init_cookies(struct module *module)
{
	if (get_cookies_save())
		load_cookies();
}

static void
free_cookies_list(struct list_head *list)
{
	while (!list_empty(*list)) {
		struct cookie *cookie = list->next;

		delete_cookie(cookie);
	}
}

static void
done_cookies(struct module *module)
{
	free_list(c_domains);

	if (!cookies_nosave && get_cookies_save())
		save_cookies();

	free_cookies_list(&cookies);
	free_cookies_list(&cookie_queries);
}

struct module cookies_module = struct_module(
	/* name: */		N_("Cookies"),
	/* options: */		cookies_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_cookies,
	/* done: */		done_cookies
);
