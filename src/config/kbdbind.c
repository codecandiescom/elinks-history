/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.9 2002/05/05 14:52:53 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA
#include <lua.h>
#include <lualib.h>
#endif
#include <string.h>

#include <links.h>

#include <config/conf.h>
#include <config/kbdbind.h>
#include <config/options.h>
#include <lowlevel/kbd.h>
#include <lua/lua.h>

#ifndef HAVE_LUA
#define LUA_NOREF	0
#endif

static void add_default_keybindings();

struct keybinding {
	struct keybinding *next;
	struct keybinding *prev;
	enum keyact act;
	long x;
	long y;
	int func_ref;
};

static struct list_head keymaps[KM_MAX];

static void delete_keybinding(enum keymap km, long x, long y)
{
	struct keybinding *kb;
	foreach(kb, keymaps[km]) if (kb->x == x && kb->y == y) {
#ifdef HAVE_LUA
		if (kb->func_ref != LUA_NOREF) lua_unref(lua_state, kb->func_ref);
#endif
		del_from_list(kb);
		mem_free(kb);
		break;
	}
}

static void add_keybinding(enum keymap km, int act, long x, long y, int func_ref)
{
	struct keybinding *kb;
	delete_keybinding(km, x, y);
	if ((kb = mem_alloc(sizeof(struct keybinding)))) {
		kb->act = act;
		kb->x = x;
		kb->y = y;
		kb->func_ref = func_ref;
		add_to_list(keymaps[km], kb);
	}
}

void init_keymaps()
{
    	enum keymap i;
	for (i = 0; i < KM_MAX; i++) init_list(keymaps[i]);
	add_default_keybindings();
}

void free_keymaps()
{
	enum keymap i;
	for (i = 0; i < KM_MAX; i++) free_list(keymaps[i]);
}

int kbd_action(enum keymap kmap, struct event *ev, int *func_ref)
{
	struct keybinding *kb;
	if (ev->ev == EV_KBD) foreach(kb, keymaps[kmap])
		if (ev->x == kb->x && ev->y == kb->y) {
			if (kb->act == ACT_LUA_FUNCTION && func_ref)
				*func_ref = kb->func_ref;
			return kb->act;
		}
	return -1;
}

/*
 * Config file helpers.
 */

struct strtonum {
	unsigned char *str;
	long num;
};

static long strtonum(struct strtonum *table, char *s)
{
	struct strtonum *p;
	for (p = table; p->str; p++)
		if (!strcmp(p->str, s)) return p->num;
	return -1;
}

static int parse_keymap(unsigned char *s)
{
	struct strtonum table[] = {
		{ "main", KM_MAIN },
		{ "edit", KM_EDIT },
		{ "menu", KM_MENU },
		{ NULL, 0 }
	};
	return strtonum(table, s);
}

long parse_key(unsigned char *s)
{
	struct strtonum table[] = {
		{ "Enter", KBD_ENTER },
		{ "Backspace", KBD_BS },
		{ "Tab", KBD_TAB },
		{ "Escape", KBD_ESC },
		{ "Left", KBD_LEFT },
		{ "Right", KBD_RIGHT },
		{ "Up", KBD_UP },
		{ "Down", KBD_DOWN },
		{ "Insert", KBD_INS },
		{ "Delete", KBD_DEL },
		{ "Home", KBD_HOME },
		{ "End", KBD_END },
		{ "PageUp", KBD_PAGE_UP },
		{ "PageDown", KBD_PAGE_DOWN },
		{ "F1", KBD_F1 },
		{ "F2", KBD_F2 },
		{ "F3", KBD_F3 },
		{ "F4", KBD_F4 },
		{ "F5", KBD_F5 },
		{ "F6", KBD_F6 },
		{ "F7", KBD_F7 },
		{ "F8", KBD_F8 },
		{ "F9", KBD_F9 },
		{ "F10", KBD_F10 },
		{ "F11", KBD_F11 },
		{ "F12", KBD_F12 },
		{ NULL, 0 }
	};

	return (strlen(s) == 1) ? *s : strtonum(table, s);
}

