/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.71 2003/06/08 10:49:25 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "terminal/kbd.h"
#include "lua/core.h"
#include "util/memory.h"

#ifndef HAVE_LUA
#define LUA_NOREF	0
#endif

#define table table_dirty_workaround_for_name_clash_with_libraries_on_macos


INIT_LIST_HEAD(kbdbind_box_items);
INIT_LIST_HEAD(kbdbind_boxes);

static struct listbox_item *keyact_box_items[ACT_ZOOM_FRAME + 1];
static struct list_head keymaps[KM_MAX];

static int read_action(unsigned char *);
static void add_default_keybindings(void);
static void init_action_listboxes(void);
static void free_action_listboxes(void);


void
add_keybinding(enum keymap km, int action, long key, long meta, int func_ref)
{
	struct keybinding *kb;

	delete_keybinding(km, key, meta);

	kb = mem_alloc(sizeof(struct keybinding));
	if (kb) {
		struct listbox_item *keymap;
		unsigned char *keystroke = init_str();
		int len = 0;

		kb->keymap = km;
		kb->action = action;
		kb->key = key;
		kb->meta = meta;
		kb->func_ref = func_ref;
		kb->flags &= ~KBDB_WATERMARK;
		add_to_list(keymaps[km], kb);

		if (action == ACT_NONE) {
			/* We don't want such a listbox_item, do we? */
			kb->box_item = NULL;
			return; /* Or goto. */
		}
		make_keystroke(&keystroke, &len, key, meta);
		kb->box_item = mem_calloc(1, sizeof(struct listbox_item) + len + 1);
		if (!kb->box_item) {
			mem_free(keystroke);
			return; /* Or just goto after end of this if block. */
		}
		kb->box_item->text = ((unsigned char *) kb->box_item
					+ sizeof(struct listbox_item));
		strcpy(kb->box_item->text, keystroke);
		mem_free(keystroke);

		if (!keyact_box_items[action]) {
boom:
			mem_free(kb->box_item);
			kb->box_item = NULL;
			return; /* Or goto ;-). */
		}
		for (keymap = keyact_box_items[action]->child.next;
		     keymap != (struct listbox_item *) &keyact_box_items[action]->child && km;
		     km--)
			keymap = keymap->next;
		if (keymap == (struct listbox_item *) &keyact_box_items[action]->child)
			goto boom;

		add_to_list(keymap->child, kb->box_item);
		kb->box_item->root = keymap;
		init_list(kb->box_item->child);
		kb->box_item->visible = 1;
		kb->box_item->translated = 1;
		kb->box_item->udata = kb;
		kb->box_item->type = BI_LEAF;
		kb->box_item->depth = keymap->depth + 1;
		kb->box_item->box = &kbdbind_boxes;
	}
}

void
free_keybinding(struct keybinding *kb)
{
	if (kb->box_item) {
		del_from_list(kb->box_item);
		mem_free(kb->box_item);
	}
#ifdef HAVE_LUA
	if (kb->func_ref != LUA_NOREF)
		lua_unref(lua_state, kb->func_ref);
#endif
	del_from_list(kb);
	mem_free(kb);
}

void
delete_keybinding(enum keymap km, long key, long meta)
{
	struct keybinding *kb;

	foreach (kb, keymaps[km]) {
		if (kb->key != key || kb->meta != meta)
			continue;

		free_keybinding(kb);
		break;
	}
}


void
init_keymaps(void)
{
    	enum keymap i;

	for (i = 0; i < KM_MAX; i++)
		init_list(keymaps[i]);

	init_action_listboxes();
	add_default_keybindings();
}

