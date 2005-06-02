/* Python scripting hooks */
/* $Id: hooks.c,v 1.1 2005/06/02 18:01:34 witekfl Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/uri.h"
#include "sched/event.h"
#include "sched/location.h"
#include "sched/session.h"
#include "scripting/python/core.h"
#include "scripting/python/hooks.h"
#include "util/string.h"

/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

static void
do_script_hook_goto_url(struct session *ses, unsigned char **url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "goto_url_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pArg = PyTuple_New(1);
		PyObject *pValue;
		const unsigned char *str;

		if (!ses || !have_location(ses)) {
			pValue = Py_None;
		} else {
			unsigned char *uri = struri(cur_loc(ses)->vs.uri);

			pValue = PyString_FromString(uri);
		}
		PyTuple_SetItem(pArg, 0, pValue);
		pValue = PyObject_CallObject(pFunc, pArg);
		Py_DECREF(pArg);
		PyArg_ParseTuple(pValue, "s", &str);
		
		if (str) {
			unsigned char *new_url = stracpy(str);

			if (new_url) mem_free_set(url, new_url);
		}
		Py_DECREF(pValue);
	}
}

static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);

	if (*url)
		do_script_hook_goto_url(ses, url);

	return EVENT_HOOK_STATUS_NEXT;
}

static void
do_script_hook_follow_url(unsigned char **url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "follow_url_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pArg = PyTuple_New(1);
		PyObject *pValue;
		const unsigned char *str;

		pValue = PyString_FromString(*url);
		PyTuple_SetItem(pArg, 0, pValue);
		pValue = PyObject_CallObject(pFunc, pArg);
		Py_DECREF(pArg);
		PyArg_ParseTuple(pValue, "s", &str);
		
		if (str) {
			unsigned char *new_url = stracpy(str);

			if (new_url) mem_free_set(url, new_url);
		}
		Py_DECREF(pValue);
	}
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);

	if (*url)
		do_script_hook_follow_url(url);

	return EVENT_HOOK_STATUS_NEXT;
}

static void
do_script_hook_pre_format_html(unsigned char *url, unsigned char **html,
			       int *html_len)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "pre_format_html_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pArgs = PyTuple_New(2);
		PyObject *pValue;
		const unsigned char *str;

		pValue = PyString_FromString(url);
		PyTuple_SetItem(pArgs, 0, pValue);
		pValue = PyString_FromString(*html);
		PyTuple_SetItem(pArgs, 1, pValue);
		pValue = PyObject_CallObject(pFunc, pArgs);
		Py_DECREF(pArgs);
		PyArg_ParseTuple(pValue, "s", &str);
		
		if (str) {
			*html_len = strlen(str);
			*html = memacpy(str, *html_len);
			if (!html) *html_len = 0;
		}
		Py_DECREF(pValue);
	}
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	unsigned char **html = va_arg(ap, unsigned char **);
	int *html_len = va_arg(ap, int *);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);

	if (ses && url && *html && *html_len)
		do_script_hook_pre_format_html(url, html, html_len);

	return EVENT_HOOK_STATUS_NEXT;
}

static inline void
do_script_hook_get_proxy(unsigned char **new_proxy_url, unsigned char *url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "proxy_for_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pArg = PyTuple_New(1);
		PyObject *pValue;
		const unsigned char *str;

		pValue = PyString_FromString(*url);
		PyTuple_SetItem(pArg, 0, pValue);
		pValue = PyObject_CallObject(pFunc, pArg);
		Py_DECREF(pArg);
		PyArg_ParseTuple(pValue, "s", &str);
		
		if (str) {
			unsigned char *new_url = stracpy(str);

			if (new_url) mem_free_set(*new_proxy_url, new_url);
		} else {
			mem_free_set(*new_proxy_url, NULL);
		}
		Py_DECREF(pValue);
	}
}

static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);

	if (new_proxy_url && url)
		do_script_hook_get_proxy(new_proxy_url, url);

	return EVENT_HOOK_STATUS_NEXT;
}

static void
do_script_hook_quit(void)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "quit_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject_CallObject(pFunc, Py_None);
	}
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	do_script_hook_quit();
	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info python_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_goto_url, NULL },
	{ "follow-url", 0, script_hook_follow_url, NULL },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },
	{ "get-proxy", 0, script_hook_get_proxy, NULL },
	{ "quit", 0, script_hook_quit, NULL },
	NULL_EVENT_HOOK_INFO,
};
