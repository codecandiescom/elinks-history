/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.190 2004/01/25 13:52:29 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "sched/event.h"
#include "terminal/kbd.h"
#include "util/memory.h"
#include "util/string.h"


/* Fix namespace clash on MacOS. */
#define table table_elinks

static struct strtonum *action_table[KM_MAX];
/* XXX: 1024 is just a quick hack, we ought to allocate the sub-arrays
 * separately. --pasky */
static struct listbox_item *action_box_items[KM_MAX][1024];
static struct list_head keymaps[KM_MAX];

static int read_action(enum keymap, unsigned char *);
static void add_default_keybindings(void);
static void init_action_listboxes(void);
static void free_action_listboxes(void);


struct keybinding *
add_keybinding(enum keymap km, int action, long key, long meta, int func_ref)
{
	struct keybinding *kb;
	struct listbox_item *box_item;
	struct string keystroke;

	delete_keybinding(km, key, meta);

	kb = mem_alloc(sizeof(struct keybinding));
	if (!kb) return NULL;

	kb->keymap = km;
	kb->action = action;
	kb->key = key;
	kb->meta = meta;
	kb->func_ref = func_ref;
	kb->flags = 0;
	add_to_list(keymaps[km], kb);

	if (action == ACT_MAIN_NONE) {
		/* We don't want such a listbox_item, do we? */
		kb->box_item = NULL;
		return NULL; /* Or goto. */
	}

	if (!init_string(&keystroke)) return NULL;

	make_keystroke(&keystroke, key, meta, 0);
	kb->box_item = mem_calloc(1, sizeof(struct listbox_item)
				  + keystroke.length + 1);
	if (!kb->box_item) {
		done_string(&keystroke);
		return NULL; /* Or just goto after end of this if block. */
	}
	kb->box_item->text = ((unsigned char *) kb->box_item
				+ sizeof(struct listbox_item));
	strcpy(kb->box_item->text, keystroke.source);
	done_string(&keystroke);

	box_item = action_box_items[km][action];
	if (!box_item) {
		mem_free(kb->box_item);
		kb->box_item = NULL;
		return NULL; /* Or goto ;-). */
	}

	add_to_list(box_item->child, kb->box_item);
	kb->box_item->root = box_item;
	init_list(kb->box_item->child);
	kb->box_item->visible = 1;
	kb->box_item->translated = 1;
	kb->box_item->udata = kb;
	kb->box_item->type = BI_LEAF;
	kb->box_item->depth = box_item->depth + 1;

	update_hierbox_browser(&keybinding_browser);

	return kb;
}

void
free_keybinding(struct keybinding *kb)
{
	if (kb->box_item) {
		done_listbox_item(&keybinding_browser, kb->box_item);
		kb->box_item = NULL;
	}

#ifdef HAVE_SCRIPTING
/* TODO: unref function must be implemented. */
/*	if (kb->func_ref != EVENT_NONE)
		scripting_unref(kb->func_ref); */
#endif

	if (kb->flags & KBDB_DEFAULT) {
		/* We cannot just delete a default keybinding, instead we have
		 * to rebind it to ACT_MAIN_NONE so that it gets written so to the
		 * config file. */
		kb->action = ACT_MAIN_NONE;
		return;
	}

	del_from_list(kb);
	mem_free(kb);
}

int
keybinding_exists(enum keymap km, long key, long meta, int *action)
{
	struct keybinding *kb;

	foreach (kb, keymaps[km]) {
		if (kb->key != key || kb->meta != meta)
			continue;

		if (action) *action = kb->action;

		return 1;
	}

	return 0;
}

