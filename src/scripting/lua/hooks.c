/* Lua scripting hooks */
/* $Id: hooks.c,v 1.26 2003/09/23 00:05:18 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA

#include "elinks.h"

#include "sched/event.h"
#include "sched/session.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"
#include "util/string.h"


/* Theoretically, you should be able to implement a links-lua style
 * interface with another scripting language library by reimplementing
 * these functions.  In practice... not likely. --pw */

/* The "in practice" part ought to be changed. We need to continue in
 * generalization of scripting and moving everything Lua-related to lua/.
 * --pasky */

static inline int
str_event_code(unsigned char **retval, unsigned char *value)
{
	*retval = value;
	return !!value;
}

/* Triggerred when user entered something into the goto URL dialog. */
/* Returns NULL if the original @url should be followed, or dynamically
 * allocated new URL to be followed instead ("" means that no URL should be
 * followed at all). */
static int
script_hook_goto_url(va_list ap)
{
	lua_State *L = lua_state;
	unsigned char **returl = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	unsigned char *value = NULL;

	lua_getglobal(L, "goto_url_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return str_event_code(returl, NULL);
	}

	lua_pushstring(L, url);

	if (!have_location(ses)) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, cur_loc(ses)->vs.url);
	}

	if (!prepare_lua(ses)) {
		int err = lua_call(L, 2, 1);

		finish_lua();
		if (err) return str_event_code(returl, NULL);

		if (lua_isstring(L, -1)) {
			value = stracpy((unsigned char *) lua_tostring(L, -1));
		} else if (!lua_isnil(L, -1)) {
			alert_lua_error("goto_url_hook must return a string or nil");
		}

		lua_pop(L, 1);
	}

	return str_event_code(returl, value);
}


/* Triggerred when user decided to let some document to load (followed a link,
 * entered URL in the goto URL dialog, is loading frames from a frameset (?)
 * or whatever). */
/* Returns NULL if the original @url should be followed, or dynamically
 * allocated new URL to be followed instead ("" means that no URL should be
 * followed at all). */
static int
script_hook_follow_url(va_list ap)
{
	lua_State *L = lua_state;
	unsigned char **returl = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	unsigned char *value = NULL;

	lua_getglobal(L, "follow_url_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return str_event_code(returl, NULL);
	}

	lua_pushstring(L, url);

	if (!prepare_lua(ses)) {
		int err = lua_call(L, 1, 1);

		finish_lua();
		if (err) return str_event_code(returl, NULL);

		if (lua_isstring(L, -1)) {
			value = stracpy((unsigned char *) lua_tostring(L, -1));
		} else if (!lua_isnil(L, -1)) {
			alert_lua_error("follow_url_hook must return a string or nil");
		}

		lua_pop(L, 1);
	}

	return str_event_code(returl, value);
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
	lua_State *L = lua_state;
	unsigned char **retval = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	unsigned char *html = va_arg(ap, unsigned char *);
	int *len = va_arg(ap, int *);
	unsigned char *value = NULL;

	lua_getglobal(L, "pre_format_html_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return str_event_code(retval, NULL);
	}

	lua_pushstring(L, url);
	lua_pushlstring(L, html, *len);

	if (!prepare_lua(ses)) {
		int err = lua_call(L, 2, 1);

		finish_lua();
		if (err) return str_event_code(retval, NULL);

		if (lua_isstring(L, -1)) {
			*len = lua_strlen(L, -1);
			*retval = memacpy((unsigned char *) lua_tostring(L, -1), *len);
		} else if (!lua_isnil(L, -1)) {
			alert_lua_error("pre_format_html_hook must return a string or nil");
		}

		lua_pop(L, 1);
	}

	return str_event_code(retval, value);
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
	lua_State *L = lua_state;
	unsigned char **retval = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	unsigned char *value = NULL;

	lua_getglobal(L, "proxy_for_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return str_event_code(retval, NULL);
	}

	lua_pushstring(L, url);
	if (!prepare_lua(NULL)) {
		int err = lua_call(L, 1, 1);

		finish_lua();
		if (err) return str_event_code(retval, NULL);

		if (lua_isstring(L, -1)) {
			value = stracpy((unsigned char *)lua_tostring(L, -1));
		} else if (!lua_isnil(L, -1)) {
			alert_lua_error("proxy_hook must return a string or nil");
		}

		lua_pop(L, 1);
	}

	return str_event_code(retval, value);
}


/* This function should allow the user to do whatever clean up is
 * required when ELinks quits. */
static int
script_hook_quit(va_list ap)
{
	if (!prepare_lua(NULL)) {
		lua_dostring(lua_state, "if quit_hook then quit_hook() end");
		finish_lua();
	}

	return 0;
}

/* TODO: Generalize and share the hook registration among the backends. */
static struct {
	unsigned char *name;
	int (*callback)(va_list ap);
} hooks[] = {
	{ "goto-url", script_hook_goto_url },
	{ "follow-url", script_hook_follow_url },
	{ "pre-format-html", script_hook_pre_format_html },
	{ "get-proxy", script_hook_get_proxy },
	{ "quit", script_hook_quit },
	{ NULL, NULL }
};

void
register_lua_hooks(void)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id;

		id = register_event(hooks[i].name);
		if (id >= 0)
			register_event_hook(id, hooks[i].callback, 0);
	}
}

void
unregister_lua_hooks(void)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id;

		id = get_event_id(hooks[i].name);
		if (id >= 0)
			unregister_event_hook(id, hooks[i].callback);
	}
}

#endif
