/* Ruby scripting hooks */
/* $Id: hooks.c,v 1.14 2005/06/14 12:25:21 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ruby.h>

#include "elinks.h"

#include "main/event.h"
#include "protocol/uri.h"
#include "scripting/ruby/core.h"
#include "scripting/ruby/hooks.h"
#include "session/location.h"
#include "session/session.h"
#include "util/string.h"


/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

/* We need to catch and handle errors because, otherwise, Ruby will kill us. */

struct erb_protect_info {
	unsigned char *name;
	int argc;
	VALUE *args;
};

static VALUE
do_erb_protected_method_call(VALUE data)
{
	struct erb_protect_info *info = (struct erb_protect_info *) data;
	ID method_id;

	assert(info);

	method_id = rb_intern(info->name);

	return rb_funcall3(erb_module, method_id, info->argc, info->args);
}

static VALUE
erb_protected_method_call(unsigned char *name, int argc, VALUE *args, int *error)
{
	struct erb_protect_info info = { name, argc, args };

	return rb_protect(do_erb_protected_method_call, (VALUE) &info, error);
}



static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	int error;
	VALUE args[2];
	VALUE result;

	if (*url == NULL)
		return EVENT_HOOK_STATUS_NEXT;

	args[0] = rb_str_new2(*url);

	if (!ses || !have_location(ses)) {
		args[1] = Qnil;
	} else {
		args[1] = rb_str_new2(struri(cur_loc(ses)->vs.uri));
	}

	result = erb_protected_method_call("goto_url_hook", 2, args, &error);
	if (error) {
		erb_report_error(ses, error);
		return EVENT_HOOK_STATUS_NEXT;
	}

	switch (rb_type(result)) {
	case T_STRING:
	{
		unsigned char *new_url;

		new_url = memacpy(RSTRING(result)->ptr, RSTRING(result)->len);
		if (new_url) {
			mem_free_set(url, new_url);
		}
		break;
	}
	case T_NIL:
		break;

	default:
		alert_ruby_error(ses, "goto_url_hook must return a string or nil");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	int error;
	VALUE args[1];
	VALUE result;

	evhook_use_params(url && ses);

	if (*url == NULL)
		return EVENT_HOOK_STATUS_NEXT;

	args[0] = rb_str_new2(*url);

	result = erb_protected_method_call("follow_url_hook", 1, args, &error);
	if (error) {
		erb_report_error(ses, error);
		return EVENT_HOOK_STATUS_NEXT;
	}

	switch (rb_type(result)) {
	case T_STRING:
	{
		unsigned char *new_url;

		new_url = memacpy(RSTRING(result)->ptr, RSTRING(result)->len);
		if (new_url) {
			mem_free_set(url, new_url);
		}
		break;
	}
	case T_NIL:
		break;

	default:
		alert_ruby_error(ses, "follow_url_hook must return a string or nil");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	unsigned char **html = va_arg(ap, unsigned char **);
	int *html_len = va_arg(ap, int *);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	int error;
	VALUE args[2];
	VALUE result;

	evhook_use_params(url && ses);

	if (*html == NULL || *html_len == 0)
		return EVENT_HOOK_STATUS_NEXT;

	args[0] = rb_str_new2(url);
	/* FIXME: Use html_len */
	args[1] = rb_str_new2(*html);

	result = erb_protected_method_call("pre_format_html_hook", 2, args, &error);
	if (error) {
		erb_report_error(ses, error);
		return EVENT_HOOK_STATUS_NEXT;
	}

	switch (rb_type(result)) {
	case T_STRING:
	{
		unsigned char *new_html;

		new_html = memacpy(RSTRING(result)->ptr, RSTRING(result)->len);
		if (new_html) {
			*html_len = RSTRING(result)->len;
			*html = new_html;
		}
		break;
	}
	case T_NIL:
		break;

	default:
		alert_ruby_error(ses, "pre_format_html_hook must return a string or nil");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

/* The Ruby function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies */
static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	int error;
	VALUE args[1];
	VALUE result;

	if (!new_proxy_url || !url)
		return EVENT_HOOK_STATUS_NEXT;

	args[0] = rb_str_new2(url);

	result = erb_protected_method_call("proxy_hook", 1, args, &error);
	if (error) {
		erb_report_error(NULL, error);
		return EVENT_HOOK_STATUS_NEXT;
	}

	switch (rb_type(result)) {
	case T_STRING:
	{
		unsigned char *proxy;

		proxy = memacpy(RSTRING(result)->ptr, RSTRING(result)->len);
		if (proxy) {
			mem_free_set(new_proxy_url, proxy);
		}
		break;
	}
	case T_NIL:
		break;

	default:
		alert_ruby_error(NULL, "proxy_hook must return a string or nil");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	VALUE args[1];
	int error;

	erb_protected_method_call("quit_hook", 0, args, &error);
	if (error)
		erb_report_error(NULL, error);

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info ruby_scripting_hooks[] = {
	{ "goto-url",        0, script_hook_goto_url,        NULL },
	{ "follow-url",      0, script_hook_follow_url,      NULL },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },
	{ "get-proxy",       0, script_hook_get_proxy,       NULL },
	{ "quit",            0, script_hook_quit,            NULL },

	NULL_EVENT_HOOK_INFO,
};
