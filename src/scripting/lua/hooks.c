/* Lua scripting hooks */
/* $Id: hooks.c,v 1.38 2003/09/25 19:44:37 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA

#include "elinks.h"

#include "sched/event.h"
#include "sched/session.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"
#include "scripting/scripting.h"
#include "util/string.h"


/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

static enum evhook_status
script_hook_goto_url(va_list ap)
{
	lua_State *L = lua_state;
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	int status;

	lua_getglobal(L, "goto_url_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EHS_NEXT;
	}

	lua_pushstring(L, *url);

	if (!have_location(ses)) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, cur_loc(ses)->vs.url);
	}

	if (prepare_lua(ses)) return EHS_NEXT;

	status = lua_call(L, 2, 1);
	finish_lua();
	if (status) return EHS_NEXT;

	if (lua_isstring(L, -1)) {
		*url = stracpy((unsigned char *) lua_tostring(L, -1));
		status = EHS_LAST;
	} else if (lua_isnil(L, -1)) {
		*url = NULL;
		status = EHS_LAST;
	} else {
		alert_lua_error("goto_url_hook must return a string or nil");
		status = EHS_NEXT;
	}

	lua_pop(L, 1);

	return status;
}

static enum evhook_status
script_hook_follow_url(va_list ap)
{
	lua_State *L = lua_state;
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	int status;

	lua_getglobal(L, "follow_url_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EHS_NEXT;
	}

	lua_pushstring(L, *url);

	if (prepare_lua(ses)) return EHS_NEXT;

	status = lua_call(L, 1, 1);
	finish_lua();
	if (status) return EHS_NEXT;

	if (lua_isstring(L, -1)) {
		*url = stracpy((unsigned char *) lua_tostring(L, -1));
		status = EHS_LAST;
	} else if (lua_isnil(L, -1)) {
		*url = NULL;
		status = EHS_LAST;
	} else {
		alert_lua_error("follow_url_hook must return a string or nil");
		status = EHS_NEXT;
	}

	lua_pop(L, 1);

	return status;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap)
{
	lua_State *L = lua_state;
	unsigned char **html = va_arg(ap, unsigned char **);
	int *html_len = va_arg(ap, int *);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	int status;

	lua_getglobal(L, "pre_format_html_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EHS_NEXT;
	}

	lua_pushstring(L, url);
	lua_pushlstring(L, *html, *html_len);

	if (prepare_lua(ses)) return EHS_NEXT;

	status = lua_call(L, 2, 1);
	finish_lua();
	if (status) return EHS_NEXT;

	if (lua_isstring(L, -1)) {
		*html_len = lua_strlen(L, -1);
		*html = memacpy((unsigned char *) lua_tostring(L, -1), *html_len);
		status = EHS_LAST;
	} else if (!lua_isnil(L, -1)) {
		alert_lua_error("pre_format_html_hook must return a string or nil");
		status = EHS_NEXT;
	}

	lua_pop(L, 1);

	return status;
}

/* The Lua function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies */
static enum evhook_status
script_hook_get_proxy(va_list ap)
{
	lua_State *L = lua_state;
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	int status;

	lua_getglobal(L, "proxy_for_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EHS_NEXT;
	}

	lua_pushstring(L, url);
	if (prepare_lua(NULL)) return EHS_NEXT;

	status = lua_call(L, 1, 1);
	finish_lua();
	if (status) return EHS_NEXT;

	if (lua_isstring(L, -1)) {
		*new_proxy_url = stracpy((unsigned char *)lua_tostring(L, -1));
		status = EHS_LAST;
	} else if (lua_isnil(L, -1)) {
		*new_proxy_url = NULL;
		status = EHS_LAST;
	} else {
		alert_lua_error("proxy_hook must return a string or nil");
		status = EHS_NEXT;
	}

	lua_pop(L, 1);

	return status;
}

static enum evhook_status
script_hook_quit(va_list ap)
{
	if (!prepare_lua(NULL)) {
		lua_dostring(lua_state, "if quit_hook then quit_hook() end");
		finish_lua();
	}

	return 0;
}

struct scripting_hook lua_scripting_hooks[] = {
	{ "goto-url", script_hook_goto_url },
	{ "follow-url", script_hook_follow_url },
	{ "pre-format-html", script_hook_pre_format_html },
	{ "get-proxy", script_hook_get_proxy },
	{ "quit", script_hook_quit },
	{ NULL, NULL }
};

#endif
