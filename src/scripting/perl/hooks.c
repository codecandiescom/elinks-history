/* Perl scripting hooks */
/* $Id: hooks.c,v 1.4 2004/04/16 09:23:40 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PERL

#include "elinks.h"

#include "protocol/uri.h"
#include "sched/event.h"
#include "sched/session.h"
#include "scripting/perl/core.h"
#include "scripting/perl/hooks.h"
#include "scripting/scripting.h"
#include "util/string.h"

#define my_XPUSHs(s, slen) XPUSHs(sv_2mortal(newSVpvn(s, slen)))

/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *new_url = NULL;
	STRLEN n_a;
	int err;
	dSP;

	if (*url == NULL) return EHS_NEXT;
	if (!my_perl) return EHS_NEXT;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	my_XPUSHs(*url, strlen(*url));

	if (!have_location(ses)) {
		XPUSHs(sv_2mortal(newSV(0)));
	} else {
		unsigned char *uri = struri(cur_loc(ses)->vs.uri);

		my_XPUSHs(uri, strlen(uri));
	}
	PUTBACK;
	err = call_pv("goto_url_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) err = 0;
	SPAGAIN;
	if (err == 1) new_url = POPpx;

	if (new_url) {
		unsigned char *n = memacpy(new_url, n_a);

		if (n) {
			mem_free(*url);
			*url = n;
		}
	}
	PUTBACK;
	FREETMPS;
	LEAVE;
	return EHS_NEXT;
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	unsigned char *new_url = NULL;
	STRLEN n_a;
	int err;
	dSP;

	if (*url == NULL) return EHS_NEXT;
	if (!my_perl) return EHS_NEXT;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	my_XPUSHs(*url, strlen(*url));
	PUTBACK;
	err = call_pv("follow_url_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) err = 0;
	SPAGAIN;
	if (err == 1) new_url = POPpx;

	if (new_url) {
		unsigned char *n = memacpy(new_url, n_a);

		if (n) {
			mem_free(*url);
			*url = n;
		}
	}
	PUTBACK;
	FREETMPS;
	LEAVE;
	return EHS_NEXT;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	unsigned char **html = va_arg(ap, unsigned char **);
	int *html_len = va_arg(ap, int *);
	unsigned char *url;
	unsigned char *new_html = NULL;
	struct session *ses;
	STRLEN n_a;
	int err;
	dSP;

	ses = va_arg(ap, struct session *);
	url = va_arg(ap, unsigned char *);

	if (*html == NULL || *html_len == 0) return EHS_NEXT;
	if (!my_perl) return EHS_NEXT;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	my_XPUSHs(url, strlen(url));
	my_XPUSHs(*html, *html_len);
	PUTBACK;
	err = call_pv("pre_format_html_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) err = 0;
	SPAGAIN;
	if (err == 1) new_html = POPpx;

	if (new_html) {
		*html_len = n_a;
		*html = memacpy(new_html, *html_len);
	}
	PUTBACK;
	FREETMPS;
	LEAVE;
	return EHS_NEXT;
}

/* The Lua function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies */
static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	unsigned char *new_url = NULL;
	STRLEN n_a;
	int err;
	dSP;

	if (*new_proxy_url == NULL) return EHS_NEXT;
	if (!my_perl) return EHS_NEXT;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	my_XPUSHs(url, strlen(url));
	PUTBACK;
	err = call_pv("proxy_for_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) err = 0;
	SPAGAIN;
	if (err == 1) new_url = POPpx;

	if (new_url) {
		*new_proxy_url = memacpy(new_url, n_a);
	} else {
		*new_proxy_url = NULL;
	}
	PUTBACK;
	FREETMPS;
	LEAVE;
	return EHS_NEXT;
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	if (my_perl) call_pv("quit_hook", G_EVAL | G_DISCARD | G_NOARGS);
	return EHS_NEXT;
}

struct event_hook_info perl_scripting_hooks[] = {
	{ "goto-url", script_hook_goto_url, NULL },
	{ "follow-url", script_hook_follow_url, NULL },
	{ "pre-format-html", script_hook_pre_format_html, NULL },
	{ "get-proxy", script_hook_get_proxy, NULL },
	{ "quit", script_hook_quit, NULL },
	NULL_EVENT_HOOK_INFO,
};

#endif