void
free_keymaps(void)
{
	enum keymap i;

	free_action_listboxes();

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

	foreach (kb, keymaps[kmap]) {
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

	foreach (kb, keymaps[kmap]) {
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
	unsigned char *desc;
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


static unsigned char *
numtodesc(struct strtonum *table, long num)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (num == rec->num)
			return (rec->desc) ? rec->desc : rec->str;

	return NULL;
}


static struct strtonum keymap_table[] = {
	{ "main", KM_MAIN, N_("Main mapping") },
	{ "edit", KM_EDIT, N_("Edit mapping") },
	{ "menu", KM_MENU, N_("Menu mapping") },
	{ NULL, 0, NULL }
};

static int
read_keymap(unsigned char *keymap)
{
	return strtonum(keymap_table, keymap);
}

unsigned char *
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
	return (key[0] && !key[1]) ? *key : strtonum(key_table, key);
}

static unsigned char *
write_key(long key)
{
	static unsigned char dirty[3];
	unsigned char *bin = numtostr(key_table, key);

	if (bin) return bin;

	dirty[0] = (unsigned char) key;
	if (key == '\\')
		dirty[1] = '\\', dirty[2] = '\0';
	else
		dirty[1] = '\0';
	return dirty;
}


int
parse_keystroke(unsigned char *s, long *key, long *meta)
{
	*meta = 0;
	if (!strncmp(s, "Shift-", 6)) {
		*meta |= KBD_SHIFT;
		s += 6;
	} else if (!strncmp(s, "Ctrl-", 5)) {
		*meta |= KBD_CTRL;
		s += 5;
		if (s[0] && !s[1]) s[0] = toupper(s[0]);
	} else if (!strncmp(s, "Alt-", 4)) {
		*meta |= KBD_ALT;
		s += 4;
	}

	*key = read_key(s);
	return (*key < 0) ? -1 : 0;
}

void
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
	{ "none", ACT_NONE, NULL },
	{ "abort-connection", ACT_ABORT_CONNECTION, N_("Abort connection") },
	{ "add-bookmark", ACT_ADD_BOOKMARK, N_("Add a new bookmark") },
	{ "add-bookmark-link", ACT_ADD_BOOKMARK_LINK, N_("Add a new bookmark using current link") },
	{ "auto-complete", ACT_AUTO_COMPLETE, N_("Attempt to auto-complete the input") },
	{ "auto-complete-unambiguous", ACT_AUTO_COMPLETE_UNAMBIGUOUS, N_("Attempt to unambiguously auto-complete the input") },
	{ "back", ACT_BACK, N_("Return to the previous document in history") },
	{ "backspace", ACT_BACKSPACE, N_("Delete character in front of the cursor") },
	{ "bookmark-manager", ACT_BOOKMARK_MANAGER, N_("Open bookmark manager") },
	{ "cookies-load", ACT_COOKIES_LOAD, N_("Reload cookies file") },
	{ "copy-clipboard", ACT_COPY_CLIPBOARD, N_("Copy text to clipboard") },
	{ "cut-clipboard", ACT_CUT_CLIPBOARD, N_("Delete text from clipboard") },
	{ "delete", ACT_DELETE, N_("Delete character under cursor") },
	{ "document-info", ACT_DOCUMENT_INFO, N_("Show informations about the current page") },
	{ "down", ACT_DOWN, N_("Move cursor downwards") },
	{ "download", ACT_DOWNLOAD, N_("Download the current link") },
	{ "download-image", ACT_DOWNLOAD_IMAGE, N_("Download the current image") },
	{ "edit", ACT_EDIT, N_("Begin editing") }, /* FIXME */
	{ "end", ACT_END, N_("Go to the end of the page/line") },
	{ "enter", ACT_ENTER, N_("Follow the current link") },
	{ "enter-reload", ACT_ENTER_RELOAD, N_("Follow the current link, forcing reload of the target") },
	{ "file-menu", ACT_FILE_MENU, N_("Open the File menu") },
	{ "find-next", ACT_FIND_NEXT, N_("Find the next occurrence of the current search text") },
	{ "find-next-back", ACT_FIND_NEXT_BACK, N_("Find the next occurrence of the current search text, in reverse") },
	{ "forget-credentials", ACT_FORGET_CREDENTIALS, N_("Forget authentication credentials") },
	{ "goto-url", ACT_GOTO_URL, N_("Open \"Go to URL\" dialog box") },
	{ "goto-url-current", ACT_GOTO_URL_CURRENT, N_("Open \"Go to URL\" dialog box containing the current URL") },
	{ "goto-url-current-link", ACT_GOTO_URL_CURRENT_LINK, N_("Open \"Go to URL\" dialog box containing the current link URL") },
	{ "goto-url-home", ACT_GOTO_URL_HOME, N_("Go to the homepage") },
	{ "header-info", ACT_HEADER_INFO, N_("Show informations about the the current page HTTP headers") },
	{ "history-manager", ACT_HISTORY_MANAGER, N_("Open history manager") },
	{ "home", ACT_HOME, N_("Go to the start of the page/line") },
	{ "kill-to-bol", ACT_KILL_TO_BOL, N_("Delete to beginning of line") },
	{ "kill-to-eol", ACT_KILL_TO_EOL, N_("Delete to end of line") },
	{ "keybinding-manager", ACT_KEYBINDING_MANAGER, N_("Open keybinding manager") },
	{ "left", ACT_LEFT, N_("Move the cursor left") },
	{ "link-menu", ACT_LINK_MENU, N_("Activate the link context menu") },
	{ "jump-to-link", ACT_JUMP_TO_LINK, N_("Jump to link") },
#ifdef HAVE_LUA
	{ "lua-console", ACT_LUA_CONSOLE, N_("Open a Lua console") },
#else
	{ "lua-console", ACT_LUA_CONSOLE, N_("Open a Lua console (DISABLED)") },
#endif
	{ " *lua-function*", ACT_LUA_FUNCTION, NULL }, /* internal use only */
	{ "menu", ACT_MENU, N_("Activate the menu") },
	{ "next-frame", ACT_NEXT_FRAME, N_("Move to the next frame") },
	{ "open-new-window", ACT_OPEN_NEW_WINDOW, N_("Open a new window") },
	{ "open-link-in-new-window", ACT_OPEN_LINK_IN_NEW_WINDOW, N_("Open the current link in a new window") },
	{ "options-manager", ACT_OPTIONS_MANAGER, N_("Open options manager") },
	{ "page-down", ACT_PAGE_DOWN, N_("Move downwards by a page") },
	{ "page-up", ACT_PAGE_UP, N_("Move upwards by a page") },
	{ "paste-clipboard", ACT_PASTE_CLIPBOARD, N_("Paste text from the clipboard") },
	{ "previous-frame", ACT_PREVIOUS_FRAME, N_("Move to the previous frame") },
	{ "quit", ACT_QUIT, N_("Open a quit confirmation dialog box") },
	{ "really-quit", ACT_REALLY_QUIT, N_("Quit without confirmation") },
	{ "reload", ACT_RELOAD, N_("Reload the current page") },
	{ "resume-download", ACT_RESUME_DOWNLOAD, N_("Attempt to resume download of the current link") },
	{ "right", ACT_RIGHT, N_("Move the cursor right") },
	{ "save-formatted", ACT_SAVE_FORMATTED, N_("Save formatted document") },
	{ "scroll-down", ACT_SCROLL_DOWN, N_("Scroll down") },
	{ "scroll-left", ACT_SCROLL_LEFT, N_("Scroll left") },
	{ "scroll-right", ACT_SCROLL_RIGHT, N_("Scroll right") },
	{ "scroll-up", ACT_SCROLL_UP, N_("Scroll up") },
	{ "search", ACT_SEARCH, N_("Search for a text pattern") },
	{ "search-back", ACT_SEARCH_BACK, N_("Search backwards for a text pattern") },
	{ "tab-close", ACT_TAB_CLOSE, N_("Close tab") },
	{ "tab-next", ACT_TAB_NEXT, N_("Next tab") },
	{ "tab-prev", ACT_TAB_PREV, N_("Previous tab") },
	{ "toggle-display-images", ACT_TOGGLE_DISPLAY_IMAGES, N_("Toggle displaying of links to images") },
	{ "toggle-display-tables", ACT_TOGGLE_DISPLAY_TABLES, N_("Toggle rendering of tables") },
	{ "toggle-html-plain", ACT_TOGGLE_HTML_PLAIN, N_("Toggle rendering page as HTML / plain text") },
	{ "toggle-numbered-links", ACT_TOGGLE_NUMBERED_LINKS, N_("Toggle displaying of links numbers") },
	{ "unback", ACT_UNBACK, N_("Go forward in the unhistory") },
	{ "up", ACT_UP, N_("Move cursor upwards") },
	{ "view-image", ACT_VIEW_IMAGE, N_("View the current image") },
	{ "zoom-frame", ACT_ZOOM_FRAME, N_("Maximize the current frame") },
	{ NULL, 0, NULL }
};

