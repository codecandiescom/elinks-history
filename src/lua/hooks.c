/* Lua scripting hooks */
/* $Id: hooks.c,v 1.10 2003/06/12 01:15:24 miciah Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA

#include "elinks.h"

#include "lua/core.h"
#include "lua/hooks.h"
#include "sched/session.h"
#include "util/string.h"


/* Theoretically, you should be able to implement a links-lua style
 * interface with another scripting language library by reimplementing
 * these functions.  In practice... not likely. --pw */

/* The "in practice" part ought to be changed. We need to continue in
 * generalization of scripting and moving everything Lua-related to lua/.
 * --pasky */

/*
 * goto_url:
 *
 * This function can do one of the following:
 *  - call goto_url on the url given;
 *  - call goto_url on some other url;
 *  - do nothing.
 */

void
script_hook_goto_url(struct session *ses, unsigned char *url)
{
	lua_State *L = lua_state;
	int err;

	lua_getglobal(L, "goto_url_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		goto_url(ses, url);
		return;
	}

	lua_pushstring(L, url);
	if (!have_location(ses))
		lua_pushnil(L);
	else
		lua_pushstring(L, cur_loc(ses)->vs.url);

	if (prepare_lua(ses))
		return;
	err = lua_call(L, 2, 1);
	finish_lua();
	if (err)
		return;

	if (lua_isstring(L, -1))
		goto_url(ses, (unsigned char *) lua_tostring(L, -1));
	else if (!lua_isnil(L, -1))
		alert_lua_error("goto_url_hook must return a string or nil");
	lua_pop(L, 1);
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
	lua_State *L = lua_state;
	unsigned char *s = NULL;
	int err;

	lua_getglobal(L, "follow_url_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return stracpy(url);
	}

	lua_pushstring(L, url);
	if (prepare_lua(ses))
		return NULL;
	err = lua_call(L, 1, 1);
	finish_lua();
	if (err)
		return NULL;

	if (lua_isstring(L, -1)) {
		int len = lua_strlen(L, -1);

		s = memacpy((unsigned char *)lua_tostring(L, -1), len);
	}
	else if (!lua_isnil(L, -1))
		alert_lua_error("follow_url_hook must return a string or nil");

	lua_pop(L, 1);
	return s;
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
	lua_State *L = lua_state;
	unsigned char *s = NULL;
	int err;

	lua_getglobal(L, "pre_format_html_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return NULL;
	}

	lua_pushstring(L, url);
	lua_pushlstring(L, html, *len);

	if (prepare_lua(ses))
		return NULL;
	err = lua_call(L, 2, 1);
	finish_lua();
	if (err)
		return NULL;

	if (lua_isstring(L, -1)) {
		*len = lua_strlen(L, -1);
		s = memacpy((unsigned char *) lua_tostring(L, -1), *len);
	}
	else if (!lua_isnil(L, -1))
		alert_lua_error("pre_format_html_hook must return a string or nil");
	lua_pop(L, 1);
	return s;
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
	lua_State *L = lua_state;
	char *ret = NULL;
	int err;

	lua_getglobal(L, "proxy_for_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return NULL;
	}

	lua_pushstring(L, url);
	if (prepare_lua(NULL))
		return NULL;
	err = lua_call(L, 1, 1);
	finish_lua();
	if (err)
		return NULL;

	if (lua_isstring(L, -1))
		ret = stracpy((unsigned char *)lua_tostring(L, -1));
	else if (!lua_isnil(L, -1))
		alert_lua_error("proxy_hook must return a string or nil");
	lua_pop(L, 1);
	return ret;
}


/*
 * This function should allow the user to do whatever clean up is
 * required when Links quits.
 */

void
script_hook_quit(void)
{
	if (prepare_lua(NULL) == 0) {
		lua_dostring(lua_state, "if quit_hook then quit_hook() end");
		finish_lua();
	}
}


#endif