static int parse_keystroke(unsigned char *s, long *x, long *y)
{
	*y = 0;
	if (!strncmp(s, "Shift-", 6)) *y |= KBD_SHIFT, s += 6;
	else if (!strncmp(s, "Ctrl-", 5)) *y |= KBD_CTRL, s += 5;
	else if (!strncmp(s, "Alt-", 4)) *y |= KBD_ALT, s += 4;
	return ((*x = parse_key(s)) < 0) ? -1 : 0;
}

static int parse_act(unsigned char *s)
{
	int i;
	/* Please keep this table in alphabetical order, and in sync with
	 * the ACT_* constants in kbdbind.h.  */
	unsigned char *table[] = {
		"add-bookmark",
		"auto-complete",
		"back",
		"backspace",
		"bookmark-manager",
		"cookies-load",
		"copy-clipboard",
		"cut-clipboard",
		"delete",
		"document-info",
		"down",
		"download",
		"edit",
		"end",
		"enter",
		"enter-reload",
		"file-menu",
		"find-next",
		"find-next-back",
		"goto-url",
		"goto-url-current",
		"goto-url-current-link",
		"header-info",
		"history-manager",
		"home",
		"kill-to-bol",
		"kill-to-eol",
		"left",
		"link-menu",
		"lua-console",
		" *lua-function*", /* internal use only */
		"menu",
		"next-frame",
		"open-new-window",
		"open-link-in-new-window",
		"page-down",
		"page-up",
		"paste-clipboard",
		"previous-frame",
		"quit",
		"really-quit",
		"reload",
		"right",
		"scroll-down",
		"scroll-left",
		"scroll-right",
		"scroll-up",
		"search",
		"search-back",
		"toggle-display-images",
		"toggle-display-tables",
		"toggle-html-plain",
		"unback",
		"up",
		"view-image",
		"zoom-frame",
		NULL
	};
	for (i = 0; table[i]; i++) if (!strcmp(table[i], s)) return i;
	return -1;
}

/*
 * Config file readers.
 */

/* bind KEYMAP KEYSTROKE ACTION */
unsigned char *bind_rd(struct option *o, unsigned char *line)
{
	unsigned char *err = NULL;
	unsigned char *ckmap;
	unsigned char *ckey;
	unsigned char *cact;
	int kmap;
	long x, y;
	int act;

	ckmap = get_token(&line);
	ckey = get_token(&line);
	cact = get_token(&line);

	if (!ckmap || !ckey || !cact)
		err = "Missing arguments";
	else if ((kmap = parse_keymap(ckmap)) < 0)
		err = "Unrecognised keymap";
	else if (parse_keystroke(ckey, &x, &y) < 0)
		err = "Error parsing keystroke";
	else if ((act = parse_act(cact)) < 0)
		err = "Unrecognised action";
	else
		add_keybinding(kmap, act, x, y, LUA_NOREF);

	if (cact) mem_free(cact);
	if (ckey) mem_free(ckey);
	if (ckmap) mem_free(ckmap);
	return err;
}

/* unbind KEYMAP KEYSTROKE */
unsigned char *unbind_rd(struct option *o, unsigned char *line)
{
	unsigned char *err = NULL;
	unsigned char *ckmap;
	unsigned char *ckey;
	int kmap;
	long x, y;

	ckmap = get_token(&line);
	ckey = get_token(&line);
	if (!ckmap)
		err = "Missing arguments";
	else if ((kmap = parse_keymap(ckmap)) < 0)
		err = "Unrecognised keymap";
	else if (parse_keystroke(ckey, &x, &y) < 0)
		err = "Error parsing keystroke";
	else
		delete_keybinding(kmap, x, y);

	if (ckey) mem_free(ckey);
	if (ckmap) mem_free(ckmap);
	return err;
}

/*
 * Bind to Lua function.
 */

