/* Guile scripting hooks */
/* $Id: hooks.c,v 1.4 2003/09/23 00:47:19 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GUILE

#include <libguile.h>

#include "elinks.h"

#include "sched/event.h"
#include "sched/session.h"
#include "scripting/guile/hooks.h"
#include "util/string.h"


static SCM
internal_module(void)
{
	/* XXX: should use internal_module from core.c instead of referring to
	 * the module by name, once I figure out how... --pw */
	return scm_c_resolve_module("elinks internal");
}


/*
 * goto_url:
 *
 * This function can do one of the following:
 *  - call goto_url on the url given;
 *  - call goto_url on some other url;
 *  - do nothing.
 */

static int
script_hook_goto_url(va_list ap)
{
	unsigned char **returl = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	SCM proc = scm_c_module_lookup(internal_module(), "%goto-url-hook");
	SCM x;

	if (0 && ses);

	*returl = NULL;

	if (!*url)
		return 0;

	x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(url));
	if (SCM_STRINGP(x)) {
		*returl = stracpy(SCM_STRING_UCHARS(x));
	} else {
		*returl = stracpy("");
	}

	return *returl ? 1 : 0;
}


/*
 * follow_url:
 *
 * This function should return the URL to follow in a dynamically
 * allocated string, or NULL to not follow any URL.
 */

static int
script_hook_follow_url(va_list ap)
{
	unsigned char **returl = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	SCM proc = scm_c_module_lookup(internal_module(), "%follow-url-hook");
	SCM x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(url));

	if (0 && ses);

	if (SCM_STRINGP(x))
		*returl = memacpy(SCM_STRING_UCHARS(x), SCM_STRING_LENGTH(x)+1);
	else
		*returl = stracpy("");

	return *returl ? 1 : 0;
}


/*
 * pre_format_html:
 *
 * This function can return either new content in a dynamically
 * allocated string, or NULL to keep the content unchanged.
 */

static int
script_hook_pre_format_html(va_list ap)
{
	unsigned char **retval = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	unsigned char *html = va_arg(ap, unsigned char *);
	int *len = va_arg(ap, int *);
	SCM proc = scm_c_module_lookup(internal_module(), "%pre-format-html-hook");
	SCM x = scm_call_2(SCM_VARIABLE_REF(proc), scm_makfrom0str(url),
			   scm_mem2string(html, *len));

	if (0 && ses);

	if (SCM_STRINGP(x)) {
		*len = SCM_STRING_LENGTH(x);
		*retval = memacpy(SCM_STRING_UCHARS(x), SCM_STRING_LENGTH(x)+1);
	} else {
		*retval = NULL;
	}

	return *retval ? 1 : 0;
}


/*
 * get_proxy:
 *
 * The Lua function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies
 *
 * The implementor of this function should return one of:
 *  - a dynamically allocated string containing the proxy:port
 *  - an empty string (dynamically allocated!) to use no proxy
 *  - NULL to use default proxies
 */

static int
script_hook_get_proxy(va_list ap)
{
	unsigned char **retval = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	SCM proc = scm_c_module_lookup(internal_module(), "%get-proxy-hook");
	SCM x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(url));

	if (SCM_STRINGP(x))
		*retval = memacpy(SCM_STRING_UCHARS(x), SCM_STRING_LENGTH(x)+1);
	else if (SCM_NULLP(x))
		*retval = stracpy("");
	else
		*retval = NULL;
	return *retval ? 1 : 0;
}


/*
 * This function should allow the user to do whatever clean up is
 * required when Links quits.
 */

static int
script_hook_quit(va_list ap)
{
	SCM proc = scm_c_module_lookup(internal_module(), "%quit-hook");

	scm_call_0(SCM_VARIABLE_REF(proc));

	return 0;
}


struct scripting_hook guile_scripting_hooks[] = {
	{ "goto-url", script_hook_goto_url },
	{ "follow-url", script_hook_follow_url },
	{ "pre-format-html", script_hook_pre_format_html },
	{ "get-proxy", script_hook_get_proxy },
	{ "quit", script_hook_quit },
	{ NULL, NULL }
};

#endif /* HAVE_GUILE */