static void
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
kbd_action(enum keymap kmap, struct term_event *ev, int *func_ref)
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

		if (kb->action == ACT_MAIN_SCRIPTING_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_nm_lookup(enum keymap kmap, unsigned char *name, int *func_ref)
{
	struct keybinding *kb;
	int act = read_action(kmap, name);

	if (act < 0) return NULL;

	foreach (kb, keymaps[kmap]) {
		if (act != kb->action)
			continue;

		if (kb->action == ACT_MAIN_SCRIPTING_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_act_lookup(enum keymap map, int action)
{
	struct keybinding *kb;

	foreach (kb, keymaps[map]) {
		if (action != kb->action)
			continue;

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
strtonum(struct strtonum *table, unsigned char *str)
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
	{ "Space", ' ' },
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
make_keystroke(struct string *str, long key, long meta, int escape)
{
	unsigned char key_buffer[3] = "\\x";
	unsigned char *key_string = numtostr(key_table, key);

	if (meta & KBD_SHIFT)
		add_to_string(str, "Shift-");
	if (meta & KBD_CTRL)
		add_to_string(str, "Ctrl-");
	if (meta & KBD_ALT)
		add_to_string(str, "Alt-");

	if (!key_string) {
		key_string = key_buffer + 1;
		*key_string = (unsigned char) key;
		if (key == '\\' && escape)
			key_string--;
	}

	add_to_string(str, key_string);
}

void
add_keystroke_to_string(struct string *string, int action,
			enum keymap map)
{
	struct keybinding *kb = kbd_act_lookup(map, action);

	if (kb)
		make_keystroke(string, kb->key, kb->meta, 0);
}

void
add_actions_to_string(struct string *string, int *actions,
		      enum keymap map, struct terminal *term)
{
	int i;

	assert(map >= 0 && map < KM_MAX);

	add_format_to_string(string, "%s:\n", _(numtodesc(keymap_table, map), term));

	for (i = 0; actions[i] != ACT_MAIN_NONE; i++) {
		struct keybinding *kb = kbd_act_lookup(map, actions[i]);
		int keystrokelen = string->length;
		unsigned char *desc = numtodesc(action_table[map], actions[i]);

		if (!kb) continue;

		add_char_to_string(string, '\n');
		make_keystroke(string, kb->key, kb->meta, 0);
		keystrokelen = string->length - keystrokelen;
		add_xchar_to_string(string, ' ', int_max(15 - keystrokelen, 1));
		add_to_string(string, _(desc, term));
	}
}


#ifndef ELINKS_SMALL
#define DACT(x) (x)
#else
#define DACT(x) (NULL)
#endif

/* Please keep these tables in alphabetical order, and in sync with
 * the ACT_* constants in kbdbind.h.  */

static struct strtonum main_action_table[] = {
	{ "none", ACT_MAIN_NONE, DACT(N_("Do nothing")) },
	{ " *scripting-function*", ACT_MAIN_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "abort-connection", ACT_MAIN_ABORT_CONNECTION, DACT(N_("Abort connection")) },
	{ "add-bookmark", ACT_MAIN_ADD_BOOKMARK, DACT(N_("Add a new bookmark")) },
	{ "add-bookmark-link", ACT_MAIN_ADD_BOOKMARK_LINK, DACT(N_("Add a new bookmark using current link")) },
	{ "add-bookmark-tabs", ACT_MAIN_ADD_BOOKMARK_TABS, DACT(N_("Bookmark all open tabs")) },
	{ "auto-complete", ACT_MAIN_AUTO_COMPLETE, DACT(N_("Attempt to auto-complete the input")) },
	{ "auto-complete-unambiguous", ACT_MAIN_AUTO_COMPLETE_UNAMBIGUOUS, DACT(N_("Attempt to unambiguously auto-complete the input")) },
	{ "back", ACT_MAIN_BACK, DACT(N_("Return to the previous document in history")) },
	{ "backspace", ACT_MAIN_BACKSPACE, DACT(N_("Delete character in front of the cursor")) },
	{ "beginning-of-buffer", ACT_MAIN_BEGINNING_OF_BUFFER, DACT(N_("Go to the first line of the buffer")) },
	{ "bookmark-manager", ACT_MAIN_BOOKMARK_MANAGER, DACT(N_("Open bookmark manager")) },
	{ "cache-manager", ACT_MAIN_CACHE_MANAGER, DACT(N_("Open cache manager")) },
	{ "cache-minimize", ACT_MAIN_CACHE_MINIMIZE, DACT(N_("Free unused cache entries")) },
	{ "cancel", ACT_MAIN_CANCEL, DACT(N_("Cancel current state")) },
	{ "cookie-manager", ACT_MAIN_COOKIE_MANAGER, DACT(N_("Open cookie manager")) },
	{ "cookies-load", ACT_MAIN_COOKIES_LOAD, DACT(N_("Reload cookies file")) },
	{ "copy-clipboard", ACT_MAIN_COPY_CLIPBOARD, DACT(N_("Copy text to clipboard")) },
	{ "cut-clipboard", ACT_MAIN_CUT_CLIPBOARD, DACT(N_("Delete text from clipboard")) },
	{ "delete", ACT_MAIN_DELETE, DACT(N_("Delete character under cursor")) },
	{ "document-info", ACT_MAIN_DOCUMENT_INFO, DACT(N_("Show information about the current page")) },
	{ "down", ACT_MAIN_DOWN, DACT(N_("Move cursor downwards")) },
	{ "download", ACT_MAIN_DOWNLOAD, DACT(N_("Download the current link")) },
	{ "download-image", ACT_MAIN_DOWNLOAD_IMAGE, DACT(N_("Download the current image")) },
	{ "download-manager", ACT_MAIN_DOWNLOAD_MANAGER, DACT(N_("Open download manager")) },
	{ "edit", ACT_MAIN_EDIT, DACT(N_("Begin editing")) }, /* FIXME */
	{ "end", ACT_MAIN_END, DACT(N_("Go to the end of the page/line")) },
	{ "end-of-buffer", ACT_MAIN_END_OF_BUFFER, DACT(N_("Go to the last line of the buffer")) },
	{ "enter", ACT_MAIN_ENTER, DACT(N_("Follow the current link")) },
	{ "enter-reload", ACT_MAIN_ENTER_RELOAD, DACT(N_("Follow the current link, forcing reload of the target")) },
	{ "exmode", ACT_MAIN_EXMODE, DACT(N_("Enter ex-mode (command line)")) },
	{ "expand", ACT_MAIN_EXPAND, DACT(N_("Expand item")) },
	{ "file-menu", ACT_MAIN_FILE_MENU, DACT(N_("Open the File menu")) },
	{ "find-next", ACT_MAIN_FIND_NEXT, DACT(N_("Find the next occurrence of the current search text")) },
	{ "find-next-back", ACT_MAIN_FIND_NEXT_BACK, DACT(N_("Find the previous occurrence of the current search text")) },
	{ "forget-credentials", ACT_MAIN_FORGET_CREDENTIALS, DACT(N_("Forget authentication credentials")) },
	{ "formhist-manager", ACT_MAIN_FORMHIST_MANAGER, DACT(N_("Open form history manager")) },
	{ "goto-url", ACT_MAIN_GOTO_URL, DACT(N_("Open \"Go to URL\" dialog box")) },
	{ "goto-url-current", ACT_MAIN_GOTO_URL_CURRENT, DACT(N_("Open \"Go to URL\" dialog box containing the current URL")) },
	{ "goto-url-current-link", ACT_MAIN_GOTO_URL_CURRENT_LINK, DACT(N_("Open \"Go to URL\" dialog box containing the current link URL")) },
	{ "goto-url-home", ACT_MAIN_GOTO_URL_HOME, DACT(N_("Go to the homepage")) },
	{ "header-info", ACT_MAIN_HEADER_INFO, DACT(N_("Show information about the current page HTTP headers")) },
	{ "history-manager", ACT_MAIN_HISTORY_MANAGER, DACT(N_("Open history manager")) },
	{ "home", ACT_MAIN_HOME, DACT(N_("Go to the start of the page/line")) },
	{ "jump-to-link", ACT_MAIN_JUMP_TO_LINK, DACT(N_("Jump to link")) },
	{ "keybinding-manager", ACT_MAIN_KEYBINDING_MANAGER, DACT(N_("Open keybinding manager")) },
	{ "kill-backgrounded-connections", ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS, DACT(N_("Kill all backgrounded connections")) },
	{ "kill-to-bol", ACT_MAIN_KILL_TO_BOL, DACT(N_("Delete to beginning of line")) },
	{ "kill-to-eol", ACT_MAIN_KILL_TO_EOL, DACT(N_("Delete to end of line")) },
	{ "left", ACT_MAIN_LEFT,DACT( N_("Move the cursor left")) },
	{ "link-menu", ACT_MAIN_LINK_MENU, DACT(N_("Open the link context menu")) },
#ifdef HAVE_LUA
	{ "lua-console", ACT_MAIN_LUA_CONSOLE, DACT(N_("Open a Lua console")) },
#else
	{ "lua-console", ACT_MAIN_LUA_CONSOLE, DACT(N_("Open a Lua console (DISABLED)")) },
#endif
	{ "mark-goto", ACT_MAIN_MARK_GOTO, DACT(N_("Go at a specified mark")) },
	{ "mark-item", ACT_MAIN_MARK_ITEM, DACT(N_("Mark item")) },
	{ "mark-set", ACT_MAIN_MARK_SET, DACT(N_("Set a mark")) },
	{ "menu", ACT_MAIN_MENU, DACT(N_("Activate the menu")) },
	{ "next-frame", ACT_MAIN_NEXT_FRAME, DACT(N_("Move to the next frame")) },
	{ "next-item", ACT_MAIN_NEXT_ITEM, DACT(N_("Move to the next item")) },
	{ "open-link-in-new-tab", ACT_MAIN_OPEN_LINK_IN_NEW_TAB, DACT(N_("Open the current link in a new tab")) },
	{ "open-link-in-new-tab-in-background", ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open the current link a new tab in background")) },
	{ "open-link-in-new-window", ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW, DACT(N_("Open the current link in a new window")) },
	{ "open-new-tab", ACT_MAIN_OPEN_NEW_TAB, DACT(N_("Open a new tab")) },
	{ "open-new-tab-in-background", ACT_MAIN_OPEN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open a new tab in background")) },
	{ "open-new-window", ACT_MAIN_OPEN_NEW_WINDOW, DACT(N_("Open a new window")) },
	{ "open-os-shell", ACT_MAIN_OPEN_OS_SHELL, DACT(N_("Open an OS shell")) },
	{ "options-manager", ACT_MAIN_OPTIONS_MANAGER, DACT(N_("Open options manager")) },
	{ "page-down", ACT_MAIN_PAGE_DOWN, DACT(N_("Move downwards by a page")) },
	{ "page-up", ACT_MAIN_PAGE_UP, DACT(N_("Move upwards by a page")) },
	{ "paste-clipboard", ACT_MAIN_PASTE_CLIPBOARD, DACT(N_("Paste text from the clipboard")) },
	{ "previous-frame", ACT_MAIN_PREVIOUS_FRAME, DACT(N_("Move to the previous frame")) },
	{ "quit", ACT_MAIN_QUIT, DACT(N_("Open a quit confirmation dialog box")) },
	{ "really-quit", ACT_MAIN_REALLY_QUIT, DACT(N_("Quit without confirmation")) },
	{ "redraw", ACT_MAIN_REDRAW, DACT(N_("Redraw the terminal")) },
	{ "reload", ACT_MAIN_RELOAD, DACT(N_("Reload the current page")) },
	{ "rerender", ACT_MAIN_RERENDER, DACT(N_("Re-render the current page")) },
	{ "reset-form", ACT_MAIN_RESET_FORM, DACT(N_("Reset form items to their initial values")) },
	{ "resource-info", ACT_MAIN_RESOURCE_INFO, DACT(N_("Show information about the currently used resources")) },
	{ "resume-download", ACT_MAIN_RESUME_DOWNLOAD, DACT(N_("Attempt to resume download of the current link")) },
	{ "right", ACT_MAIN_RIGHT, DACT(N_("Move the cursor right")) },
	{ "save-as", ACT_MAIN_SAVE_AS, DACT(N_("Save as")) },
	{ "save-formatted", ACT_MAIN_SAVE_FORMATTED, DACT(N_("Save formatted document")) },
	{ "save-options", ACT_MAIN_SAVE_OPTIONS, DACT(N_("Save options")), },
	{ "save-url-as", ACT_MAIN_SAVE_URL_AS, DACT(N_("Save URL as")) },
	{ "scroll-down", ACT_MAIN_SCROLL_DOWN, DACT(N_("Scroll down")) },
	{ "scroll-left", ACT_MAIN_SCROLL_LEFT, DACT(N_("Scroll left")) },
	{ "scroll-right", ACT_MAIN_SCROLL_RIGHT, DACT(N_("Scroll right")) },
	{ "scroll-up", ACT_MAIN_SCROLL_UP, DACT(N_("Scroll up")) },
	{ "search", ACT_MAIN_SEARCH, DACT(N_("Search for a text pattern")) },
	{ "search-back", ACT_MAIN_SEARCH_BACK, DACT(N_("Search backwards for a text pattern")) },
	{ "search-typeahead", ACT_MAIN_SEARCH_TYPEAHEAD, DACT(N_("Search link text by typing ahead")) },
	{ "select", ACT_MAIN_SELECT, DACT(N_("Select current highlighted item")) },
	{ "show-term-options", ACT_MAIN_SHOW_TERM_OPTIONS, DACT(N_("Show terminal options dialog")) },
	{ "submit-form", ACT_MAIN_SUBMIT_FORM, DACT(N_("Submit form")) },
	{ "submit-form-reload", ACT_MAIN_SUBMIT_FORM_RELOAD, DACT(N_("Submit form and reload")) },
	{ "tab-close", ACT_MAIN_TAB_CLOSE, DACT(N_("Close tab")) },
	{ "tab-close-all-but-current", ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT, DACT(N_("Close all tabs but the current one")) },
	{ "tab-menu", ACT_MAIN_TAB_MENU, DACT(N_("Open the tab menu")) },
	{ "tab-next", ACT_MAIN_TAB_NEXT, DACT(N_("Next tab")) },
	{ "tab-prev", ACT_MAIN_TAB_PREV,DACT( N_("Previous tab")) },
	{ "toggle-display-images", ACT_MAIN_TOGGLE_DISPLAY_IMAGES, DACT(N_("Toggle displaying of links to images")) },
	{ "toggle-display-tables", ACT_MAIN_TOGGLE_DISPLAY_TABLES, DACT(N_("Toggle rendering of tables")) },
	{ "toggle-document-colors", ACT_MAIN_TOGGLE_DOCUMENT_COLORS, DACT(N_("Toggle usage of document specific colors")) },
	{ "toggle-html-plain", ACT_MAIN_TOGGLE_HTML_PLAIN, DACT(N_("Toggle rendering page as HTML / plain text")) },
	{ "toggle-numbered-links", ACT_MAIN_TOGGLE_NUMBERED_LINKS, DACT(N_("Toggle displaying of links numbers")) },
	{ "toggle-plain-compress-empty-lines", ACT_MAIN_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES, DACT(N_("Toggle plain renderer compression of empty lines")) },
	{ "toggle-wrap-text", ACT_MAIN_TOGGLE_WRAP_TEXT, DACT(N_("Toggle wrapping of text")) },
	{ "unback", ACT_MAIN_UNBACK, DACT(N_("Go forward in the unhistory")) },
	{ "unexpand", ACT_MAIN_UNEXPAND, DACT(N_("Collapse item")) },
	{ "up", ACT_MAIN_UP, DACT(N_("Move cursor upwards")) },
	{ "view-image", ACT_MAIN_VIEW_IMAGE, DACT(N_("View the current image")) },
	{ "zoom-frame", ACT_MAIN_ZOOM_FRAME, DACT(N_("Maximize the current frame")) },

	{ NULL, 0, NULL }
};

static struct strtonum edit_action_table[] = {
	{ "none", ACT_EDIT_NONE, DACT(N_("Do nothing")) },
	{ " *scripting-function*", ACT_EDIT_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "abort-connection", ACT_EDIT_ABORT_CONNECTION, DACT(N_("Abort connection")) },
	{ "add-bookmark", ACT_EDIT_ADD_BOOKMARK, DACT(N_("Add a new bookmark")) },
	{ "add-bookmark-link", ACT_EDIT_ADD_BOOKMARK_LINK, DACT(N_("Add a new bookmark using current link")) },
	{ "add-bookmark-tabs", ACT_EDIT_ADD_BOOKMARK_TABS, DACT(N_("Bookmark all open tabs")) },
	{ "auto-complete", ACT_EDIT_AUTO_COMPLETE, DACT(N_("Attempt to auto-complete the input")) },
	{ "auto-complete-unambiguous", ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS, DACT(N_("Attempt to unambiguously auto-complete the input")) },
	{ "back", ACT_EDIT_BACK, DACT(N_("Return to the previous document in history")) },
	{ "backspace", ACT_EDIT_BACKSPACE, DACT(N_("Delete character in front of the cursor")) },
	{ "beginning-of-buffer", ACT_EDIT_BEGINNING_OF_BUFFER, DACT(N_("Go to the first line of the buffer")) },
	{ "bookmark-manager", ACT_EDIT_BOOKMARK_MANAGER, DACT(N_("Open bookmark manager")) },
	{ "cache-manager", ACT_EDIT_CACHE_MANAGER, DACT(N_("Open cache manager")) },
	{ "cache-minimize", ACT_EDIT_CACHE_MINIMIZE, DACT(N_("Free unused cache entries")) },
	{ "cancel", ACT_EDIT_CANCEL, DACT(N_("Cancel current state")) },
	{ "cookie-manager", ACT_EDIT_COOKIE_MANAGER, DACT(N_("Open cookie manager")) },
	{ "cookies-load", ACT_EDIT_COOKIES_LOAD, DACT(N_("Reload cookies file")) },
	{ "copy-clipboard", ACT_EDIT_COPY_CLIPBOARD, DACT(N_("Copy text to clipboard")) },
	{ "cut-clipboard", ACT_EDIT_CUT_CLIPBOARD, DACT(N_("Delete text from clipboard")) },
	{ "delete", ACT_EDIT_DELETE, DACT(N_("Delete character under cursor")) },
	{ "document-info", ACT_EDIT_DOCUMENT_INFO, DACT(N_("Show information about the current page")) },
	{ "down", ACT_EDIT_DOWN, DACT(N_("Move cursor downwards")) },
	{ "download", ACT_EDIT_DOWNLOAD, DACT(N_("Download the current link")) },
	{ "download-image", ACT_EDIT_DOWNLOAD_IMAGE, DACT(N_("Download the current image")) },
	{ "download-manager", ACT_EDIT_DOWNLOAD_MANAGER, DACT(N_("Open download manager")) },
	{ "edit", ACT_EDIT_EDIT, DACT(N_("Begin editing")) }, /* FIXME */
	{ "end", ACT_EDIT_END, DACT(N_("Go to the end of the page/line")) },
	{ "end-of-buffer", ACT_EDIT_END_OF_BUFFER, DACT(N_("Go to the last line of the buffer")) },
	{ "enter", ACT_EDIT_ENTER, DACT(N_("Follow the current link")) },
	{ "enter-reload", ACT_EDIT_ENTER_RELOAD, DACT(N_("Follow the current link, forcing reload of the target")) },
	{ "exmode", ACT_EDIT_EXMODE, DACT(N_("Enter ex-mode (command line)")) },
	{ "expand", ACT_EDIT_EXPAND, DACT(N_("Expand item")) },
	{ "file-menu", ACT_EDIT_FILE_MENU, DACT(N_("Open the File menu")) },
	{ "find-next", ACT_EDIT_FIND_NEXT, DACT(N_("Find the next occurrence of the current search text")) },
	{ "find-next-back", ACT_EDIT_FIND_NEXT_BACK, DACT(N_("Find the previous occurrence of the current search text")) },
	{ "forget-credentials", ACT_EDIT_FORGET_CREDENTIALS, DACT(N_("Forget authentication credentials")) },
	{ "formhist-manager", ACT_EDIT_FORMHIST_MANAGER, DACT(N_("Open form history manager")) },
	{ "goto-url", ACT_EDIT_GOTO_URL, DACT(N_("Open \"Go to URL\" dialog box")) },
	{ "goto-url-current", ACT_EDIT_GOTO_URL_CURRENT, DACT(N_("Open \"Go to URL\" dialog box containing the current URL")) },
	{ "goto-url-current-link", ACT_EDIT_GOTO_URL_CURRENT_LINK, DACT(N_("Open \"Go to URL\" dialog box containing the current link URL")) },
	{ "goto-url-home", ACT_EDIT_GOTO_URL_HOME, DACT(N_("Go to the homepage")) },
	{ "header-info", ACT_EDIT_HEADER_INFO, DACT(N_("Show information about the current page HTTP headers")) },
	{ "history-manager", ACT_EDIT_HISTORY_MANAGER, DACT(N_("Open history manager")) },
	{ "home", ACT_EDIT_HOME, DACT(N_("Go to the start of the page/line")) },
	{ "jump-to-link", ACT_EDIT_JUMP_TO_LINK, DACT(N_("Jump to link")) },
	{ "keybinding-manager", ACT_EDIT_KEYBINDING_MANAGER, DACT(N_("Open keybinding manager")) },
	{ "kill-backgrounded-connections", ACT_EDIT_KILL_BACKGROUNDED_CONNECTIONS, DACT(N_("Kill all backgrounded connections")) },
	{ "kill-to-bol", ACT_EDIT_KILL_TO_BOL, DACT(N_("Delete to beginning of line")) },
	{ "kill-to-eol", ACT_EDIT_KILL_TO_EOL, DACT(N_("Delete to end of line")) },
	{ "left", ACT_EDIT_LEFT,DACT( N_("Move the cursor left")) },
	{ "link-menu", ACT_EDIT_LINK_MENU, DACT(N_("Open the link context menu")) },
#ifdef HAVE_LUA
	{ "lua-console", ACT_EDIT_LUA_CONSOLE, DACT(N_("Open a Lua console")) },
#else
	{ "lua-console", ACT_EDIT_LUA_CONSOLE, DACT(N_("Open a Lua console (DISABLED)")) },
#endif
	{ "mark-goto", ACT_EDIT_MARK_GOTO, DACT(N_("Go at a specified mark")) },
	{ "mark-item", ACT_EDIT_MARK_ITEM, DACT(N_("Mark item")) },
	{ "mark-set", ACT_EDIT_MARK_SET, DACT(N_("Set a mark")) },
	{ "menu", ACT_EDIT_MENU, DACT(N_("Activate the menu")) },
	{ "next-frame", ACT_EDIT_NEXT_FRAME, DACT(N_("Move to the next frame")) },
	{ "next-item", ACT_EDIT_NEXT_ITEM, DACT(N_("Move to the next item")) },
	{ "open-link-in-new-tab", ACT_EDIT_OPEN_LINK_IN_NEW_TAB, DACT(N_("Open the current link in a new tab")) },
	{ "open-link-in-new-tab-in-background", ACT_EDIT_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open the current link a new tab in background")) },
	{ "open-link-in-new-window", ACT_EDIT_OPEN_LINK_IN_NEW_WINDOW, DACT(N_("Open the current link in a new window")) },
	{ "open-new-tab", ACT_EDIT_OPEN_NEW_TAB, DACT(N_("Open a new tab")) },
	{ "open-new-tab-in-background", ACT_EDIT_OPEN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open a new tab in background")) },
	{ "open-new-window", ACT_EDIT_OPEN_NEW_WINDOW, DACT(N_("Open a new window")) },
	{ "open-os-shell", ACT_EDIT_OPEN_OS_SHELL, DACT(N_("Open an OS shell")) },
	{ "options-manager", ACT_EDIT_OPTIONS_MANAGER, DACT(N_("Open options manager")) },
	{ "page-down", ACT_EDIT_PAGE_DOWN, DACT(N_("Move downwards by a page")) },
	{ "page-up", ACT_EDIT_PAGE_UP, DACT(N_("Move upwards by a page")) },
	{ "paste-clipboard", ACT_EDIT_PASTE_CLIPBOARD, DACT(N_("Paste text from the clipboard")) },
	{ "previous-frame", ACT_EDIT_PREVIOUS_FRAME, DACT(N_("Move to the previous frame")) },
	{ "quit", ACT_EDIT_QUIT, DACT(N_("Open a quit confirmation dialog box")) },
	{ "really-quit", ACT_EDIT_REALLY_QUIT, DACT(N_("Quit without confirmation")) },
	{ "redraw", ACT_EDIT_REDRAW, DACT(N_("Redraw the terminal")) },
	{ "reload", ACT_EDIT_RELOAD, DACT(N_("Reload the current page")) },
	{ "rerender", ACT_EDIT_RERENDER, DACT(N_("Re-render the current page")) },
	{ "reset-form", ACT_EDIT_RESET_FORM, DACT(N_("Reset form items to their initial values")) },
	{ "resource-info", ACT_EDIT_RESOURCE_INFO, DACT(N_("Show information about the currently used resources")) },
	{ "resume-download", ACT_EDIT_RESUME_DOWNLOAD, DACT(N_("Attempt to resume download of the current link")) },
	{ "right", ACT_EDIT_RIGHT, DACT(N_("Move the cursor right")) },
	{ "save-as", ACT_EDIT_SAVE_AS, DACT(N_("Save as")) },
	{ "save-formatted", ACT_EDIT_SAVE_FORMATTED, DACT(N_("Save formatted document")) },
	{ "save-options", ACT_EDIT_SAVE_OPTIONS, DACT(N_("Save options")), },
	{ "save-url-as", ACT_EDIT_SAVE_URL_AS, DACT(N_("Save URL as")) },
	{ "scroll-down", ACT_EDIT_SCROLL_DOWN, DACT(N_("Scroll down")) },
	{ "scroll-left", ACT_EDIT_SCROLL_LEFT, DACT(N_("Scroll left")) },
	{ "scroll-right", ACT_EDIT_SCROLL_RIGHT, DACT(N_("Scroll right")) },
	{ "scroll-up", ACT_EDIT_SCROLL_UP, DACT(N_("Scroll up")) },
	{ "search", ACT_EDIT_SEARCH, DACT(N_("Search for a text pattern")) },
	{ "search-back", ACT_EDIT_SEARCH_BACK, DACT(N_("Search backwards for a text pattern")) },
	{ "search-typeahead", ACT_EDIT_SEARCH_TYPEAHEAD, DACT(N_("Search link text by typing ahead")) },
	{ "select", ACT_EDIT_SELECT, DACT(N_("Select current highlighted item")) },
	{ "show-term-options", ACT_EDIT_SHOW_TERM_OPTIONS, DACT(N_("Show terminal options dialog")) },
	{ "submit-form", ACT_EDIT_SUBMIT_FORM, DACT(N_("Submit form")) },
	{ "submit-form-reload", ACT_EDIT_SUBMIT_FORM_RELOAD, DACT(N_("Submit form and reload")) },
	{ "tab-close", ACT_EDIT_TAB_CLOSE, DACT(N_("Close tab")) },
	{ "tab-close-all-but-current", ACT_EDIT_TAB_CLOSE_ALL_BUT_CURRENT, DACT(N_("Close all tabs but the current one")) },
	{ "tab-menu", ACT_EDIT_TAB_MENU, DACT(N_("Open the tab menu")) },
	{ "tab-next", ACT_EDIT_TAB_NEXT, DACT(N_("Next tab")) },
	{ "tab-prev", ACT_EDIT_TAB_PREV,DACT( N_("Previous tab")) },
	{ "toggle-display-images", ACT_EDIT_TOGGLE_DISPLAY_IMAGES, DACT(N_("Toggle displaying of links to images")) },
	{ "toggle-display-tables", ACT_EDIT_TOGGLE_DISPLAY_TABLES, DACT(N_("Toggle rendering of tables")) },
	{ "toggle-document-colors", ACT_EDIT_TOGGLE_DOCUMENT_COLORS, DACT(N_("Toggle usage of document specific colors")) },
	{ "toggle-html-plain", ACT_EDIT_TOGGLE_HTML_PLAIN, DACT(N_("Toggle rendering page as HTML / plain text")) },
	{ "toggle-numbered-links", ACT_EDIT_TOGGLE_NUMBERED_LINKS, DACT(N_("Toggle displaying of links numbers")) },
	{ "toggle-plain-compress-empty-lines", ACT_EDIT_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES, DACT(N_("Toggle plain renderer compression of empty lines")) },
	{ "toggle-wrap-text", ACT_EDIT_TOGGLE_WRAP_TEXT, DACT(N_("Toggle wrapping of text")) },
	{ "unback", ACT_EDIT_UNBACK, DACT(N_("Go forward in the unhistory")) },
	{ "unexpand", ACT_EDIT_UNEXPAND, DACT(N_("Collapse item")) },
	{ "up", ACT_EDIT_UP, DACT(N_("Move cursor upwards")) },
	{ "view-image", ACT_EDIT_VIEW_IMAGE, DACT(N_("View the current image")) },
	{ "zoom-frame", ACT_EDIT_ZOOM_FRAME, DACT(N_("Maximize the current frame")) },

	{ NULL, 0, NULL }
};

static struct strtonum menu_action_table[] = {
	{ "none", ACT_MENU_NONE, DACT(N_("Do nothing")) },
	{ " *scripting-function*", ACT_MENU_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "abort-connection", ACT_MENU_ABORT_CONNECTION, DACT(N_("Abort connection")) },
	{ "add-bookmark", ACT_MENU_ADD_BOOKMARK, DACT(N_("Add a new bookmark")) },
	{ "add-bookmark-link", ACT_MENU_ADD_BOOKMARK_LINK, DACT(N_("Add a new bookmark using current link")) },
	{ "add-bookmark-tabs", ACT_MENU_ADD_BOOKMARK_TABS, DACT(N_("Bookmark all open tabs")) },
	{ "auto-complete", ACT_MENU_AUTO_COMPLETE, DACT(N_("Attempt to auto-complete the input")) },
	{ "auto-complete-unambiguous", ACT_MENU_AUTO_COMPLETE_UNAMBIGUOUS, DACT(N_("Attempt to unambiguously auto-complete the input")) },
	{ "back", ACT_MENU_BACK, DACT(N_("Return to the previous document in history")) },
	{ "backspace", ACT_MENU_BACKSPACE, DACT(N_("Delete character in front of the cursor")) },
	{ "beginning-of-buffer", ACT_MENU_BEGINNING_OF_BUFFER, DACT(N_("Go to the first line of the buffer")) },
	{ "bookmark-manager", ACT_MENU_BOOKMARK_MANAGER, DACT(N_("Open bookmark manager")) },
	{ "cache-manager", ACT_MENU_CACHE_MANAGER, DACT(N_("Open cache manager")) },
	{ "cache-minimize", ACT_MENU_CACHE_MINIMIZE, DACT(N_("Free unused cache entries")) },
	{ "cancel", ACT_MENU_CANCEL, DACT(N_("Cancel current state")) },
	{ "cookie-manager", ACT_MENU_COOKIE_MANAGER, DACT(N_("Open cookie manager")) },
	{ "cookies-load", ACT_MENU_COOKIES_LOAD, DACT(N_("Reload cookies file")) },
	{ "copy-clipboard", ACT_MENU_COPY_CLIPBOARD, DACT(N_("Copy text to clipboard")) },
	{ "cut-clipboard", ACT_MENU_CUT_CLIPBOARD, DACT(N_("Delete text from clipboard")) },
	{ "delete", ACT_MENU_DELETE, DACT(N_("Delete character under cursor")) },
	{ "document-info", ACT_MENU_DOCUMENT_INFO, DACT(N_("Show information about the current page")) },
	{ "down", ACT_MENU_DOWN, DACT(N_("Move cursor downwards")) },
	{ "download", ACT_MENU_DOWNLOAD, DACT(N_("Download the current link")) },
	{ "download-image", ACT_MENU_DOWNLOAD_IMAGE, DACT(N_("Download the current image")) },
	{ "download-manager", ACT_MENU_DOWNLOAD_MANAGER, DACT(N_("Open download manager")) },
	{ "edit", ACT_MENU_EDIT, DACT(N_("Begin editing")) }, /* FIXME */
	{ "end", ACT_MENU_END, DACT(N_("Go to the end of the page/line")) },
	{ "end-of-buffer", ACT_MENU_END_OF_BUFFER, DACT(N_("Go to the last line of the buffer")) },
	{ "enter", ACT_MENU_ENTER, DACT(N_("Follow the current link")) },
	{ "enter-reload", ACT_MENU_ENTER_RELOAD, DACT(N_("Follow the current link, forcing reload of the target")) },
	{ "exmode", ACT_MENU_EXMODE, DACT(N_("Enter ex-mode (command line)")) },
	{ "expand", ACT_MENU_EXPAND, DACT(N_("Expand item")) },
	{ "file-menu", ACT_MENU_FILE_MENU, DACT(N_("Open the File menu")) },
	{ "find-next", ACT_MENU_FIND_NEXT, DACT(N_("Find the next occurrence of the current search text")) },
	{ "find-next-back", ACT_MENU_FIND_NEXT_BACK, DACT(N_("Find the previous occurrence of the current search text")) },
	{ "forget-credentials", ACT_MENU_FORGET_CREDENTIALS, DACT(N_("Forget authentication credentials")) },
	{ "formhist-manager", ACT_MENU_FORMHIST_MANAGER, DACT(N_("Open form history manager")) },
	{ "goto-url", ACT_MENU_GOTO_URL, DACT(N_("Open \"Go to URL\" dialog box")) },
	{ "goto-url-current", ACT_MENU_GOTO_URL_CURRENT, DACT(N_("Open \"Go to URL\" dialog box containing the current URL")) },
	{ "goto-url-current-link", ACT_MENU_GOTO_URL_CURRENT_LINK, DACT(N_("Open \"Go to URL\" dialog box containing the current link URL")) },
	{ "goto-url-home", ACT_MENU_GOTO_URL_HOME, DACT(N_("Go to the homepage")) },
	{ "header-info", ACT_MENU_HEADER_INFO, DACT(N_("Show information about the current page HTTP headers")) },
	{ "history-manager", ACT_MENU_HISTORY_MANAGER, DACT(N_("Open history manager")) },
	{ "home", ACT_MENU_HOME, DACT(N_("Go to the start of the page/line")) },
	{ "jump-to-link", ACT_MENU_JUMP_TO_LINK, DACT(N_("Jump to link")) },
	{ "keybinding-manager", ACT_MENU_KEYBINDING_MANAGER, DACT(N_("Open keybinding manager")) },
	{ "kill-backgrounded-connections", ACT_MENU_KILL_BACKGROUNDED_CONNECTIONS, DACT(N_("Kill all backgrounded connections")) },
	{ "kill-to-bol", ACT_MENU_KILL_TO_BOL, DACT(N_("Delete to beginning of line")) },
	{ "kill-to-eol", ACT_MENU_KILL_TO_EOL, DACT(N_("Delete to end of line")) },
	{ "left", ACT_MENU_LEFT,DACT( N_("Move the cursor left")) },
	{ "link-menu", ACT_MENU_LINK_MENU, DACT(N_("Open the link context menu")) },
#ifdef HAVE_LUA
	{ "lua-console", ACT_MENU_LUA_CONSOLE, DACT(N_("Open a Lua console")) },
#else
	{ "lua-console", ACT_MENU_LUA_CONSOLE, DACT(N_("Open a Lua console (DISABLED)")) },
#endif
	{ "mark-goto", ACT_MENU_MARK_GOTO, DACT(N_("Go at a specified mark")) },
	{ "mark-item", ACT_MENU_MARK_ITEM, DACT(N_("Mark item")) },
	{ "mark-set", ACT_MENU_MARK_SET, DACT(N_("Set a mark")) },
	{ "menu", ACT_MENU_MENU, DACT(N_("Activate the menu")) },
	{ "next-frame", ACT_MENU_NEXT_FRAME, DACT(N_("Move to the next frame")) },
	{ "next-item", ACT_MENU_NEXT_ITEM, DACT(N_("Move to the next item")) },
	{ "open-link-in-new-tab", ACT_MENU_OPEN_LINK_IN_NEW_TAB, DACT(N_("Open the current link in a new tab")) },
	{ "open-link-in-new-tab-in-background", ACT_MENU_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open the current link a new tab in background")) },
	{ "open-link-in-new-window", ACT_MENU_OPEN_LINK_IN_NEW_WINDOW, DACT(N_("Open the current link in a new window")) },
	{ "open-new-tab", ACT_MENU_OPEN_NEW_TAB, DACT(N_("Open a new tab")) },
	{ "open-new-tab-in-background", ACT_MENU_OPEN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open a new tab in background")) },
	{ "open-new-window", ACT_MENU_OPEN_NEW_WINDOW, DACT(N_("Open a new window")) },
	{ "open-os-shell", ACT_MENU_OPEN_OS_SHELL, DACT(N_("Open an OS shell")) },
	{ "options-manager", ACT_MENU_OPTIONS_MANAGER, DACT(N_("Open options manager")) },
	{ "page-down", ACT_MENU_PAGE_DOWN, DACT(N_("Move downwards by a page")) },
	{ "page-up", ACT_MENU_PAGE_UP, DACT(N_("Move upwards by a page")) },
	{ "paste-clipboard", ACT_MENU_PASTE_CLIPBOARD, DACT(N_("Paste text from the clipboard")) },
	{ "previous-frame", ACT_MENU_PREVIOUS_FRAME, DACT(N_("Move to the previous frame")) },
	{ "quit", ACT_MENU_QUIT, DACT(N_("Open a quit confirmation dialog box")) },
	{ "really-quit", ACT_MENU_REALLY_QUIT, DACT(N_("Quit without confirmation")) },
	{ "redraw", ACT_MENU_REDRAW, DACT(N_("Redraw the terminal")) },
	{ "reload", ACT_MENU_RELOAD, DACT(N_("Reload the current page")) },
	{ "rerender", ACT_MENU_RERENDER, DACT(N_("Re-render the current page")) },
	{ "reset-form", ACT_MENU_RESET_FORM, DACT(N_("Reset form items to their initial values")) },
	{ "resource-info", ACT_MENU_RESOURCE_INFO, DACT(N_("Show information about the currently used resources")) },
	{ "resume-download", ACT_MENU_RESUME_DOWNLOAD, DACT(N_("Attempt to resume download of the current link")) },
	{ "right", ACT_MENU_RIGHT, DACT(N_("Move the cursor right")) },
	{ "save-as", ACT_MENU_SAVE_AS, DACT(N_("Save as")) },
	{ "save-formatted", ACT_MENU_SAVE_FORMATTED, DACT(N_("Save formatted document")) },
	{ "save-options", ACT_MENU_SAVE_OPTIONS, DACT(N_("Save options")), },
	{ "save-url-as", ACT_MENU_SAVE_URL_AS, DACT(N_("Save URL as")) },
	{ "scroll-down", ACT_MENU_SCROLL_DOWN, DACT(N_("Scroll down")) },
	{ "scroll-left", ACT_MENU_SCROLL_LEFT, DACT(N_("Scroll left")) },
	{ "scroll-right", ACT_MENU_SCROLL_RIGHT, DACT(N_("Scroll right")) },
	{ "scroll-up", ACT_MENU_SCROLL_UP, DACT(N_("Scroll up")) },
	{ "search", ACT_MENU_SEARCH, DACT(N_("Search for a text pattern")) },
	{ "search-back", ACT_MENU_SEARCH_BACK, DACT(N_("Search backwards for a text pattern")) },
	{ "search-typeahead", ACT_MENU_SEARCH_TYPEAHEAD, DACT(N_("Search link text by typing ahead")) },
	{ "select", ACT_MENU_SELECT, DACT(N_("Select current highlighted item")) },
	{ "show-term-options", ACT_MENU_SHOW_TERM_OPTIONS, DACT(N_("Show terminal options dialog")) },
	{ "submit-form", ACT_MENU_SUBMIT_FORM, DACT(N_("Submit form")) },
	{ "submit-form-reload", ACT_MENU_SUBMIT_FORM_RELOAD, DACT(N_("Submit form and reload")) },
	{ "tab-close", ACT_MENU_TAB_CLOSE, DACT(N_("Close tab")) },
	{ "tab-close-all-but-current", ACT_MENU_TAB_CLOSE_ALL_BUT_CURRENT, DACT(N_("Close all tabs but the current one")) },
	{ "tab-menu", ACT_MENU_TAB_MENU, DACT(N_("Open the tab menu")) },
	{ "tab-next", ACT_MENU_TAB_NEXT, DACT(N_("Next tab")) },
	{ "tab-prev", ACT_MENU_TAB_PREV,DACT( N_("Previous tab")) },
	{ "toggle-display-images", ACT_MENU_TOGGLE_DISPLAY_IMAGES, DACT(N_("Toggle displaying of links to images")) },
	{ "toggle-display-tables", ACT_MENU_TOGGLE_DISPLAY_TABLES, DACT(N_("Toggle rendering of tables")) },
	{ "toggle-document-colors", ACT_MENU_TOGGLE_DOCUMENT_COLORS, DACT(N_("Toggle usage of document specific colors")) },
	{ "toggle-html-plain", ACT_MENU_TOGGLE_HTML_PLAIN, DACT(N_("Toggle rendering page as HTML / plain text")) },
	{ "toggle-numbered-links", ACT_MENU_TOGGLE_NUMBERED_LINKS, DACT(N_("Toggle displaying of links numbers")) },
	{ "toggle-plain-compress-empty-lines", ACT_MENU_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES, DACT(N_("Toggle plain renderer compression of empty lines")) },
	{ "toggle-wrap-text", ACT_MENU_TOGGLE_WRAP_TEXT, DACT(N_("Toggle wrapping of text")) },
	{ "unback", ACT_MENU_UNBACK, DACT(N_("Go forward in the unhistory")) },
	{ "unexpand", ACT_MENU_UNEXPAND, DACT(N_("Collapse item")) },
	{ "up", ACT_MENU_UP, DACT(N_("Move cursor upwards")) },
	{ "view-image", ACT_MENU_VIEW_IMAGE, DACT(N_("View the current image")) },
	{ "zoom-frame", ACT_MENU_ZOOM_FRAME, DACT(N_("Maximize the current frame")) },

	{ NULL, 0, NULL }
};

static struct strtonum *action_table[KM_MAX] = {
	main_action_table,
	edit_action_table,
	menu_action_table,
};

#undef DACT

static int
read_action(enum keymap keymap, unsigned char *action)
{
	assert(keymap >= 0 && keymap < KM_MAX);
	return strtonum(action_table[keymap], action);
}

unsigned char *
write_action(enum keymap keymap, int action)
{
	assert(keymap >= 0 && keymap < KM_MAX);
	return numtostr(action_table[keymap], action);
}

static void
init_action_listboxes(void)
{
	struct strtonum *act;
	int i;

	for (i = 0; i < KM_MAX; i++) {
		struct listbox_item *keymap;

		keymap = mem_calloc(1, sizeof(struct listbox_item));
		if (!keymap) continue;

		init_list(keymap->child);
		keymap->visible = 1;
		keymap->translated = 1;
		keymap->udata = (void *) i;
		keymap->type = BI_FOLDER;
		keymap->expanded = 0; /* Maybe you would like this being 1? */
		keymap->depth = 0;
		keymap->text = numtodesc(keymap_table, i);
		add_to_list_end(keybinding_browser.root.child, keymap);

		for (act = action_table[i]; act->str; act++) {
			struct listbox_item *box_item;

			if (act->num == ACT_MAIN_SCRIPTING_FUNCTION
			    || act->num == ACT_MAIN_NONE)
				continue;

			box_item = mem_calloc(1, sizeof(struct listbox_item));
			if (!box_item) continue;

			box_item->root = keymap;
			add_to_list_end(keymap->child, box_item);
			init_list(box_item->child);
			box_item->udata = (void *) act->num;
			box_item->type = BI_FOLDER;
			box_item->expanded = 1;
			box_item->visible = 1;
			box_item->depth = 1;

			assert(act->desc);
			box_item->text = act->desc;
			box_item->translated = 1;

			action_box_items[i][act->num] = box_item;
		}
	}
}

static void
free_action_listboxes(void)
{
	struct listbox_item *action;

	foreach (action, keybinding_browser.root.child) {
		struct listbox_item *keymap;

		foreach (keymap, action->child) {
			free_list(keymap->child);
		}
		free_list(action->child);
	}
	free_list(keybinding_browser.root.child);
}


void
toggle_display_action_listboxes(void)
{
	struct listbox_item *keymap;
	unsigned char *(*toggle)(struct strtonum *table, long num);
	static int state = 1;

	state = !state;
	toggle = state ? numtodesc : numtostr;

	foreach (keymap, keybinding_browser.root.child) {
		struct listbox_item *action;

		keymap->text = toggle(keymap_table, (int) keymap->udata);
		keymap->translated = state;

		foreach (action, keymap->child) {
			action->text = toggle(action_table[(int) keymap->udata],
						(int) action->udata);
			action->translated = state;
		}
	}
}


/*
 * Bind to Lua function.
 */

#ifdef HAVE_SCRIPTING
unsigned char *
bind_scripting_func(unsigned char *ckmap, unsigned char *ckey, int func_ref)
{
	unsigned char *err = NULL;
	long key, meta;
	int action;
	int kmap = read_keymap(ckmap);

	if (kmap < 0)
		err = gettext("Unrecognised keymap");
	else if (parse_keystroke(ckey, &key, &meta) < 0)
		err = gettext("Error parsing keystroke");
	else if ((action = read_action(kmap, " *scripting-function*")) < 0)
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
	long key;
	long meta;
	int action;
};

static struct default_kb default_main_keymap[] = {
	{ ' ',		 0,		ACT_MAIN_PAGE_DOWN },
	{ '#',		 0,		ACT_MAIN_SEARCH_TYPEAHEAD },
	{ '%',		 0,		ACT_MAIN_TOGGLE_DOCUMENT_COLORS },
	{ '*',		 0,		ACT_MAIN_TOGGLE_DISPLAY_IMAGES },
	{ ',',		 0,		ACT_MAIN_LUA_CONSOLE },
	{ '.',		 0,		ACT_MAIN_TOGGLE_NUMBERED_LINKS },
	{ '/',		 0,		ACT_MAIN_SEARCH },
	{ ':',		 0,		ACT_MAIN_EXMODE },
	{ '<',		 0,		ACT_MAIN_TAB_PREV },
	{ '=',		 0,		ACT_MAIN_DOCUMENT_INFO },
	{ '>',		 0,		ACT_MAIN_TAB_NEXT },
	{ '?',		 0,		ACT_MAIN_SEARCH_BACK },
	{ 'A',		 0,		ACT_MAIN_ADD_BOOKMARK_LINK },
	{ 'A',		 KBD_CTRL,	ACT_MAIN_HOME },
	{ 'B',		 KBD_CTRL,	ACT_MAIN_PAGE_UP },
	{ 'C',		 0,		ACT_MAIN_CACHE_MANAGER },
	{ 'D',		 0,		ACT_MAIN_DOWNLOAD_MANAGER },
	{ 'E',		 0,		ACT_MAIN_GOTO_URL_CURRENT_LINK },
	{ 'E',		 KBD_CTRL,	ACT_MAIN_END },
	{ 'F',		 KBD_CTRL,	ACT_MAIN_PAGE_DOWN },
	{ 'F',		 0,		ACT_MAIN_FORMHIST_MANAGER },
	{ 'G',		 0,		ACT_MAIN_GOTO_URL_CURRENT },
	{ 'H',		 0,		ACT_MAIN_GOTO_URL_HOME },
	{ 'K',		 0,		ACT_MAIN_COOKIE_MANAGER },
	{ 'K',		 KBD_CTRL,	ACT_MAIN_COOKIES_LOAD },
	{ 'L',		 0,		ACT_MAIN_LINK_MENU },
	{ 'L',		 KBD_CTRL,	ACT_MAIN_REDRAW },
	{ 'N',		 0,		ACT_MAIN_FIND_NEXT_BACK },
	{ 'N',		 KBD_CTRL,	ACT_MAIN_SCROLL_DOWN },
	{ 'P',		 KBD_CTRL,	ACT_MAIN_SCROLL_UP },
	{ 'Q',		 0,		ACT_MAIN_REALLY_QUIT },
	{ 'R',		 KBD_CTRL,	ACT_MAIN_RELOAD },
	{ 'T',		 0,		ACT_MAIN_OPEN_LINK_IN_NEW_TAB },
	{ 'W',		 0,		ACT_MAIN_TOGGLE_WRAP_TEXT },
	{ '[',		 0,		ACT_MAIN_SCROLL_LEFT },
	{ '\'',		 0,		ACT_MAIN_MARK_GOTO },
	{ '\\',		 0,		ACT_MAIN_TOGGLE_HTML_PLAIN },
	{ ']',		 0,		ACT_MAIN_SCROLL_RIGHT },
	{ 'a',		 0,		ACT_MAIN_ADD_BOOKMARK },
	{ 'b',		 0,		ACT_MAIN_PAGE_UP },
	{ 'c',		 0,		ACT_MAIN_TAB_CLOSE },
	{ 'd',		 0,		ACT_MAIN_DOWNLOAD },
	{ 'e',		 0,		ACT_MAIN_TAB_MENU },
	{ 'f',		 0,		ACT_MAIN_ZOOM_FRAME },
	{ 'g',		 0,		ACT_MAIN_GOTO_URL },
	{ 'h',		 0,		ACT_MAIN_HISTORY_MANAGER },
	{ 'k',		 0,		ACT_MAIN_KEYBINDING_MANAGER },
	{ 'l',		 0,		ACT_MAIN_JUMP_TO_LINK },
	{ 'm',		 0,		ACT_MAIN_MARK_SET },
	{ 'n',		 0,		ACT_MAIN_FIND_NEXT },
	{ 'o',		 0,		ACT_MAIN_OPTIONS_MANAGER },
	{ 'q',		 0,		ACT_MAIN_QUIT },
	{ 'r',		 0,		ACT_MAIN_RESUME_DOWNLOAD },
	{ 's',		 0,		ACT_MAIN_BOOKMARK_MANAGER },
	{ 't',		 0,		ACT_MAIN_OPEN_NEW_TAB },
	{ 'u',		 0,		ACT_MAIN_UNBACK },
	{ 'v',		 0,		ACT_MAIN_VIEW_IMAGE },
	{ 'w',		 0,		ACT_MAIN_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES },
	{ 'x',		 0,		ACT_MAIN_ENTER_RELOAD },
	{ 'z',		 0,		ACT_MAIN_ABORT_CONNECTION },
	{ '{',		 0,		ACT_MAIN_SCROLL_LEFT },
	{ '|',		 0,		ACT_MAIN_HEADER_INFO },
	{ '}',		 0,		ACT_MAIN_SCROLL_RIGHT },
	{ KBD_DEL,	 0,		ACT_MAIN_SCROLL_DOWN },
	{ KBD_DOWN,	 0,		ACT_MAIN_DOWN },
	{ KBD_END,	 0,		ACT_MAIN_END },
	{ KBD_ENTER,	 0,		ACT_MAIN_ENTER },
	{ KBD_ENTER,	 KBD_CTRL,	ACT_MAIN_ENTER_RELOAD },
	{ KBD_ESC,	 0,		ACT_MAIN_MENU },
	{ KBD_F10,	 0,		ACT_MAIN_FILE_MENU },
	{ KBD_F9,	 0,		ACT_MAIN_MENU },
	{ KBD_HOME,	 0,		ACT_MAIN_HOME },
	{ KBD_INS,	 0,		ACT_MAIN_SCROLL_UP },
	{ KBD_INS,	 KBD_CTRL,	ACT_MAIN_COPY_CLIPBOARD },
	{ KBD_LEFT,	 0,		ACT_MAIN_BACK },
	{ KBD_PAGE_DOWN, 0,		ACT_MAIN_PAGE_DOWN },
	{ KBD_PAGE_UP,	 0,		ACT_MAIN_PAGE_UP },
	{ KBD_RIGHT,	 0,		ACT_MAIN_ENTER },
	{ KBD_RIGHT,	 KBD_CTRL,	ACT_MAIN_ENTER_RELOAD },
	{ KBD_TAB,	 0,		ACT_MAIN_NEXT_FRAME },
	{ KBD_UP,	 0,		ACT_MAIN_UP },
	{ 0, 0, 0 }
};

static struct default_kb default_edit_keymap[] = {
	{ '<',		 KBD_ALT,	ACT_EDIT_BEGINNING_OF_BUFFER },
	{ '>',		 KBD_ALT,	ACT_EDIT_END_OF_BUFFER },
	{ 'A',		 KBD_CTRL,	ACT_EDIT_HOME },
	{ 'D',		 KBD_CTRL,	ACT_EDIT_DELETE },
	{ 'E',		 KBD_CTRL,	ACT_EDIT_END },
	{ 'H',		 KBD_CTRL,	ACT_EDIT_BACKSPACE },
	{ 'K',		 KBD_CTRL,	ACT_EDIT_KILL_TO_EOL },
	{ 'L',		 KBD_CTRL,	ACT_EDIT_REDRAW },
	{ 'R',		 KBD_CTRL,	ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS },
	{ 'T',		 KBD_CTRL,	ACT_EDIT_EDIT },
	{ 'U',		 KBD_CTRL,	ACT_EDIT_KILL_TO_BOL },
	{ 'V',		 KBD_CTRL,	ACT_EDIT_PASTE_CLIPBOARD },
	{ 'W',		 KBD_CTRL,	ACT_EDIT_AUTO_COMPLETE },
	{ 'X',		 KBD_CTRL,	ACT_EDIT_CUT_CLIPBOARD },
	{ KBD_BS,	 0,		ACT_EDIT_BACKSPACE },
	{ KBD_DEL,	 0,		ACT_EDIT_DELETE },
	{ KBD_DOWN,	 0,		ACT_EDIT_DOWN },
	{ KBD_END,	 0,		ACT_EDIT_END },
	{ KBD_ENTER,	 0,		ACT_EDIT_ENTER },
	{ KBD_ESC,	 0,		ACT_EDIT_CANCEL },
	{ KBD_F4,	 0,		ACT_EDIT_EDIT },
	{ KBD_HOME,	 0,		ACT_EDIT_HOME },
	{ KBD_INS,	 KBD_CTRL,	ACT_EDIT_COPY_CLIPBOARD },
	{ KBD_LEFT,	 0,		ACT_EDIT_LEFT },
	{ KBD_RIGHT,	 0,		ACT_EDIT_RIGHT },
	{ KBD_TAB,	 0,		ACT_EDIT_NEXT_ITEM },
	{ KBD_UP,	 0,		ACT_EDIT_UP },
	{ 0, 0, 0 }
};

static struct default_kb default_menu_keymap[] = {
	{ ' ',		 0,		ACT_MENU_SELECT },
	{ '*',		 0,		ACT_MENU_MARK_ITEM },
	{ '+',		 0,		ACT_MENU_EXPAND },
	{ '-',		 0,		ACT_MENU_UNEXPAND },
	{ '=',		 0,		ACT_MENU_EXPAND },
	{ 'A',		 KBD_CTRL,	ACT_MENU_HOME },
	{ 'B',		 KBD_CTRL,	ACT_MENU_PAGE_UP },
	{ 'E',		 KBD_CTRL,	ACT_MENU_END },
	{ 'F',		 KBD_CTRL,	ACT_MENU_PAGE_DOWN },
	{ 'L',		 KBD_CTRL,	ACT_MENU_REDRAW },
	{ 'N',		 KBD_CTRL,	ACT_MENU_DOWN },
	{ 'P',		 KBD_CTRL,	ACT_MENU_UP },
	{ 'V',		 KBD_ALT,	ACT_MENU_PAGE_UP },
	{ 'V',		 KBD_CTRL,	ACT_MENU_PAGE_DOWN },
	{ '[',		 0,		ACT_MENU_EXPAND },
	{ ']',		 0,		ACT_MENU_UNEXPAND },
	{ '_',		 0,		ACT_MENU_UNEXPAND },
	{ KBD_DEL,	 0,		ACT_MENU_DELETE },
	{ KBD_DOWN,	 0,		ACT_MENU_DOWN },
	{ KBD_END,	 0,		ACT_MENU_END },
	{ KBD_ENTER,	 0,		ACT_MENU_ENTER },
	{ KBD_ESC,	 0,		ACT_MENU_CANCEL },
	{ KBD_HOME,	 0,		ACT_MENU_HOME },
	{ KBD_INS,	 0,		ACT_MENU_MARK_ITEM },
	{ KBD_LEFT,	 0,		ACT_MENU_LEFT },
	{ KBD_PAGE_DOWN, 0,		ACT_MENU_PAGE_DOWN },
	{ KBD_PAGE_UP,	 0,		ACT_MENU_PAGE_UP },
	{ KBD_RIGHT,	 0,		ACT_MENU_RIGHT },
	{ KBD_TAB,	 0,		ACT_MENU_NEXT_ITEM },
	{ KBD_UP,	 0,		ACT_MENU_UP },
	{ 0, 0, 0}
};

static inline void
add_keymap_default_keybindings(enum keymap keymap, struct default_kb *defaults)
{
	struct default_kb *kb;

	for (kb = defaults; kb->key; kb++) {
		struct keybinding *keybinding;

		keybinding = add_keybinding(keymap, kb->action,
					    kb->key, kb->meta, EVENT_NONE);
		keybinding->flags |= KBDB_DEFAULT;
	}
}

static void
add_default_keybindings(void)
{
	/* Maybe we shouldn't delete old keybindings. But on the other side, we
	 * can't trust clueless users what they'll push into sources modifying
	 * defaults, can we? ;)) */

	add_keymap_default_keybindings(KM_MAIN, default_main_keymap);
	add_keymap_default_keybindings(KM_EDIT, default_edit_keymap);
	add_keymap_default_keybindings(KM_MENU, default_menu_keymap);
}


/*
 * Config file tools.
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

	action_ = read_action(keymap_, action);
	if (action_ < 0) return 77 / 9 - 5;

	add_keybinding(keymap_, action_, key_, meta_, EVENT_NONE);
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

	action = write_action(keymap_, kb->action);
	if (!action)
		return NULL;

	kb->flags |= KBDB_WATERMARK;
	return straconcat("\"", action, "\"", NULL);
}

static void
single_bind_config_string(struct string *file, enum keymap keymap,
			  struct keybinding *keybinding)
{
	unsigned char *keymap_str = write_keymap(keymap);
	unsigned char *action_str = write_action(keymap, keybinding->action);

	if (!keymap_str || !action_str || action_str[0] == ' ')
		return;

	if (keybinding->flags & KBDB_WATERMARK) {
		keybinding->flags &= ~KBDB_WATERMARK;
		return;
	}

	/* TODO: Maybe we should use string.write.. */
	add_to_string(file, "bind \"");
	add_to_string(file, keymap_str);
	add_to_string(file, "\" \"");
	make_keystroke(file, keybinding->key, keybinding->meta, 1);
	add_to_string(file, "\" = \"");
	add_to_string(file, action_str);
	add_char_to_string(file, '\"');
	add_char_to_string(file, '\n');
}

void
bind_config_string(struct string *file)
{
	enum keymap keymap;

	for (keymap = 0; keymap < KM_MAX; keymap++) {
		struct keybinding *keybinding;

		foreach (keybinding, keymaps[keymap]) {
			single_bind_config_string(file, keymap, keybinding);
		}
	}
}
