/* Internal cookies implementation */
/* $Id: cookies.c,v 1.170 2004/11/10 12:25:18 zas Exp $ */

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

/* #define DEBUG_COOKIES */

#include "bfu/msgbox.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "cookies/parser.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "modules/module.h"
#include "protocol/date.h"
#include "protocol/header.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#ifdef DEBUG_COOKIES
#include "util/error.h"
#endif
#include "util/file.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/secsave.h"
#include "util/string.h"
#include "util/ttime.h"

#define COOKIES_FILENAME		"cookies"


static int cookies_nosave = 0;

static INIT_LIST_HEAD(cookies);
static INIT_LIST_HEAD(cookie_queries);

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
		"0  is force expiration at the end of session, ignoring cookie's expiration date\n"
		"1+ is use cookie's expiration date, but limit age to the given number of days")),

	INIT_OPT_BOOL("cookies", N_("Paranoid security"),
		"paranoid_security", 0, 0,
		N_("When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Please see\n"
		"code (cookies.c:check_domain_security()) for explanation.")),

	INIT_OPT_BOOL("cookies", N_("Saving"),
		"save", 0, 1,
		N_("Load/save cookies from/to disk?")),

	INIT_OPT_BOOL("cookies", N_("Resaving"),
		"resave", 0, 1,
		N_("Save cookies after each change in cookies list? No effect when\n"
		"cookies_save is off.")),

	NULL_OPTION_INFO,
};

#define get_opt_cookies(which)		cookies_options[(which)].option.value
#define get_cookies_accept_policy()	get_opt_cookies(COOKIES_ACCEPT_POLICY).number
#define get_cookies_max_age()		get_opt_cookies(COOKIES_MAX_AGE).number
#define get_cookies_paranoid_security()	get_opt_cookies(COOKIES_PARANOID_SECURITY).number
#define get_cookies_save()		get_opt_cookies(COOKIES_SAVE).number
#define get_cookies_resave()		get_opt_cookies(COOKIES_RESAVE).number

struct cookie_server *
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

	cs = mem_calloc(1, sizeof(struct cookie_server) + hostlen);
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
free_cookie(struct cookie *c)
{
	if (c->box_item) done_listbox_item(&cookie_browser, c->box_item);
	if (c->server) done_cookie_server(c->server);
	mem_free_if(c->name);
	mem_free_if(c->value);
	mem_free_if(c->path);
	mem_free_if(c->domain);
	mem_free(c);
}


