/* Internal cookies implementation */
/* $Id: cookies.c,v 1.121 2004/03/11 04:44:23 witekfl Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_COOKIES

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

#include "bfu/msgbox.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "cookies/parser.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "modules/module.h"
#include "protocol/http/date.h"
#include "protocol/http/header.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#ifdef COOKIES_DEBUG
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

static unsigned int cookie_id = 0;

static INIT_LIST_HEAD(cookies);
static INIT_LIST_HEAD(cookie_queries);

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

void
free_cookie(struct cookie *c)
{
	if (c->box_item) done_listbox_item(&cookie_browser, c->box_item);
	if (c->name) mem_free(c->name);
	if (c->value) mem_free(c->value);
	if (c->server) mem_free(c->server);
	if (c->path) mem_free(c->path);
	if (c->domain) mem_free(c->domain);
	mem_free(c);
}


static int
check_domain_security(unsigned char *domain, unsigned char *server, int server_len)
{
	register int i, j;
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
		if (upcase(server[i]) != upcase(domain[j]))
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
	unsigned char *date, *secure;
	struct cookie *cookie;
	struct c_server *cs;
	struct cookie_str cstr;

	if (get_cookies_accept_policy() == COOKIES_ACCEPT_NONE)
		return;

#ifdef COOKIES_DEBUG
	DBG("set_cookie -> (%s) %s", struri(*uri), str);
#endif

	cstr.str = str;
	if (!parse_cookie_str(&cstr)) return;

	cookie = mem_calloc(1, sizeof(struct cookie));
	if (!cookie) return;

	object_nolock(cookie); /* Debugging purpose. */

	/* Fill main fields */

	cookie->name = memacpy(str, cstr.nam_end - str);
	cookie->value = memacpy(cstr.val_start, cstr.val_end - cstr.val_start);
	cookie->server = memacpy(uri->host, uri->hostlen);
	cookie->domain = parse_http_header_param(str, "domain");
	if (!cookie->domain) cookie->domain = memacpy(uri->host, uri->hostlen);

	/* Now check that all is well */
	if (!cookie->domain
	    || !cookie->name
	    || !cookie->value
	    || !cookie->server) {
		free_cookie(cookie);
		return;
	}

	/* Get expiration date */

	date = parse_http_header_param(str, "expires");
	if (date) {
		cookie->expires = parse_http_date(date); /* Convert date to seconds. */

		if (cookie->expires) {
			int max_age = get_cookies_max_age(); /* Max. age in days */

			if (max_age == 0) cookie->expires = 0; /* Always expires at session end. */
			else if (max_age > 0) {
				ttime deadline = time(NULL) + max_age*24*3600 /* days->seconds.*/;

				if (cookie->expires > deadline) /* Over-aged cookie ? */
					cookie->expires = deadline;
			}
		}

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
		struct string path;

		if (!init_string(&path)) {
			free_cookie(cookie);
			return;
		}

		add_char_to_string(&path, '/');
		add_bytes_to_string(&path, uri->data, uri->datalen);

		cookie->path = path.source;
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
	{
		unsigned char *server = memacpy(uri->host, uri->hostlen);

		DBG("Got cookie %s = %s from %s (%s), domain %s, "
		      "expires at %d, secure %d\n", cookie->name,
		      cookie->value, cookie->server, server, cookie->domain,
		      cookie->expires, cookie->secure);
		if (server) mem_free(server);
	}
#endif

	if (!check_domain_security(cookie->domain, uri->host, uri->hostlen)) {
#ifdef COOKIES_DEBUG
		DBG("Domain security violated: %s vs %*s", cookie->domain,
				uri->host, uri->hostlen);
#endif
		mem_free(cookie->domain);
		cookie->domain = memacpy(uri->host, uri->hostlen);
	}

	cookie->id = cookie_id++;

	foreach (cs, c_servers) {
		if (strlcasecmp(cs->server, -1, uri->host, uri->hostlen))
			continue;

		if (cs->accept)	goto ok;

#ifdef COOKIES_DEBUG
		DBG("Dropped.");
#endif
		free_cookie(cookie);
		return;
	}

	/* We have already check COOKIES_ACCEPT_NONE */
	if (get_cookies_accept_policy() == COOKIES_ACCEPT_ASK) {
		add_to_list(cookie_queries, cookie);
		add_questions_entry(accept_cookie_dialog, cookie);
		return;
	}

ok:
	accept_cookie(cookie);

	return;
}


