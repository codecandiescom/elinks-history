/* Perl scripting hooks */
/* $Id: hooks.c,v 1.11 2004/06/07 15:58:08 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static inline void
do_script_hook_goto_url(struct uri *current_uri, unsigned char **url)
{
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(*url, strlen(*url));
	if (!current_uri) {
		XPUSHs(sv_2mortal(newSV(0)));
	} else {
		unsigned char *uri = struri(current_uri);

		my_XPUSHs(uri, strlen(uri));
	}
	PUTBACK;

	count = call_pv("goto_url_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
		STRLEN n_a;	/* Used by POPpx macro. */
		unsigned char *new_url = POPpx;

		if (new_url) {
			unsigned char *n = memacpy(new_url, n_a);

			if (n) {
				mem_free(*url);
				*url = n;
			}
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct uri *current_uri = va_arg(ap, struct uri *);

	if (my_perl && *url)
		do_script_hook_goto_url(current_uri, url);

	return EHS_NEXT;
}

static inline void
do_script_hook_follow_url(unsigned char **url)
{
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(*url, strlen(*url));
	PUTBACK;

	count = call_pv("follow_url_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
		STRLEN n_a;	/* Used by POPpx macro. */
		unsigned char *new_url = POPpx;

		if (new_url) {
			unsigned char *n = memacpy(new_url, n_a);

			if (n) {
				mem_free(*url);
				*url = n;
			}
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);

	if (my_perl && *url)
		do_script_hook_follow_url(url);

	return EHS_NEXT;
}

static inline void
do_script_hook_pre_format_html(unsigned char *url, unsigned char **html,
			       int *html_len)
{
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(url, strlen(url));
	my_XPUSHs(*html, *html_len);
	PUTBACK;

	count = call_pv("pre_format_html_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
		STRLEN n_a;	/* Used by POPpx macro. */
		unsigned char *new_html = POPpx;

		if (new_html) {
			*html_len = n_a;
			*html = memacpy(new_html, *html_len);
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	unsigned char **html = va_arg(ap, unsigned char **);
	int *html_len = va_arg(ap, int *);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);

	if (my_perl && ses && url && *html && *html_len)
		do_script_hook_pre_format_html(url, html, html_len);

	return EHS_NEXT;
}

static inline void
do_script_hook_get_proxy(unsigned char **new_proxy_url, unsigned char *url)
{
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(url, strlen(url));
	PUTBACK;

	count = call_pv("proxy_for_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
		STRLEN n_a;	/* Used by POPpx macro. */
		unsigned char *new_url = POPpx;

		if (new_url) {
			*new_proxy_url = memacpy(new_url, n_a);
		} else {
			*new_proxy_url = NULL;
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);

	if (my_perl && new_proxy_url && *new_proxy_url && url)
		do_script_hook_get_proxy(new_proxy_url, url);

	return EHS_NEXT;
}

static inline void
do_script_hook_quit(void)
{
	dSP;

	PUSHMARK(SP);

	call_pv("quit_hook", G_EVAL | G_DISCARD | G_NOARGS);
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	if (my_perl)
		do_script_hook_quit();

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