static int
read_action(unsigned char *action)
{
	return strtonum(action_table, action);
}

unsigned char *
write_action(int action)
{

	return numtostr(action_table, action);
}

static void
init_action_listboxes(void)
{
	struct strtonum *act;

	for (act = action_table + 1; act->str; act++) {
		struct listbox_item *box_item;
		int i;

		keyact_box_items[act->num] = box_item =
			mem_calloc(1, sizeof(struct listbox_item));
		if (!box_item) continue;
		add_to_list_bottom(kbdbind_box_items, box_item);
		box_item->root = NULL;
		init_list(box_item->child);
		box_item->visible = (act->num != ACT_LUA_FUNCTION); /* XXX */
		box_item->translated = 1;
		box_item->udata = (void *) act->num;
		box_item->type = BI_FOLDER;
		box_item->expanded = 0; /* Maybe you would like this being 1? */
		box_item->depth = 0;
		box_item->box = &kbdbind_boxes;
		box_item->text = act->desc;

		for (i = 0; i < KM_MAX; i++) {
			struct listbox_item *keymap;

			keymap = mem_calloc(1, sizeof(struct listbox_item));
			if (!keymap) continue;
			add_to_list_bottom(box_item->child, keymap);
			keymap->root = box_item;
			init_list(keymap->child);
			keymap->visible = 1;
			keymap->translated = 1;
			keymap->udata = (void *) i;
			keymap->type = BI_FOLDER;
			keymap->expanded = 1;
			keymap->depth = 1;
			keymap->box = &kbdbind_boxes;
			keymap->text = numtodesc(keymap_table, i);
		}
	}
}