static int
check_domain_security(unsigned char *domain, unsigned char *server, int server_len)
{
	int i, j;
	int domain_len;
	int need_dots;

	if (domain[0] == '.') domain++;
	domain_len = strlen(domain);

	/* Match domain and server.. */

	/* XXX: Hmm, can't we use strlcasecmp() here? --pasky */

	if (domain_len > server_len) return 0;

	if (!strncasecmp(domain, server, server_len)) {
		/* We should probably allow domains which are same as servers.
		 * --<rono@sentuny.com.au> */
		/* Mozilla does it as well ;))) and I can't figure out any
		 * security risk. --pasky */
		return 0;
	}

	for (i = server_len - domain_len, j = 0; domain[j]; i++, j++)
		if (toupper(server[i]) != toupper(domain[j]))
			return 0;

	/* Also test if domain is secure enough.. */

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

static void accept_cookie_dialog(struct session *ses, void *data);

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

	cookie = mem_calloc(1, sizeof(struct cookie));
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
		free_cookie(cookie);
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
		free_cookie(cookie);
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
			ttime expires = parse_date(date); /* Convert date to seconds. */

			mem_free(date);

			if (expires) {
				if (max_age > 0) {
					int seconds = max_age*24*3600;
					ttime deadline = time(NULL) + seconds;

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
			free_cookie(cookie);
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
		      "expires at %d, secure %d\n", cookie->name,
		      cookie->value, cookie->server->host, cookie->domain,
		      cookie->expires, cookie->secure);
	}
#endif

	if (!check_domain_security(cookie->domain, uri->host, uri->hostlen)) {
#ifdef DEBUG_COOKIES
		DBG("Domain security violated: %s vs %*s", cookie->domain,
				uri->host, uri->hostlen);
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
accept_cookie(struct cookie *c)
{
	struct c_domain *cd;
	struct listbox_item *root = c->server->box_item;
	struct cookie *d, *e;
	int domain_len;

	if (root)
		c->box_item = add_listbox_leaf(&cookie_browser, root, c);

	/* Do not weed out duplicates when loading the cookie file. It doesn't
	 * scale at all, being O(N^2) and taking about 2s with my 500 cookies
	 * (so if you don't notice that 100ms with your 100 cookies, that's
	 * not an argument). --pasky */
	if (!cookies_nosave) {
		foreach (d, cookies) {
			if (strcasecmp(d->name, c->name)
			    || strcasecmp(d->domain, c->domain))
				continue;
			e = d;
			d = d->prev;
			del_from_list(e);
			free_cookie(e);
		}
	}

	add_to_list(cookies, c);
	cookies_dirty = 1;

	/* XXX: This crunches CPU too. --pasky */
	foreach (cd, c_domains)
		if (!strcasecmp(cd->domain, c->domain))
			return;

	domain_len = strlen(c->domain);
	/* One byte is reserved for domain in struct c_domain. */
	cd = mem_alloc(sizeof(struct c_domain) + domain_len);
	if (!cd) return;

	memcpy(cd->domain, c->domain, domain_len + 1);
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
	free_cookie(c);

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

/* TODO: Store cookie in data arg. --jonas*/
static void
accept_cookie_dialog(struct session *ses, void *data)
{
	struct cookie *cookie = cookie_queries.next;
	struct string string;

	assert(ses);

	if (list_empty(cookie_queries)
	    || !init_string(&string))
		return;

	del_from_list(cookie);

#ifdef HAVE_STRFTIME
	if (cookie->expires) {
		add_date_to_string(&string, get_opt_str("ui.date_format"), &cookie->expires);
	} else
#endif
		add_to_string(&string, _("at quit time", ses->tab->term));

	msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
		N_("Accept cookie?"), ALIGN_LEFT,
		msg_text(ses->tab->term, N_("Do you want to accept a cookie "
		"from %s?\n\n"
		"Name: %s\n"
		"Value: %s\n"
		"Domain: %s\n"
		"Expires: %s\n"
		"Secure: %s\n"),
		cookie->server->host, cookie->name, cookie->value,
		cookie->domain, string.source,
		_(cookie->secure ? N_("yes") : N_("no"), ses->tab->term)),
		cookie, 2,
		N_("Accept"), accept_cookie, B_ENTER,
		N_("Reject"), free_cookie, B_ESC);

	done_string(&string);
}


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
	struct cookie *c, *d;
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

	foreach (c, cookies) {
		if (!is_in_domain(c->domain, uri->host, uri->hostlen)
		    || !is_path_prefix(c->path, path))
			continue;

		if (is_expired(c->expires)) {
#ifdef DEBUG_COOKIES
			DBG("Cookie %s=%s (exp %d) expired.\n",
			      c->name, c->value, c->expires);
#endif
			d = c;
			c = c->prev;
			del_from_list(d);
			free_cookie(d);

			cookies_dirty = 1;
			continue;
		}

		/* Not sure if this is 100% right..? --pasky */
		if (c->secure && uri->protocol != PROTOCOL_HTTPS)
			continue;

		if (header.length) {
			add_to_string(&header, "; ");
		}

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

	fp = fopen(cookfile, "r");
	if (elinks_home) mem_free(cookfile);
	if (!fp) return;

	while (fgets(in_buffer, 6 * MAX_STR_LEN, fp)) {
		struct cookie *cookie;
		unsigned char *p, *q = in_buffer;
		enum { NAME = 0, VALUE, SERVER, PATH, DOMAIN, MEMBERS } member;
		unsigned char *members[MEMBERS];
		ttime expires = 0;

		memset(members, 0, sizeof(unsigned char *) * MEMBERS);

		/* First read in and allocate all members. */
		for (member = NAME; member < MEMBERS; member++, q = p) {
			p = strchr(q, '\t');
			if (!p) break;
			*p++ = '\0';
			members[member] = stracpy(q);
			if (!members[member]) break;
		}

		/* Finally get a hold of the expire field. */
		p = (member == MEMBERS ? strchr(q, '\t') : NULL);

		if (p) {
			expires = atol(q);
			if (is_dead(expires)) {
				p = NULL;
				cookies_dirty = 1;
			}
		}

		/* Prepare cookie if all members and fields was read. */
		cookie = (p ? mem_calloc(1, sizeof(struct cookie)) : NULL);

		if (cookie) {
			unsigned char *host = members[SERVER];
			int hostlen = strlen(host);

			cookie->server = get_cookie_server(host, hostlen);
			if (cookie->server) {
				mem_free(members[SERVER]);
			} else {
				mem_free(cookie);
				cookie = NULL;
			}
		}

		if (!cookie) {
			/* Something went wrong so clean up. */
			for (member = NAME; member < MEMBERS; member++)
				if (members[member])
					mem_free(members[member]);
			continue;
		}

		cookie->name	= members[NAME];
		cookie->value	= members[VALUE];
		cookie->path	= members[PATH];
		cookie->domain	= members[DOMAIN];

		*p++ = '\0';
		cookie->expires = expires;

		/* Drop ending '\n'. */
		if (*p) p[strlen(p) - 1] = '\0';
		cookie->secure = atoi(p);

		/* XXX: We don't want to overwrite the cookies file
		 * periodically to our death. */
		cookies_nosave = 1;
		accept_cookie(cookie);
		cookies_nosave = 0;
	}

	fclose(fp);
}

void
save_cookies(void) {
	struct cookie *c;
	unsigned char *cookfile;
	struct secure_save_info *ssi;

	if (cookies_nosave || !elinks_home || !cookies_dirty
	    || get_cmd_opt_int("anonymous"))
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

		del_from_list(cookie);
		free_cookie(cookie);
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
