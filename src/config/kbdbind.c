/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.35 2002/08/08 20:46:53 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "lowlevel/kbd.h"
#include "lua/core.h"
#include "util/memory.h"

#ifndef HAVE_LUA
#define LUA_NOREF	0
#endif

#define table table_dirty_workaround_for_name_clash_with_libraries_on_macos


static struct list_head keymaps[KM_MAX];

static void add_default_keybindings();

static void delete_keybinding(enum keymap, long, long);

static int read_action(unsigned char *);


static void
add_keybinding(enum keymap km, int action, long key, long meta, int func_ref)
{
	struct keybinding *kb;

	delete_keybinding(km, key, meta);

	kb = mem_alloc(sizeof(struct keybinding));
	if (kb) {
		kb->action = action;
		kb->key = key;
		kb->meta = meta;
		kb->func_ref = func_ref;
		kb->watermark = 0;
		add_to_list(keymaps[km], kb);
	}
}

static void
delete_keybinding(enum keymap km, long key, long meta)
{
	struct keybinding *kb;

	foreach(kb, keymaps[km]) {
		if (kb->key != key || kb->meta != meta)
			continue;

#ifdef HAVE_LUA
		if (kb->func_ref != LUA_NOREF)
			lua_unref(lua_state, kb->func_ref);
#endif
		del_from_list(kb);
		mem_free(kb);
		break;
	}
}


void
init_keymaps()
{
    	enum keymap i;

	for (i = 0; i < KM_MAX; i++)
		init_list(keymaps[i]);

	add_default_keybindings();
}

void
free_keymaps()
{
	enum keymap i;

	for (i = 0; i < KM_MAX; i++)
		free_list(keymaps[i]);
}


int
kbd_action(enum keymap kmap, struct event *ev, int *func_ref)
{
	struct keybinding *kb;

	if (ev->ev != EV_KBD) return -1;

	kb = kbd_ev_lookup(kmap, ev->x, ev->y, func_ref);
	return kb ? kb->action : -1;
}