static void
free_action_listboxes(void)
{
	struct listbox_item *action;

	foreach (action, kbdbind_box_items) {
		struct listbox_item *keymap;

		foreach (keymap, action->child) {
			free_list(keymap->child);
		}
		free_list(action->child);
	}
	free_list(kbdbind_box_items);
}


void
toggle_display_action_listboxes(void)
{
	struct listbox_item *action;
	unsigned char *(*toggle)(struct strtonum *table, long num);
	static int state = 1;

	state = !state;
	toggle = state ? numtodesc : numtostr;

	foreach (action, kbdbind_box_items) {
		struct listbox_item *keymap;

		action->text = toggle(action_table, (int) action->udata);
		foreach (keymap, action->child)
			keymap->text = toggle(keymap_table, (int) keymap->udata);
	}
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

unsigned char *
bind_act(unsigned char *keymap, unsigned char *keystroke)
{
	int keymap_;
	long key_, meta_;
	unsigned char *action;
	struct keybinding *kb;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0)
		return NULL;

	if (parse_keystroke(keystroke, &key_, &meta_) < 0)
		return NULL;

	kb = kbd_ev_lookup(keymap_, key_, meta_, NULL);
	if (!kb) return NULL;

	action = write_action(kb->action);
	if (!action)
		return NULL;

	kb->flags |= KBDB_WATERMARK;
	return straconcat("\"", action, "\"", NULL);
}