#ifdef HAVE_LUA
unsigned char *bind_lua_func(unsigned char *ckmap, unsigned char *ckey, int func_ref)
{
	unsigned char *err = NULL;
	int kmap;
	long x, y;
	int act;

	if ((kmap = parse_keymap(ckmap)) < 0)
		err = "Unrecognised keymap";
	else if (parse_keystroke(ckey, &x, &y) < 0)
		err = "Error parsing keystroke";
	else if ((act = parse_act(" *lua-function*")) < 0)
		err = "Unrecognised action (internal error)";
	else
		add_keybinding(kmap, act, x, y, func_ref);

	return err;
}
#endif

/*
 * Default keybindings.
 */

struct default_kb {
	int act;
	long x;
	long y;
};

static struct default_kb default_main_keymap[] = {
	{ ACT_PAGE_DOWN, KBD_PAGE_DOWN, 0 },
	{ ACT_PAGE_DOWN, ' ', 0 },
	{ ACT_PAGE_DOWN, 'F', KBD_CTRL },
	{ ACT_PAGE_UP, KBD_PAGE_UP, 0 },
	{ ACT_PAGE_UP, 'b', 0 },
	{ ACT_PAGE_UP, 'B', 0 },
	{ ACT_PAGE_UP, 'B', KBD_CTRL },
	{ ACT_DOWN, KBD_DOWN, 0 },
	{ ACT_UP, KBD_UP, 0 },
	{ ACT_COPY_CLIPBOARD, KBD_INS, KBD_CTRL },
	{ ACT_COPY_CLIPBOARD, 'C', KBD_CTRL },
	{ ACT_SCROLL_UP, KBD_INS, 0 },
	{ ACT_SCROLL_UP, 'P', KBD_CTRL },
	{ ACT_SCROLL_DOWN, KBD_DEL, 0 },
	{ ACT_SCROLL_DOWN, 'N', KBD_CTRL },
	{ ACT_SCROLL_LEFT, '[', 0 },
	{ ACT_SCROLL_RIGHT, ']', 0 },
	{ ACT_HOME, KBD_HOME, 0 },
	{ ACT_HOME, 'A', KBD_CTRL },
	{ ACT_END, KBD_END, 0 },
	{ ACT_END, 'E', KBD_CTRL },
	{ ACT_ENTER, KBD_RIGHT, 0 },
	{ ACT_ENTER, KBD_ENTER, 0 },
	{ ACT_ENTER_RELOAD, KBD_RIGHT, KBD_CTRL },
	{ ACT_ENTER_RELOAD, KBD_ENTER, KBD_CTRL },
	{ ACT_BACK, KBD_LEFT, 0 },
	{ ACT_UNBACK, 'u', 0 },
    	{ ACT_UNBACK, 'U', 0 },
	{ ACT_DOWNLOAD, 'd', 0 },
	{ ACT_DOWNLOAD, 'D', 0 },
	{ ACT_SEARCH, '/', 0 },
	{ ACT_SEARCH_BACK, '?', 0 },
	{ ACT_FIND_NEXT, 'n', 0 },
	{ ACT_FIND_NEXT_BACK, 'N', 0 },
	{ ACT_ZOOM_FRAME, 'f', 0 },
	{ ACT_ZOOM_FRAME, 'F', 0 },
	{ ACT_RELOAD, 'R', KBD_CTRL },
	{ ACT_GOTO_URL, 'g', 0 },
	{ ACT_GOTO_URL_CURRENT, 'G', 0 },
	{ ACT_ADD_BOOKMARK, 'a' },
	{ ACT_ADD_BOOKMARK, 'A' },
	{ ACT_BOOKMARK_MANAGER, 's' },
	{ ACT_BOOKMARK_MANAGER, 'S' },
	{ ACT_HISTORY_MANAGER, 'h' },
	{ ACT_HISTORY_MANAGER, 'H' },
	{ ACT_COOKIES_LOAD, 'K', KBD_CTRL },
	{ ACT_QUIT, 'q' },
	{ ACT_REALLY_QUIT, 'Q' },
	{ ACT_DOCUMENT_INFO, '=' },
	{ ACT_HEADER_INFO, '|' },
	{ ACT_TOGGLE_HTML_PLAIN, '\\' },
	{ ACT_TOGGLE_DISPLAY_IMAGES, '*' },
	{ ACT_NEXT_FRAME, KBD_TAB },
	{ ACT_MENU, KBD_ESC },
	{ ACT_MENU, KBD_F9 },
	{ ACT_FILE_MENU, KBD_F10 },
	{ ACT_LUA_CONSOLE, ',' },
	{ ACT_LINK_MENU, 'l' },
	{ ACT_LINK_MENU, 'L' },
	{ 0, 0, 0 }
};