struct keybinding *
kbd_ev_lookup(enum keymap kmap, long key, long meta, int *func_ref)
{
	struct keybinding *kb;

	foreach(kb, keymaps[kmap]) {
		if (key != kb->key || meta != kb->meta)
			continue;

		if (kb->action == ACT_LUA_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_nm_lookup(enum keymap kmap, unsigned char *name, int *func_ref)
{
	struct keybinding *kb;
	enum keyact act = read_action(name);

	if (act < 0) return NULL;

	foreach(kb, keymaps[kmap]) {
		if (act != kb->action)
			continue;

		if (kb->action == ACT_LUA_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

/*
 * Config file helpers.
 */

struct strtonum {
	unsigned char *str;
	long num;
};

static long
strtonum(struct strtonum *table, char *str)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (!strcmp(rec->str, str))
			return rec->num;

	return -1;
}

static unsigned char *
numtostr(struct strtonum *table, long num)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (num == rec->num)
			return rec->str;

	return NULL;
}


static struct strtonum keymap_table[] = {
	{ "main", KM_MAIN },
	{ "edit", KM_EDIT },
	{ "menu", KM_MENU },
	{ NULL, 0 }
};

static int
read_keymap(unsigned char *keymap)
{

	return strtonum(keymap_table, keymap);
}

static unsigned char *
write_keymap(enum keymap keymap)
{
	return numtostr(keymap_table, keymap);
}


static struct strtonum key_table[] = {
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

long
read_key(unsigned char *key)
{
	return (strlen(key) == 1) ? *key : strtonum(key_table, key);
}

static unsigned char *
write_key(long key)
{
	static unsigned char dirty[3];
	unsigned char *bin = numtostr(key_table, key);

	dirty[0] = (unsigned char) key;
	if (key == '\\')
		dirty[1] = '\\', dirty[2] = '\0';
	else
		dirty[1] = '\0';
	
	return bin ? bin : dirty;
}


static int
parse_keystroke(unsigned char *s, long *key, long *meta)
{
	*meta = 0;
	if (!strncmp(s, "Shift-", 6)) {
		*meta |= KBD_SHIFT;
		s += 6;
	} else if (!strncmp(s, "Ctrl-", 5)) {
		*meta |= KBD_CTRL;
		s += 5;
	} else if (!strncmp(s, "Alt-", 4)) {
		*meta |= KBD_ALT;
		s += 4;
	}

	*key = read_key(s);
	return (*key < 0) ? -1 : 0;
}

static void
make_keystroke(unsigned char **str, int *len, long key, long meta)
{
	if (meta & KBD_SHIFT)
		add_to_str(str, len, "Shift-");
	if (meta & KBD_CTRL)
		add_to_str(str, len, "Ctrl-");
	if (meta & KBD_ALT)
		add_to_str(str, len, "Alt-");

	add_to_str(str, len, write_key(key));
}


/* Please keep this table in alphabetical order, and in sync with
 * the ACT_* constants in kbdbind.h.  */
static struct strtonum action_table[] = {
	{ "none", ACT_NONE },
	{ "add-bookmark", ACT_ADD_BOOKMARK },
	{ "auto-complete", ACT_AUTO_COMPLETE },
	{ "auto-complete-unambiguous", ACT_AUTO_COMPLETE_UNAMBIGUOUS },
	{ "back", ACT_BACK },
	{ "backspace", ACT_BACKSPACE },
	{ "bookmark-manager", ACT_BOOKMARK_MANAGER },
	{ "cookies-load", ACT_COOKIES_LOAD },
	{ "copy-clipboard", ACT_COPY_CLIPBOARD },
	{ "cut-clipboard", ACT_CUT_CLIPBOARD },
	{ "delete", ACT_DELETE },
	{ "document-info", ACT_DOCUMENT_INFO },
	{ "down", ACT_DOWN },
	{ "download", ACT_DOWNLOAD },
	{ "download-image", ACT_DOWNLOAD_IMAGE },
	{ "edit", ACT_EDIT },
	{ "end", ACT_END },
	{ "enter", ACT_ENTER },
	{ "enter-reload", ACT_ENTER_RELOAD },
	{ "file-menu", ACT_FILE_MENU },
	{ "find-next", ACT_FIND_NEXT },
	{ "find-next-back", ACT_FIND_NEXT_BACK },
	{ "forget-credentials", ACT_FORGET_CREDENTIALS },
	{ "goto-url", ACT_GOTO_URL },
	{ "goto-url-current", ACT_GOTO_URL_CURRENT },
	{ "goto-url-current-link", ACT_GOTO_URL_CURRENT_LINK },
	{ "goto-url-home", ACT_GOTO_URL_HOME },
	{ "header-info", ACT_HEADER_INFO },
	{ "history-manager", ACT_HISTORY_MANAGER },
	{ "home", ACT_HOME },
	{ "kill-to-bol", ACT_KILL_TO_BOL },
	{ "kill-to-eol", ACT_KILL_TO_EOL },
	{ "left", ACT_LEFT },
	{ "link-menu", ACT_LINK_MENU },
	{ "jump-to-link", ACT_JUMP_TO_LINK },
	{ "follow-link", ACT_FOLLOW_LINK },
	{ "lua-console", ACT_LUA_CONSOLE },
	{ " *lua-function*", ACT_LUA_FUNCTION }, /* internal use only */
	{ "menu", ACT_MENU },
	{ "next-frame", ACT_NEXT_FRAME },
	{ "open-new-window", ACT_OPEN_NEW_WINDOW },
	{ "open-link-in-new-window", ACT_OPEN_LINK_IN_NEW_WINDOW },
	{ "page-down", ACT_PAGE_DOWN },
	{ "page-up", ACT_PAGE_UP },
	{ "paste-clipboard", ACT_PASTE_CLIPBOARD },
	{ "previous-frame", ACT_PREVIOUS_FRAME },
	{ "quit", ACT_QUIT },
	{ "really-quit", ACT_REALLY_QUIT },
	{ "reload", ACT_RELOAD },
	{ "right", ACT_RIGHT },
	{ "save-formatted", ACT_SAVE_FORMATTED },
	{ "scroll-down", ACT_SCROLL_DOWN },
	{ "scroll-left", ACT_SCROLL_LEFT },
	{ "scroll-right", ACT_SCROLL_RIGHT },
	{ "scroll-up", ACT_SCROLL_UP },
	{ "search", ACT_SEARCH },
	{ "search-back", ACT_SEARCH_BACK },
	{ "toggle-display-images", ACT_TOGGLE_DISPLAY_IMAGES },
	{ "toggle-display-tables", ACT_TOGGLE_DISPLAY_TABLES },
	{ "toggle-html-plain", ACT_TOGGLE_HTML_PLAIN },
	{ "unback", ACT_UNBACK },
	{ "up", ACT_UP },
	{ "view-image", ACT_VIEW_IMAGE },
	{ "zoom-frame", ACT_ZOOM_FRAME },
	{ NULL, 0 }
};

static int
read_action(unsigned char *action)
{
	return strtonum(action_table, action);
}

static unsigned char *
write_action(int action)
{

	return numtostr(action_table, action);
}


/*
 * Config file readers.
 */

/* Return 0 when ok, something strange otherwise. */
int
bind_do(unsigned char *keymap, unsigned char *keystroke, unsigned char *action)
{
	int keymap_, action_;
	long key_, meta_;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0) return 1;

	if (parse_keystroke(keystroke, &key_, &meta_) < 0) return 2;

	action_ = read_action(action);
	if (action_ < 0) return 3;

	add_keybinding(keymap_, action_, key_, meta_, LUA_NOREF);
	return 0;
}

void
bind_act(unsigned char **str, int *len, unsigned char *keymap,
	 unsigned char *keystroke)
{
	int keymap_;
	long key_, meta_;
	unsigned char *action;
	struct keybinding *kb;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0) {
fail:
		add_to_str(str, len, "\"\"");
		return;
	}

	if (parse_keystroke(keystroke, &key_, &meta_) < 0) goto fail;

	kb = kbd_ev_lookup(keymap_, key_, meta_, NULL);
	if (!kb) goto fail;

	action = write_action(kb->action);
	if (!action) goto fail;

	kb->watermark = 1;
	add_to_str(str, len, "\"");
	add_to_str(str, len, action);
	add_to_str(str, len, "\"");
}

void
bind_config_string(unsigned char **file, int *len)
{
	enum keymap keymap;
	struct keybinding *keybinding;

	for (keymap = 0; keymap < KM_MAX; keymap++)
		foreach(keybinding, keymaps[keymap]) {
			unsigned char *keymap_str = write_keymap(keymap);
			unsigned char *action_str =
				write_action(keybinding->action);

			if (!keymap_str || !action_str || action_str[0] == ' ')
				continue;

			if (keybinding->watermark) {
				keybinding->watermark = 0;
				continue;
			}

			/* TODO: Maybe we should use string.write.. */
			add_to_str(file, len, "bind \"");
			add_to_str(file, len, keymap_str);
			add_to_str(file, len, "\" \"");
			make_keystroke(file, len, keybinding->key,
				       keybinding->meta);
			add_to_str(file, len, "\" = \"");
			add_to_str(file, len, action_str);
			add_to_str(file, len, "\"");
			add_to_str(file, len, NEWLINE);
		}
}


/*
 * Bind to Lua function.
 */

#ifdef HAVE_LUA
unsigned char *
bind_lua_func(unsigned char *ckmap, unsigned char *ckey, int func_ref)
{
	unsigned char *err = NULL;
	long key, meta;
	int action;
	int kmap = read_keymap(ckmap);

	if (kmap < 0)
		err = "Unrecognised keymap";
	else if (parse_keystroke(ckey, &key, &meta) < 0)
		err = "Error parsing keystroke";
	else if ((action = read_action(" *lua-function*")) < 0)
		err = "Unrecognised action (internal error)";
	else
		add_keybinding(kmap, action, key, meta, func_ref);

	return err;
}
#endif


/*
 * Default keybindings.
 */

struct default_kb {
	int action;
	long key;
	long meta;
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
	{ ACT_LINK_MENU, 'L' },
	{ ACT_JUMP_TO_LINK, 'l' },
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
	{ ACT_AUTO_COMPLETE_UNAMBIGUOUS, 'R', KBD_CTRL },
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

static void
add_default_keybindings()
{
	struct default_kb *kb;

	/* Maybe we shouldn't delete old keybindings. But on the other side, we
	 * can't trust clueless users what they'll push into sources modifying
	 * defaults, can we? ;)) */

	for (kb = default_main_keymap; kb->key; kb++)
		add_keybinding(KM_MAIN, kb->action, kb->key, kb->meta, LUA_NOREF);

	for (kb = default_edit_keymap; kb->key; kb++)
		add_keybinding(KM_EDIT, kb->action, kb->key, kb->meta, LUA_NOREF);

	for (kb = default_menu_keymap; kb->key; kb++)
		add_keybinding(KM_MENU, kb->action, kb->key, kb->meta, LUA_NOREF);
}
