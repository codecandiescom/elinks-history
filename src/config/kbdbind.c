/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.152 2004/01/09 00:14:28 jonas Exp $ */

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

static struct listbox_item *keyact_box_items[KEYACTS];
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
		struct string keystroke;

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

		if (!init_string(&keystroke)) return;

		make_keystroke(&keystroke, key, meta, 0);
		kb->box_item = mem_calloc(1, sizeof(struct listbox_item)
					  + keystroke.length + 1);
		if (!kb->box_item) {
			done_string(&keystroke);
			return; /* Or just goto after end of this if block. */
		}
		kb->box_item->text = ((unsigned char *) kb->box_item
					+ sizeof(struct listbox_item));
		strcpy(kb->box_item->text, keystroke.source);
		done_string(&keystroke);

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

		update_hierbox_browser(&keybinding_browser);
	}
}

void
free_keybinding(struct keybinding *kb)
{
	if (kb->box_item) done_listbox_item(&keybinding_browser, kb->box_item);
#ifdef HAVE_SCRIPTING
/* TODO: unref function must be implemented. */
/*	if (kb->func_ref != EVENT_NONE)
		scripting_unref(kb->func_ref); */
#endif
	del_from_list(kb);
	mem_free(kb);
}

int
keybinding_exists(enum keymap km, long key, long meta, enum keyact *action)
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

		if (kb->action == ACT_SCRIPTING_FUNCTION && func_ref)
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

		if (kb->action == ACT_SCRIPTING_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_act_lookup(enum keymap map, enum keyact action)
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
		if (key == '\\' && escape) key_string--;
	}

	add_to_string(str, key_string);
}

#ifndef ELINKS_SMALL
#define DACT(x) (x)
#else
#define DACT(x) (NULL)
#endif

/* Please keep this table in alphabetical order, and in sync with
 * the ACT_* constants in kbdbind.h.  */