static struct default_kb default_edit_keymap[] = {
	{ ACT_LEFT, KBD_LEFT, 0 },
	{ ACT_RIGHT, KBD_RIGHT, 0 },
	{ ACT_HOME, KBD_HOME, 0 },
	{ ACT_HOME, 'A', KBD_CTRL },
	{ ACT_UP, KBD_UP, 0 },
	{ ACT_DOWN, KBD_DOWN, 0 },
	{ ACT_END, KBD_END, 0 },
	{ ACT_END, 'E', KBD_CTRL },
	{ ACT_EDIT, KBD_F4, 0 },
	{ ACT_EDIT, 'T', KBD_CTRL },
	{ ACT_COPY_CLIPBOARD, KBD_INS, KBD_CTRL },
	{ ACT_COPY_CLIPBOARD, 'C', KBD_CTRL },
	{ ACT_CUT_CLIPBOARD, 'X', KBD_CTRL },
	{ ACT_PASTE_CLIPBOARD, 'V', KBD_CTRL },
	{ ACT_ENTER, KBD_ENTER, 0 },
	{ ACT_BACKSPACE, KBD_BS, 0 },
	{ ACT_BACKSPACE, 'H', KBD_CTRL },
	{ ACT_DELETE, KBD_DEL, 0 },
	{ ACT_DELETE, 'D', KBD_CTRL },
	{ ACT_KILL_TO_BOL, 'U', KBD_CTRL },
	{ ACT_KILL_TO_EOL, 'K', KBD_CTRL },
    	{ ACT_AUTO_COMPLETE, 'W', KBD_CTRL },
	{ 0, 0, 0 }
};

static struct default_kb default_menu_keymap[] = {
	{ ACT_LEFT, KBD_LEFT, 0 },
	{ ACT_RIGHT, KBD_RIGHT, 0 },
	{ ACT_HOME, KBD_HOME, 0 },
	{ ACT_HOME, 'A', KBD_CTRL },
	{ ACT_UP, KBD_UP, 0 },
	{ ACT_DOWN, KBD_DOWN, 0 },
	{ ACT_END, KBD_END, 0 },
	{ ACT_END, 'E', KBD_CTRL },
	{ ACT_ENTER, KBD_ENTER, 0 },
	{ ACT_PAGE_DOWN, KBD_PAGE_DOWN, 0 },
	{ ACT_PAGE_DOWN, 'F', KBD_CTRL },
	{ ACT_PAGE_UP, KBD_PAGE_UP, 0 },
	{ ACT_PAGE_UP, 'B', KBD_CTRL },
	{ 0, 0, 0}
};

static void add_default_keybindings()
{
	struct default_kb *kb;
	for (kb = default_main_keymap; kb->x; kb++) add_keybinding(KM_MAIN, kb->act, kb->x, kb->y, LUA_NOREF);
    	for (kb = default_edit_keymap; kb->x; kb++) add_keybinding(KM_EDIT, kb->act, kb->x, kb->y, LUA_NOREF);
    	for (kb = default_menu_keymap; kb->x; kb++) add_keybinding(KM_MENU, kb->act, kb->x, kb->y, LUA_NOREF);
}