void
bind_config_string(unsigned char **file, int *len)
{
	enum keymap keymap;
	struct keybinding *keybinding;

	for (keymap = 0; keymap < KM_MAX; keymap++)
		foreach (keybinding, keymaps[keymap]) {
			unsigned char *keymap_str = write_keymap(keymap);
			unsigned char *action_str =
				write_action(keybinding->action);

			if (!keymap_str || !action_str || action_str[0] == ' ')
				continue;

			if (keybinding->flags & KBDB_WATERMARK) {
				keybinding->flags &= ~KBDB_WATERMARK;
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
			add_chr_to_str(file, len, '\"');
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
		err = gettext("Unrecognised keymap");
	else if (parse_keystroke(ckey, &key, &meta) < 0)
		err = gettext("Error parsing keystroke");
	else if ((action = read_action(" *lua-function*")) < 0)
		err = gettext("Unrecognised action (internal error)");
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
	{ ACT_SCROLL_LEFT, '{', 0 },
	{ ACT_SCROLL_RIGHT, '}', 0 },
	{ ACT_HOME, KBD_HOME, 0 },
	{ ACT_HOME, 'A', KBD_CTRL },
	{ ACT_END, KBD_END, 0 },
	{ ACT_END, 'E', KBD_CTRL },
	{ ACT_ENTER, KBD_RIGHT, 0 },
	{ ACT_ENTER, KBD_ENTER, 0 },
	{ ACT_ENTER_RELOAD, KBD_RIGHT, KBD_CTRL },
	{ ACT_ENTER_RELOAD, KBD_ENTER, KBD_CTRL },
	{ ACT_ENTER_RELOAD, 'x', 0 },
	{ ACT_BACK, KBD_LEFT, 0 },
	{ ACT_UNBACK, 'u', 0 },
	{ ACT_UNBACK, 'U', 0 },
	{ ACT_DOWNLOAD, 'd', 0 },
	{ ACT_DOWNLOAD, 'D', 0 },
	{ ACT_RESUME_DOWNLOAD, 'r', 0 },
	{ ACT_RESUME_DOWNLOAD, 'R', 0 },
	{ ACT_ABORT_CONNECTION, 'z', 0 },
	{ ACT_SEARCH, '/', 0 },
	{ ACT_SEARCH_BACK, '?', 0 },
	{ ACT_FIND_NEXT, 'n', 0 },
	{ ACT_FIND_NEXT_BACK, 'N', 0 },
	{ ACT_ZOOM_FRAME, 'f', 0 },
	{ ACT_ZOOM_FRAME, 'F', 0 },
	{ ACT_RELOAD, 'R', KBD_CTRL },
	{ ACT_GOTO_URL_CURRENT_LINK, 'E', 0},
	{ ACT_GOTO_URL, 'g', 0 },
	{ ACT_GOTO_URL_CURRENT, 'G', 0 },
	{ ACT_GOTO_URL_HOME, 'H' },
	{ ACT_GOTO_URL_HOME, 'm' },
	{ ACT_GOTO_URL_HOME, 'M' },
	{ ACT_ADD_BOOKMARK, 'a' },
	{ ACT_ADD_BOOKMARK_LINK, 'A' },
	{ ACT_BOOKMARK_MANAGER, 's' },
	{ ACT_BOOKMARK_MANAGER, 'S' },
	{ ACT_HISTORY_MANAGER, 'h' },
	{ ACT_OPTIONS_MANAGER, 'o' },
	{ ACT_KEYBINDING_MANAGER, 'k' },
	{ ACT_COOKIES_LOAD, 'K', KBD_CTRL },
	{ ACT_QUIT, 'q' },
	{ ACT_REALLY_QUIT, 'Q' },
	{ ACT_DOCUMENT_INFO, '=' },
	{ ACT_HEADER_INFO, '|' },
	{ ACT_TAB_CLOSE, 'c' },
	{ ACT_TAB_NEXT, '>' },
	{ ACT_TAB_PREV, '<' },
	{ ACT_TOGGLE_HTML_PLAIN, '\\' },
	{ ACT_TOGGLE_NUMBERED_LINKS, '.' },
	{ ACT_TOGGLE_DISPLAY_IMAGES, '*' },
	{ ACT_NEXT_FRAME, KBD_TAB },
	{ ACT_MENU, KBD_ESC },
	{ ACT_MENU, KBD_F9 },
	{ ACT_FILE_MENU, KBD_F10 },
	{ ACT_LUA_CONSOLE, ',' },
	{ ACT_LINK_MENU, 'L' },
	{ ACT_JUMP_TO_LINK, 'l' },
	{ ACT_VIEW_IMAGE, 'v' },
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
add_default_keybindings(void)
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