static struct strtonum action_table[KEYACTS + 1] = {
	{ "abort-connection", ACT_ABORT_CONNECTION, DACT(N_("Abort connection")) },
	{ "add-bookmark", ACT_ADD_BOOKMARK, DACT(N_("Add a new bookmark")) },
	{ "add-bookmark-link", ACT_ADD_BOOKMARK_LINK, DACT(N_("Add a new bookmark using current link")) },
	{ "add-bookmark-tabs", ACT_ADD_BOOKMARK_TABS, DACT(N_("Bookmark all open tabs")) },
	{ "auto-complete", ACT_AUTO_COMPLETE, DACT(N_("Attempt to auto-complete the input")) },
	{ "auto-complete-unambiguous", ACT_AUTO_COMPLETE_UNAMBIGUOUS, DACT(N_("Attempt to unambiguously auto-complete the input")) },
	{ "back", ACT_BACK, DACT(N_("Return to the previous document in history")) },
	{ "backspace", ACT_BACKSPACE, DACT(N_("Delete character in front of the cursor")) },
	{ "beginning-of-buffer", ACT_BEGINNING_OF_BUFFER, DACT(N_("Go to the first line of the buffer.")) },
	{ "bookmark-manager", ACT_BOOKMARK_MANAGER, DACT(N_("Open bookmark manager")) },
	{ "cache-manager", ACT_CACHE_MANAGER, DACT(N_("Open cache manager")) },
	{ "cache-minimize", ACT_CACHE_MINIMIZE, DACT(N_("Free unused cache entries")) },
	{ "cancel", ACT_CANCEL, DACT(N_("Cancel current state")) },
	{ "cookie-manager", ACT_COOKIE_MANAGER, DACT(N_("Open cookie manager")) },
	{ "cookies-load", ACT_COOKIES_LOAD, DACT(N_("Reload cookies file")) },
	{ "copy-clipboard", ACT_COPY_CLIPBOARD, DACT(N_("Copy text to clipboard")) },
	{ "cut-clipboard", ACT_CUT_CLIPBOARD, DACT(N_("Delete text from clipboard")) },
	{ "delete", ACT_DELETE, DACT(N_("Delete character under cursor")) },
	{ "document-info", ACT_DOCUMENT_INFO, DACT(N_("Show information about the current page")) },
	{ "down", ACT_DOWN, DACT(N_("Move cursor downwards")) },
	{ "download", ACT_DOWNLOAD, DACT(N_("Download the current link")) },
	{ "download-image", ACT_DOWNLOAD_IMAGE, DACT(N_("Download the current image")) },
	{ "download-manager", ACT_DOWNLOAD_MANAGER, DACT(N_("Open download manager")) },
	{ "edit", ACT_EDIT, DACT(N_("Begin editing")) }, /* FIXME */
	{ "end", ACT_END, DACT(N_("Go to the end of the page/line")) },
	{ "end-of-buffer", ACT_END_OF_BUFFER, DACT(N_("Go to the last line of the buffer.")) },
	{ "enter", ACT_ENTER, DACT(N_("Follow the current link")) },
	{ "enter-reload", ACT_ENTER_RELOAD, DACT(N_("Follow the current link, forcing reload of the target")) },
	{ "expand", ACT_EXPAND, DACT(N_("Expand item")) },
	{ "file-menu", ACT_FILE_MENU, DACT(N_("Open the File menu")) },
	{ "find-next", ACT_FIND_NEXT, DACT(N_("Find the next occurrence of the current search text")) },
	{ "find-next-back", ACT_FIND_NEXT_BACK, DACT(N_("Find the previous occurrence of the current search text")) },
	{ "forget-credentials", ACT_FORGET_CREDENTIALS, DACT(N_("Forget authentication credentials")) },
	{ "formhist-manager", ACT_FORMHIST_MANAGER, DACT(N_("Open form history manager")) },
	{ "goto-url", ACT_GOTO_URL, DACT(N_("Open \"Go to URL\" dialog box")) },
	{ "goto-url-current", ACT_GOTO_URL_CURRENT, DACT(N_("Open \"Go to URL\" dialog box containing the current URL")) },
	{ "goto-url-current-link", ACT_GOTO_URL_CURRENT_LINK, DACT(N_("Open \"Go to URL\" dialog box containing the current link URL")) },
	{ "goto-url-home", ACT_GOTO_URL_HOME, DACT(N_("Go to the homepage")) },
	{ "header-info", ACT_HEADER_INFO, DACT(N_("Show information about the current page HTTP headers")) },
	{ "history-manager", ACT_HISTORY_MANAGER, DACT(N_("Open history manager")) },
	{ "home", ACT_HOME, DACT(N_("Go to the start of the page/line")) },
	{ "jump-to-link", ACT_JUMP_TO_LINK, DACT(N_("Jump to link")) },
	{ "keybinding-manager", ACT_KEYBINDING_MANAGER, DACT(N_("Open keybinding manager")) },
	{ "kill-backgrounded-connections", ACT_KILL_BACKGROUNDED_CONNECTIONS, DACT(N_("Kill all backgrounded connections")) },
	{ "kill-to-bol", ACT_KILL_TO_BOL, DACT(N_("Delete to beginning of line")) },
	{ "kill-to-eol", ACT_KILL_TO_EOL, DACT(N_("Delete to end of line")) },
	{ "left", ACT_LEFT,DACT( N_("Move the cursor left")) },
	{ "link-menu", ACT_LINK_MENU, DACT(N_("Open the link context menu")) },
	{ "none", ACT_NONE, NULL },
#ifdef HAVE_LUA
	{ "lua-console", ACT_LUA_CONSOLE, DACT(N_("Open a Lua console")) },
#else
	{ "lua-console", ACT_LUA_CONSOLE, DACT(N_("Open a Lua console (DISABLED)")) },
#endif
	{ "mark-goto", ACT_MARK_GOTO, DACT(N_("Go at a specified mark")) },
	{ "mark-item", ACT_MARK_ITEM, DACT(N_("Mark item")) },
	{ "mark-set", ACT_MARK_SET, DACT(N_("Set a mark")) },
	{ "menu", ACT_MENU, DACT(N_("Activate the menu")) },
	{ "next-frame", ACT_NEXT_FRAME, DACT(N_("Move to the next frame")) },
	{ "next-item", ACT_NEXT_ITEM, DACT(N_("Move to the next item")) },
	{ "open-link-in-new-tab", ACT_OPEN_LINK_IN_NEW_TAB, DACT(N_("Open the current link in a new tab")) },
	{ "open-link-in-new-tab-in-background", ACT_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open the current link a new tab in background")) },
	{ "open-link-in-new-window", ACT_OPEN_LINK_IN_NEW_WINDOW, DACT(N_("Open the current link in a new window")) },
	{ "open-new-tab", ACT_OPEN_NEW_TAB, DACT(N_("Open a new tab")) },
	{ "open-new-tab-in-background", ACT_OPEN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open a new tab in background")) },
	{ "open-new-window", ACT_OPEN_NEW_WINDOW, DACT(N_("Open a new window")) },
	{ "open-os-shell", ACT_OPEN_OS_SHELL, DACT(N_("Open an OS shell")) },
	{ "options-manager", ACT_OPTIONS_MANAGER, DACT(N_("Open options manager")) },
	{ "page-down", ACT_PAGE_DOWN, DACT(N_("Move downwards by a page")) },
	{ "page-up", ACT_PAGE_UP, DACT(N_("Move upwards by a page")) },
	{ "paste-clipboard", ACT_PASTE_CLIPBOARD, DACT(N_("Paste text from the clipboard")) },
	{ "previous-frame", ACT_PREVIOUS_FRAME, DACT(N_("Move to the previous frame")) },
	{ "quit", ACT_QUIT, DACT(N_("Open a quit confirmation dialog box")) },
	{ "really-quit", ACT_REALLY_QUIT, DACT(N_("Quit without confirmation")) },
	{ "redraw", ACT_REDRAW, DACT(N_("Redraw the terminal")) },
	{ "reload", ACT_RELOAD, DACT(N_("Reload the current page")) },
	{ "reset-form", ACT_RESET_FORM, DACT(N_("Reset form items to their initial values")) },
	{ "resource-info", ACT_RESOURCE_INFO, DACT(N_("Show information about the currently used resources")) },
	{ "resume-download", ACT_RESUME_DOWNLOAD, DACT(N_("Attempt to resume download of the current link")) },
	{ "right", ACT_RIGHT, DACT(N_("Move the cursor right")) },
	{ "save-as", ACT_SAVE_AS, DACT(N_("Save as")) },
	{ "save-formatted", ACT_SAVE_FORMATTED, DACT(N_("Save formatted document")) },
	{ "save-options", ACT_SAVE_OPTIONS, DACT(N_("Save options")), },
	{ "save-url-as", ACT_SAVE_URL_AS, DACT(N_("Save URL as")) },
	{ " *scripting-function*", ACT_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "scroll-down", ACT_SCROLL_DOWN, DACT(N_("Scroll down")) },
	{ "scroll-left", ACT_SCROLL_LEFT, DACT(N_("Scroll left")) },
	{ "scroll-right", ACT_SCROLL_RIGHT, DACT(N_("Scroll right")) },
	{ "scroll-up", ACT_SCROLL_UP, DACT(N_("Scroll up")) },
	{ "search", ACT_SEARCH, DACT(N_("Search for a text pattern")) },
	{ "search-back", ACT_SEARCH_BACK, DACT(N_("Search backwards for a text pattern")) },
	{ "search-typeahead", ACT_SEARCH_TYPEAHEAD, DACT(N_("Search link text by typing ahead")) },
	{ "select", ACT_SELECT, DACT(N_("Select current highlighted item")) },
	{ "show-term-options", ACT_SHOW_TERM_OPTIONS, DACT(N_("Show terminal options dialog")) },
	{ "submit-form", ACT_SUBMIT_FORM, DACT(N_("Submit form")) },
	{ "submit-form-reload", ACT_SUBMIT_FORM_RELOAD, DACT(N_("Submit form and reload")) },
	{ "tab-close", ACT_TAB_CLOSE, DACT(N_("Close tab")) },
	{ "tab-close-all-but-current", ACT_TAB_CLOSE_ALL_BUT_CURRENT, DACT(N_("Close all tabs but the current one")) },
	{ "tab-menu", ACT_TAB_MENU, DACT(N_("Open the tab menu")) },
	{ "tab-next", ACT_TAB_NEXT, DACT(N_("Next tab")) },
	{ "tab-prev", ACT_TAB_PREV,DACT( N_("Previous tab")) },
	{ "toggle-display-images", ACT_TOGGLE_DISPLAY_IMAGES, DACT(N_("Toggle displaying of links to images")) },
	{ "toggle-display-tables", ACT_TOGGLE_DISPLAY_TABLES, DACT(N_("Toggle rendering of tables")) },
	{ "toggle-document-colors", ACT_TOGGLE_DOCUMENT_COLORS, DACT(N_("Toggle usage of document specific colors")) },
	{ "toggle-html-plain", ACT_TOGGLE_HTML_PLAIN, DACT(N_("Toggle rendering page as HTML / plain text")) },
	{ "toggle-numbered-links", ACT_TOGGLE_NUMBERED_LINKS, DACT(N_("Toggle displaying of links numbers")) },
	{ "toggle-plain-compress-empty-lines", ACT_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES, DACT(N_("Toggle plain renderer compression of empty lines")) },
	{ "unback", ACT_UNBACK, DACT(N_("Go forward in the unhistory")) },
	{ "unexpand", ACT_UNEXPAND, DACT(N_("Collapse item")) },
	{ "up", ACT_UP, DACT(N_("Move cursor upwards")) },
	{ "view-image", ACT_VIEW_IMAGE, DACT(N_("View the current image")) },
	{ "zoom-frame", ACT_ZOOM_FRAME, DACT(N_("Maximize the current frame")) },

	{ NULL, 0, NULL }
};

#undef DACT

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
		add_to_list_end(keybinding_browser.root.child, box_item);
		box_item->root = NULL;
		init_list(box_item->child);
		box_item->visible = (act->num != ACT_SCRIPTING_FUNCTION); /* XXX */
		box_item->translated = 1;
		box_item->udata = (void *) act->num;
		box_item->type = BI_FOLDER;
		box_item->expanded = 0; /* Maybe you would like this being 1? */
		box_item->depth = 0;
		box_item->text = act->desc ? act->desc : act->str;

		for (i = 0; i < KM_MAX; i++) {
			struct listbox_item *keymap;

			keymap = mem_calloc(1, sizeof(struct listbox_item));
			if (!keymap) continue;
			add_to_list_end(box_item->child, keymap);
			keymap->root = box_item;
			init_list(keymap->child);
			keymap->visible = 1;
			keymap->translated = 1;
			keymap->udata = (void *) i;
			keymap->type = BI_FOLDER;
			keymap->expanded = 1;
			keymap->depth = 1;
			keymap->text = numtodesc(keymap_table, i);
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
	struct listbox_item *action;
	unsigned char *(*toggle)(struct strtonum *table, long num);
	static int state = 1;

	state = !state;
	toggle = state ? numtodesc : numtostr;

	foreach (action, keybinding_browser.root.child) {
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

	action = write_action(kb->action);
	if (!action)
		return NULL;

	kb->flags |= KBDB_WATERMARK;
	return straconcat("\"", action, "\"", NULL);
}

void
bind_config_string(struct string *file)
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
			add_to_string(file, "bind \"");
			add_to_string(file, keymap_str);
			add_to_string(file, "\" \"");
			make_keystroke(file, keybinding->key, keybinding->meta, 1);
			add_to_string(file, "\" = \"");
			add_to_string(file, action_str);
			add_char_to_string(file, '\"');
			add_char_to_string(file, '\n');
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
	else if ((action = read_action(" *scripting-function*")) < 0)
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
	{ ' ',		 0,		ACT_PAGE_DOWN },
	{ '#',		 0,		ACT_SEARCH_TYPEAHEAD },
	{ '%',		 0,		ACT_TOGGLE_DOCUMENT_COLORS },
	{ '*',		 0,		ACT_TOGGLE_DISPLAY_IMAGES },
	{ ',',		 0,		ACT_LUA_CONSOLE },
	{ '.',		 0,		ACT_TOGGLE_NUMBERED_LINKS },
	{ '/',		 0,		ACT_SEARCH },
	{ '<',		 0,		ACT_TAB_PREV },
	{ '=',		 0,		ACT_DOCUMENT_INFO },
	{ '>',		 0,		ACT_TAB_NEXT },
	{ '?',		 0,		ACT_SEARCH_BACK },
	{ 'A',		 0,		ACT_ADD_BOOKMARK_LINK },
	{ 'A',		 KBD_CTRL,	ACT_HOME },
	{ 'B',		 KBD_CTRL,	ACT_PAGE_UP },
	{ 'C',		 0,		ACT_CACHE_MANAGER },
	{ 'D',		 0,		ACT_DOWNLOAD_MANAGER },
	{ 'E',		 0,		ACT_GOTO_URL_CURRENT_LINK },
	{ 'E',		 KBD_CTRL,	ACT_END },
	{ 'F',		 KBD_CTRL,	ACT_PAGE_DOWN },
	{ 'F',		 0,		ACT_FORMHIST_MANAGER },
	{ 'G',		 0,		ACT_GOTO_URL_CURRENT },
	{ 'H',		 0,		ACT_GOTO_URL_HOME },
	{ 'K',		 0,		ACT_COOKIE_MANAGER },
	{ 'K',		 KBD_CTRL,	ACT_COOKIES_LOAD },
	{ 'L',		 0,		ACT_LINK_MENU },
	{ 'L',		 KBD_CTRL,	ACT_REDRAW },
	{ 'N',		 0,		ACT_FIND_NEXT_BACK },
	{ 'N',		 KBD_CTRL,	ACT_SCROLL_DOWN },
	{ 'P',		 KBD_CTRL,	ACT_SCROLL_UP },
	{ 'Q',		 0,		ACT_REALLY_QUIT },
	{ 'R',		 KBD_CTRL,	ACT_RELOAD },
	{ 'T',		 0,		ACT_OPEN_LINK_IN_NEW_TAB },
	{ '[',		 0,		ACT_SCROLL_LEFT },
	{ '\'',		 0,		ACT_MARK_GOTO },
	{ '\\',		 0,		ACT_TOGGLE_HTML_PLAIN },
	{ ']',		 0,		ACT_SCROLL_RIGHT },
	{ 'a',		 0,		ACT_ADD_BOOKMARK },
	{ 'b',		 0,		ACT_PAGE_UP },
	{ 'c',		 0,		ACT_TAB_CLOSE },
	{ 'd',		 0,		ACT_DOWNLOAD },
	{ 'e',		 0,		ACT_TAB_MENU },
	{ 'f',		 0,		ACT_ZOOM_FRAME },
	{ 'g',		 0,		ACT_GOTO_URL },
	{ 'h',		 0,		ACT_HISTORY_MANAGER },
	{ 'k',		 0,		ACT_KEYBINDING_MANAGER },
	{ 'l',		 0,		ACT_JUMP_TO_LINK },
	{ 'm',		 0,		ACT_MARK_SET },
	{ 'n',		 0,		ACT_FIND_NEXT },
	{ 'o',		 0,		ACT_OPTIONS_MANAGER },
	{ 'q',		 0,		ACT_QUIT },
	{ 'r',		 0,		ACT_RESUME_DOWNLOAD },
	{ 's',		 0,		ACT_BOOKMARK_MANAGER },
	{ 't',		 0,		ACT_OPEN_NEW_TAB },
	{ 'u',		 0,		ACT_UNBACK },
	{ 'v',		 0,		ACT_VIEW_IMAGE },
	{ 'w',		 0,		ACT_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES },
	{ 'x',		 0,		ACT_ENTER_RELOAD },
	{ 'z',		 0,		ACT_ABORT_CONNECTION },
	{ '{',		 0,		ACT_SCROLL_LEFT },
	{ '|',		 0,		ACT_HEADER_INFO },
	{ '}',		 0,		ACT_SCROLL_RIGHT },
	{ KBD_DEL,	 0,		ACT_SCROLL_DOWN },
	{ KBD_DOWN,	 0,		ACT_DOWN },
	{ KBD_END,	 0,		ACT_END },
	{ KBD_ENTER,	 0,		ACT_ENTER },
	{ KBD_ENTER,	 KBD_CTRL,	ACT_ENTER_RELOAD },
	{ KBD_ESC,	 0,		ACT_MENU },
	{ KBD_F10,	 0,		ACT_FILE_MENU },
	{ KBD_F9,	 0,		ACT_MENU },
	{ KBD_HOME,	 0,		ACT_HOME },
	{ KBD_INS,	 0,		ACT_SCROLL_UP },
	{ KBD_INS,	 KBD_CTRL,	ACT_COPY_CLIPBOARD },
	{ KBD_LEFT,	 0,		ACT_BACK },
	{ KBD_PAGE_DOWN, 0,		ACT_PAGE_DOWN },
	{ KBD_PAGE_UP,	 0,		ACT_PAGE_UP },
	{ KBD_RIGHT,	 0,		ACT_ENTER },
	{ KBD_RIGHT,	 KBD_CTRL,	ACT_ENTER_RELOAD },
	{ KBD_TAB,	 0,		ACT_NEXT_FRAME },
	{ KBD_UP,	 0,		ACT_UP },
	{ 0, 0, 0 }
};

static struct default_kb default_edit_keymap[] = {
	{ '<',		 KBD_ALT,	ACT_BEGINNING_OF_BUFFER },
	{ '>',		 KBD_ALT,	ACT_END_OF_BUFFER },
	{ 'A',		 KBD_CTRL,	ACT_HOME },
	{ 'D',		 KBD_CTRL,	ACT_DELETE },
	{ 'E',		 KBD_CTRL,	ACT_END },
	{ 'H',		 KBD_CTRL,	ACT_BACKSPACE },
	{ 'K',		 KBD_CTRL,	ACT_KILL_TO_EOL },
	{ 'L',		 KBD_CTRL,	ACT_REDRAW },
	{ 'R',		 KBD_CTRL,	ACT_AUTO_COMPLETE_UNAMBIGUOUS },
	{ 'T',		 KBD_CTRL,	ACT_EDIT },
	{ 'U',		 KBD_CTRL,	ACT_KILL_TO_BOL },
	{ 'V',		 KBD_CTRL,	ACT_PASTE_CLIPBOARD },
	{ 'W',		 KBD_CTRL,	ACT_AUTO_COMPLETE },
	{ 'X',		 KBD_CTRL,	ACT_CUT_CLIPBOARD },
	{ KBD_BS,	 0,		ACT_BACKSPACE },
	{ KBD_DEL,	 0,		ACT_DELETE },
	{ KBD_DOWN,	 0,		ACT_DOWN },
	{ KBD_END,	 0,		ACT_END },
	{ KBD_ENTER,	 0,		ACT_ENTER },
	{ KBD_ESC,	 0,		ACT_CANCEL },
	{ KBD_F4,	 0,		ACT_EDIT },
	{ KBD_HOME,	 0,		ACT_HOME },
	{ KBD_INS,	 KBD_CTRL,	ACT_COPY_CLIPBOARD },
	{ KBD_LEFT,	 0,		ACT_LEFT },
	{ KBD_RIGHT,	 0,		ACT_RIGHT },
	{ KBD_TAB,	 0,		ACT_NEXT_ITEM },
	{ KBD_UP,	 0,		ACT_UP },
	{ 0, 0, 0 }
};

static struct default_kb default_menu_keymap[] = {
	{ ' ',		 0,		ACT_SELECT },
	{ '*',		 0,		ACT_MARK_ITEM },
	{ '+',		 0,		ACT_EXPAND },
	{ '-',		 0,		ACT_UNEXPAND },
	{ '=',		 0,		ACT_EXPAND },
	{ 'A',		 KBD_CTRL,	ACT_HOME },
	{ 'B',		 KBD_CTRL,	ACT_PAGE_UP },
	{ 'E',		 KBD_CTRL,	ACT_END },
	{ 'F',		 KBD_CTRL,	ACT_PAGE_DOWN },
	{ 'L',		 KBD_CTRL,	ACT_REDRAW },
	{ 'N',		 KBD_CTRL,	ACT_DOWN },
	{ 'P',		 KBD_CTRL,	ACT_UP },
	{ 'V',		 KBD_ALT,	ACT_PAGE_UP },
	{ 'V',		 KBD_CTRL,	ACT_PAGE_DOWN },
	{ '[',		 0,		ACT_EXPAND },
	{ ']',		 0,		ACT_UNEXPAND },
	{ '_',		 0,		ACT_UNEXPAND },
	{ KBD_DEL,	 0,		ACT_DELETE },
	{ KBD_DOWN,	 0,		ACT_DOWN },
	{ KBD_END,	 0,		ACT_END },
	{ KBD_ENTER,	 0,		ACT_ENTER },
	{ KBD_ESC,	 0,		ACT_CANCEL },
	{ KBD_HOME,	 0,		ACT_HOME },
	{ KBD_INS,	 0,		ACT_MARK_ITEM },
	{ KBD_LEFT,	 0,		ACT_LEFT },
	{ KBD_PAGE_DOWN, 0,		ACT_PAGE_DOWN },
	{ KBD_PAGE_UP,	 0,		ACT_PAGE_UP },
	{ KBD_RIGHT,	 0,		ACT_RIGHT },
	{ KBD_TAB,	 0,		ACT_NEXT_ITEM },
	{ KBD_UP,	 0,		ACT_UP },
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
		add_keybinding(KM_MAIN, kb->action, kb->key, kb->meta, EVENT_NONE);

	for (kb = default_edit_keymap; kb->key; kb++)
		add_keybinding(KM_EDIT, kb->action, kb->key, kb->meta, EVENT_NONE);

	for (kb = default_menu_keymap; kb->key; kb++)
		add_keybinding(KM_MENU, kb->action, kb->key, kb->meta, EVENT_NONE);
}

void
add_keystroke_to_string(struct string *string, enum keyact action,
			enum keymap map)
{
	struct keybinding *kb = kbd_act_lookup(map, action);

	if (kb)
		make_keystroke(string, kb->key, kb->meta, 0);
}

void
add_keyactions_to_string(struct string *string, enum keyact *actions,
			 enum keymap map, struct terminal *term)
{
	int i;

	add_format_to_string(string, "%s:\n", _(numtodesc(keymap_table, map), term));

	for (i = 0; actions[i] != ACT_NONE; i++) {
		struct keybinding *kb = kbd_act_lookup(map, actions[i]);
		int keystrokelen = string->length;
		unsigned char *desc = numtodesc(action_table, actions[i]);

		if (!kb) continue;

		add_char_to_string(string, '\n');
		make_keystroke(string, kb->key, kb->meta, 0);
		keystrokelen = string->length - keystrokelen;
		add_xchar_to_string(string, ' ', int_max(15 - keystrokelen, 1));
		add_to_string(string, _(desc, term));
	}
}
