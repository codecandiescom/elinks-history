/* Guile scripting hooks */
/* $Id: hooks.c,v 1.5 2003/09/25 00:50:20 jonas Exp $ */

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

/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

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

/* The Guile function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies */
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
