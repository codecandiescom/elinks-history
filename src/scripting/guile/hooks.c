/* Guile scripting hooks */
/* $Id: hooks.c,v 1.1 2003/07/24 15:33:33 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GUILE

#include <libguile.h>

#include "elinks.h"

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

unsigned char *
script_hook_goto_url(struct session *ses, unsigned char *url)
{
	SCM proc = scm_c_module_lookup(internal_module(), "%goto-url-hook");
	SCM x;

	if (!*url)
		return NULL;
	
	x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(url));
	if (SCM_STRINGP(x)) {
		return stracpy(SCM_STRING_UCHARS(x));
	} else {
		return stracpy("");
	}
}


/*
 * follow_url:
 *
 * This function should return the URL to follow in a dynamically
 * allocated string, or NULL to not follow any URL.
 */

unsigned char *
script_hook_follow_url(struct session *ses, unsigned char *url)
{
	SCM proc = scm_c_module_lookup(internal_module(), "%follow-url-hook");
	SCM x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(url));

	if (SCM_STRINGP(x))
		return memacpy(SCM_STRING_UCHARS(x), SCM_STRING_LENGTH(x)+1);
	else
		return stracpy("");
}


/*
 * pre_format_html:
 *
 * This function can return either new content in a dynamically
 * allocated string, or NULL to keep the content unchanged.
 */

unsigned char *
script_hook_pre_format_html(struct session *ses, unsigned char *url,
			    unsigned char *html, int *len)
{
	SCM proc = scm_c_module_lookup(internal_module(), "%pre-format-html-hook");
	SCM x = scm_call_2(SCM_VARIABLE_REF(proc), scm_makfrom0str(url),
			   scm_mem2string(html, *len));

	if (SCM_STRINGP(x)) {
		*len = SCM_STRING_LENGTH(x);
		return memacpy(SCM_STRING_UCHARS(x), SCM_STRING_LENGTH(x)+1);
	} else {
		return NULL;
	}
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

unsigned char *
script_hook_get_proxy(unsigned char *url)
{
	SCM proc = scm_c_module_lookup(internal_module(), "%get-proxy-hook");
	SCM x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(url));

	if (SCM_STRINGP(x))
		return memacpy(SCM_STRING_UCHARS(x), SCM_STRING_LENGTH(x)+1);
	else if (SCM_NULLP(x))
		return stracpy("");
	else
		return NULL;
}


/*
 * This function should allow the user to do whatever clean up is
 * required when Links quits.
 */

void
script_hook_quit(void)
{
	SCM proc = scm_c_module_lookup(internal_module(), "%quit-hook");

	scm_call_0(SCM_VARIABLE_REF(proc));
}

#endif /* HAVE_GUILE */