void
accept_cookie(struct cookie *c)
{
	struct c_domain *cd;
	struct cookie *d, *e;
	int domain_len;

	c->box_item = add_listbox_items(&cookie_browser, c, 1, c->server, c->name, NULL);

	foreach (d, cookies) {
		if (strcasecmp(d->name, c->name)
		    || strcasecmp(d->domain, c->domain))
			continue;
		e = d;
		d = d->prev;
		del_from_list(e);
		free_cookie(e);
	}

	add_to_list(cookies, c);
	cookies_dirty = 1;

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
	int server_len;

	if (!c) return;

	foreach (s, c_servers)
		if (!strcasecmp(s->server, c->server))
			goto found;

	server_len = strlen(c->server);
	/* One byte is reserved for server in struct c_server. */
	s = mem_alloc(sizeof(struct c_server) + server_len);
	if (s) {
		memcpy(s->server, c->server, server_len + 1);
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

/* TODO: Store cookie in data arg. --jonas*/
static void
accept_cookie_dialog(struct session *ses, void *data)
{
	struct cookie *cookie = cookie_queries.next;
	unsigned char *expires = NULL;

	assert(ses);

	if (list_empty(cookie_queries)) return;

	del_from_list(cookie);

#ifdef HAVE_STRFTIME
	if (cookie->expires) {
		struct tm *when_local = localtime(&cookie->expires);
		unsigned char str[13];
		int wr = strftime(str, sizeof(str), "%b %e %H:%M", when_local);

		if (wr > 0)
			expires = memacpy(str, wr);
	}
#endif

	msg_box(ses->tab->term, getml(expires, NULL), MSGBOX_FREE_TEXT,
		N_("Accept cookie?"), AL_LEFT,
		msg_text(ses->tab->term, N_("Do you want to accept a cookie "
		"from %s?\n\n"
		"Name: %s\n"
		"Value: %s\n"
		"Domain: %s\n"
		"Expires: %s\n"
		"Secure: %s\n"),
		cookie->server, cookie->name, cookie->value,
		cookie->domain,
		expires ? expires : _("at quit time",  ses->tab->term),
		_(cookie->secure ? N_("yes") : N_("no"), ses->tab->term)),
		cookie, 2,
		N_("Accept"), accept_cookie, B_ENTER,
		N_("Reject"), free_cookie, B_ESC);
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
is_path_prefix(unsigned char *d, unsigned char *s, int sl)
{
	int dl = strlen(d);

	/* TODO: strlcmp()? --pasky */

	if (dl > sl) return 0;

	return !memcmp(d, s, dl);
}


#define is_expired(t) ((t) && (t) <= time(NULL))
#define is_dead(t) (!(t) || (t) <= time(NULL))

struct string *
send_cookies(struct uri *uri)
{
	struct c_domain *cd;
	struct cookie *c, *d;
	unsigned char *data = NULL;
	int datalen = uri->datalen + 1;
	static struct string header;

	if (!uri->host || !uri->data)
		return NULL;

	foreach (cd, c_domains)
		if (is_in_domain(cd->domain, uri->host, uri->hostlen)) {
			data = uri->data - 1;
			break;
		}

	if (!data) return NULL;

	init_string(&header);

	foreach (c, cookies) {
		if (!is_in_domain(c->domain, uri->host, uri->hostlen)
		    || !is_path_prefix(c->path, data, datalen))
			continue;

		if (is_expired(c->expires)) {
#ifdef COOKIES_DEBUG
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
#ifdef COOKIES_DEBUG
		DBG("Cookie: %s=%s", c->name, c->value);
#endif
	}

	if (cookies_dirty && get_cookies_save() && get_cookies_resave())
		save_cookies();

	if (!header.length)
		done_string(&header);

	return header.length ? &header : NULL;
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

		if (!cookie) {
			/* Something went wrong so clean up. */
			for (member = NAME; member < MEMBERS; member++)
				if (members[member])
					mem_free(members[member]);
			continue;
		}

		cookie->name	= members[NAME];
		cookie->value	= members[VALUE];
		cookie->server	= members[SERVER];
		cookie->path	= members[PATH];
		cookie->domain	= members[DOMAIN];

		*p++ = '\0';
		cookie->expires = expires;

		/* Drop ending '\n'. */
		if (*p) p[strlen(p) - 1] = '\0';
		cookie->secure = atoi(p);

		cookie->id = cookie_id++;

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
	    || get_opt_int_tree(cmdline_options, "anonymous"))
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
				   empty_string_or_(c->server),
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
done_cookies(struct module *module)
{
	free_list(c_domains);

	if (!cookies_nosave && get_cookies_save())
		save_cookies();

	while (!list_empty(cookies)) {
		struct cookie *cookie = cookies.next;

		del_from_list(cookie);
		free_cookie(cookie);
	}

	while (!list_empty(cookie_queries)) {
		struct cookie *cookie = cookie_queries.next;

		del_from_list(cookie);
		free_cookie(cookie);
	}
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

#endif /* CONFIG_COOKIES */
